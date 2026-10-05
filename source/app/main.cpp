#include "rdrand_prng.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_handle = SOCKET;
constexpr socket_handle invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_handle = int;
constexpr socket_handle invalid_socket = -1;
#endif

namespace {

constexpr std::uint16_t default_port = 8787;
constexpr std::size_t max_request_bytes = 16U * 1024U;
constexpr std::uint64_t max_generated_values = 1000;

void close_socket(socket_handle socket) noexcept {
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

class SocketRuntime {
public:
    SocketRuntime() {
#if defined(_WIN32)
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }
    ~SocketRuntime() {
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};

class SocketGuard {
public:
    explicit SocketGuard(socket_handle socket = invalid_socket) : socket_(socket) {}
    ~SocketGuard() { if (socket_ != invalid_socket) close_socket(socket_); }
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    [[nodiscard]] socket_handle get() const noexcept { return socket_; }
private:
    socket_handle socket_;
};

std::string json_escape(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (const unsigned char character : input) {
        switch (character) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character >= 0x20U) result += static_cast<char>(character);
        }
    }
    return result;
}

std::string response(int status, std::string_view status_text,
                     std::string_view content_type, const std::string& body) {
    std::ostringstream output;
    output << "HTTP/1.1 " << status << ' ' << status_text << "\r\n"
           << "Content-Type: " << content_type << "\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Cache-Control: no-store\r\n"
           << "X-Content-Type-Options: nosniff\r\n"
           << "Content-Security-Policy: default-src 'self'; "
              "script-src 'self'; style-src 'self'; connect-src 'self'\r\n"
           << "Connection: close\r\n\r\n"
           << body;
    return output.str();
}

bool send_all(socket_handle socket, const std::string& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
#if defined(_WIN32)
        const int count = send(socket, data.data() + offset,
                               static_cast<int>(data.size() - offset), 0);
#else
        const auto count = send(socket, data.data() + offset, data.size() - offset, 0);
#endif
        if (count <= 0) return false;
        offset += static_cast<std::size_t>(count);
    }
    return true;
}

std::string receive_request(socket_handle socket) {
    std::string request;
    request.reserve(2048);
    char buffer[2048];
    while (request.size() < max_request_bytes) {
#if defined(_WIN32)
        const int count = recv(socket, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        const auto count = recv(socket, buffer, sizeof(buffer), 0);
#endif
        if (count <= 0) break;
        request.append(buffer, static_cast<std::size_t>(count));
        if (request.find("\r\n\r\n") != std::string::npos) break;
    }
    return request;
}

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string_view request_path(std::string_view target) noexcept {
    const auto query = target.find('?');
    if (query == std::string_view::npos) {
        return target;
    }
    return target.substr(0, query);
}

std::uint64_t query_integer(std::string_view target, std::string_view name,
                            std::uint64_t fallback) {
    const std::string marker = std::string(name) + '=';
    const auto query = target.find('?');
    if (query == std::string_view::npos) return fallback;
    const auto position = target.find(marker, query + 1);
    if (position == std::string_view::npos) return fallback;
    const auto start = position + marker.size();
    const auto end = target.find('&', start);
    const auto value = target.substr(start, end == std::string_view::npos
                                               ? target.size() - start : end - start);
    if (value.empty() || !std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; })) return fallback;
    try {
        return std::stoull(std::string(value));
    } catch (...) {
        return fallback;
    }
}

class RandomService {
public:
    RandomService() { initialize(); }

    std::string status() const {
        std::ostringstream body;
        body << "{\"rdrandSupported\":"
             << (intel_rng::RdrandEntropy::is_supported() ? "true" : "false")
             << ",\"ready\":" << (prng_ ? "true" : "false")
             << ",\"generation\":" << generation_
             << ",\"valuesGenerated\":" << values_generated_
             << ",\"error\":\"" << json_escape(last_error_) << "\"}";
        return body.str();
    }

    std::string reseed() {
        initialize();
        return status();
    }

    std::string generate(std::uint64_t count, std::uint64_t bound) {
        if (!prng_) {
            return "{\"error\":\"PRNG is not initialized; check hardware status\"}";
        }
        count = std::clamp<std::uint64_t>(count, 1, max_generated_values);

        std::ostringstream body;
        body << "{\"values\":[";
        for (std::uint64_t i = 0; i < count; ++i) {
            if (i != 0) body << ',';
            const auto value = bound > 0 ? prng_->bounded(bound) : prng_->next_u64();
            body << '"' << value << '"';
        }
        values_generated_ += count;
        body << "],\"bound\":" << bound
             << ",\"generation\":" << generation_
             << ",\"valuesGenerated\":" << values_generated_ << '}';
        return body.str();
    }

private:
    void initialize() {
        try {
            intel_rng::RdrandEntropy entropy;
            if (prng_) prng_->reseed(entropy);
            else prng_ = std::make_unique<intel_rng::Prng>(entropy);
            ++generation_;
            last_error_.clear();
        } catch (const std::exception& error) {
            prng_.reset();
            last_error_ = error.what();
        }
    }

    std::unique_ptr<intel_rng::Prng> prng_;
    std::uint64_t generation_ = 0;
    std::uint64_t values_generated_ = 0;
    std::string last_error_;
};

std::string handle_request(const std::string& request, RandomService& random) {
    std::istringstream input(request);
    std::string method;
    std::string target;
    std::string version;
    input >> method >> target >> version;
    if (method.empty() || target.empty()) {
        return response(400, "Bad Request", "application/json",
                        "{\"error\":\"Malformed request\"}");
    }

    const auto path = request_path(target);

    if (method == "GET" && path == "/api/status") {
        return response(200, "OK", "application/json", random.status());
    }
    if (method == "POST" && path == "/api/reseed") {
        return response(200, "OK", "application/json", random.reseed());
    }
    if (method == "GET" && path.rfind("/api/random", 0) == 0) {
        const auto count = query_integer(target, "count", 8);
        const auto bound = query_integer(target, "bound", 0);
        return response(200, "OK", "application/json", random.generate(count, bound));
    }

    std::string asset_path;
    std::string type;
    if (method == "GET" && (path == "/" || path == "/index.html")) {
        asset_path = "web/index.html"; type = "text/html; charset=utf-8";
    } else if (method == "GET" && path == "/styles.css") {
        asset_path = "web/styles.css"; type = "text/css; charset=utf-8";
    } else if (method == "GET" && path == "/app.js") {
        asset_path = "web/app.js"; type = "text/javascript; charset=utf-8";
    } else {
        return response(404, "Not Found", "application/json",
                        "{\"error\":\"Not found\"}");
    }

    const auto body = read_file(asset_path);
    if (body.empty()) {
        return response(500, "Internal Server Error", "application/json",
                        "{\"error\":\"Dashboard assets are missing\"}");
    }
    return response(200, "OK", type, body);
}

std::uint16_t parse_port(int argc, char** argv) {
    if (argc < 2) return default_port;
    try {
        const auto value = std::stoul(argv[1]);
        if (value == 0 || value > 65535) throw std::out_of_range("port");
        return static_cast<std::uint16_t>(value);
    } catch (...) {
        throw std::invalid_argument("port must be between 1 and 65535");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto port = parse_port(argc, argv);
        SocketRuntime runtime;
        SocketGuard server(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (server.get() == invalid_socket) throw std::runtime_error("socket creation failed");

        int reuse = 1;
#if defined(_WIN32)
        setsockopt(server.get(), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
        setsockopt(server.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(server.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
            throw std::runtime_error("could not bind to 127.0.0.1; port may be in use");
        if (listen(server.get(), 16) != 0) throw std::runtime_error("listen failed");

        RandomService random;
        std::cout << "RDRAND dashboard: http://127.0.0.1:" << port << "\n"
                  << "Press Ctrl+C to stop.\n";

        for (;;) {
            sockaddr_in client_address{};
#if defined(_WIN32)
            int length = sizeof(client_address);
#else
            socklen_t length = sizeof(client_address);
#endif
            SocketGuard client(accept(server.get(), reinterpret_cast<sockaddr*>(&client_address),
                                      &length));
            if (client.get() == invalid_socket) continue;
            const auto request = receive_request(client.get());
            send_all(client.get(), handle_request(request, random));
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
