#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace intel_rng {

class entropy_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Hardware entropy adapter. RDRAND can transiently fail, so each request is
// retried. This class throws rather than silently using a weak fallback seed.
class RdrandEntropy {
public:
    static constexpr unsigned default_max_retries = 10;

    explicit RdrandEntropy(unsigned max_retries = default_max_retries);

    [[nodiscard]] static bool is_supported() noexcept;
    [[nodiscard]] std::uint64_t next_u64() const;
    void fill(void* destination, std::size_t byte_count) const;

private:
    unsigned max_retries_;
};

// xoshiro256**: a fast, non-cryptographic PRNG with 256 bits of state.
// One instance must not be accessed concurrently without external locking.
class Prng {
public:
    // Seeds from four independently requested RDRAND values.
    explicit Prng(const RdrandEntropy& entropy = RdrandEntropy{});

    // Deterministic constructor, useful for tests and repeatable simulations.
    explicit Prng(std::uint64_t deterministic_seed) noexcept;

    void reseed(const RdrandEntropy& entropy = RdrandEntropy{});
    void seed(std::uint64_t deterministic_seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;
    [[nodiscard]] std::uint32_t next_u32() noexcept;

    // Uniform result in [0, exclusive_upper_bound), without modulo bias.
    [[nodiscard]] std::uint64_t bounded(std::uint64_t exclusive_upper_bound);

    // Uniform double in [0, 1), using 53 random mantissa bits.
    [[nodiscard]] double uniform_double() noexcept;

private:
    std::array<std::uint64_t, 4> state_{};
};

} // namespace intel_rng

