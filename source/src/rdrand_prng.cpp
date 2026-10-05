#include "rdrand_prng.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

namespace intel_rng {
namespace {

#if defined(_M_X64) || defined(__x86_64__)
constexpr bool is_x86_64 = true;
#else
constexpr bool is_x86_64 = false;
#endif

#if defined(_M_IX86) || defined(__i386__)
constexpr bool is_x86_32 = true;
#else
constexpr bool is_x86_32 = false;
#endif

bool rdrand_step32(std::uint32_t& value) noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    return _rdrand32_step(reinterpret_cast<unsigned int*>(&value)) != 0;
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    unsigned char success;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(success));
    return success != 0;
#else
    (void)value;
    return false;
#endif
}

bool rdrand_step64(std::uint64_t& value) noexcept {
#if defined(_MSC_VER) && defined(_M_X64)
    return _rdrand64_step(reinterpret_cast<unsigned __int64*>(&value)) != 0;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
    unsigned char success;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(success));
    return success != 0;
#elif defined(_M_IX86) || defined(__i386__)
    std::uint32_t low = 0;
    std::uint32_t high = 0;
    if (!rdrand_step32(low) || !rdrand_step32(high)) {
        return false;
    }
    value = (static_cast<std::uint64_t>(high) << 32U) | low;
    return true;
#else
    (void)value;
    return false;
#endif
}

std::uint64_t rotate_left(std::uint64_t value, int count) noexcept {
    return (value << count) | (value >> (64 - count));
}

std::uint64_t splitmix64(std::uint64_t& state) noexcept {
    std::uint64_t z = (state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31U);
}

} // namespace

RdrandEntropy::RdrandEntropy(unsigned max_retries) : max_retries_(max_retries) {
    if (max_retries == 0) {
        throw std::invalid_argument("RDRAND retry count must be greater than zero");
    }
    if (!is_supported()) {
        throw entropy_error("RDRAND is not supported by this CPU");
    }
}

bool RdrandEntropy::is_supported() noexcept {
    if (!is_x86_64 && !is_x86_32) {
        return false;
    }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int registers[4]{};
    __cpuid(registers, 1);
    return (static_cast<unsigned>(registers[2]) & (1U << 30U)) != 0;
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    return __get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0 &&
           (ecx & (1U << 30U)) != 0;
#else
    return false;
#endif
}

std::uint64_t RdrandEntropy::next_u64() const {
    for (unsigned attempt = 0; attempt < max_retries_; ++attempt) {
        std::uint64_t value = 0;
        if (rdrand_step64(value)) {
            return value;
        }
    }
    throw entropy_error("RDRAND failed after the configured retry limit");
}

void RdrandEntropy::fill(void* destination, std::size_t byte_count) const {
    if (byte_count != 0 && destination == nullptr) {
        throw std::invalid_argument("entropy destination must not be null");
    }

    auto* output = static_cast<unsigned char*>(destination);
    while (byte_count != 0) {
        const auto random = next_u64();
        const auto count = std::min(byte_count, sizeof(random));
        std::memcpy(output, &random, count);
        output += count;
        byte_count -= count;
    }
}

Prng::Prng(const RdrandEntropy& entropy) {
    reseed(entropy);
}

Prng::Prng(std::uint64_t deterministic_seed) noexcept {
    seed(deterministic_seed);
}

void Prng::reseed(const RdrandEntropy& entropy) {
    // Mix all samples so nearby or repeated hardware values cannot create a
    // structurally weak xoshiro state.
    std::uint64_t accumulator = UINT64_C(0x6A09E667F3BCC909);
    for (std::size_t i = 0; i < state_.size(); ++i) {
        accumulator ^= entropy.next_u64() +
                       UINT64_C(0x9E3779B97F4A7C15) * (i + 1U);
        state_[i] = splitmix64(accumulator);
    }
}

void Prng::seed(std::uint64_t deterministic_seed) noexcept {
    for (auto& word : state_) {
        word = splitmix64(deterministic_seed);
    }
}

std::uint64_t Prng::next_u64() noexcept {
    const std::uint64_t result = rotate_left(state_[1] * 5U, 7) * 9U;
    const std::uint64_t temporary = state_[1] << 17U;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= temporary;
    state_[3] = rotate_left(state_[3], 45);
    return result;
}

std::uint32_t Prng::next_u32() noexcept {
    return static_cast<std::uint32_t>(next_u64() >> 32U);
}

std::uint64_t Prng::bounded(std::uint64_t exclusive_upper_bound) {
    if (exclusive_upper_bound == 0) {
        throw std::invalid_argument("upper bound must be greater than zero");
    }

    const std::uint64_t rejection_threshold =
        static_cast<std::uint64_t>(-exclusive_upper_bound) % exclusive_upper_bound;
    for (;;) {
        const auto value = next_u64();
        if (value >= rejection_threshold) {
            return value % exclusive_upper_bound;
        }
    }
}

double Prng::uniform_double() noexcept {
    return static_cast<double>(next_u64() >> 11U) * 0x1.0p-53;
}

} // namespace intel_rng

