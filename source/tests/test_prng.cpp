#include "rdrand_prng.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    intel_rng::Prng first(UINT64_C(0x0123456789ABCDEF));
    intel_rng::Prng second(UINT64_C(0x0123456789ABCDEF));

    for (int i = 0; i < 1000; ++i) {
        assert(first.next_u64() == second.next_u64());
    }

    intel_rng::Prng bounded_rng(UINT64_C(42));
    for (int i = 0; i < 100000; ++i) {
        assert(bounded_rng.bounded(17) < 17);
        const double value = bounded_rng.uniform_double();
        assert(value >= 0.0 && value < 1.0);
    }

    bool threw = false;
    try {
        (void)bounded_rng.bounded(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    // Hardware testing is intentionally conditional so deterministic tests can
    // run under VMs/emulators that do not expose RDRAND.
    if (intel_rng::RdrandEntropy::is_supported()) {
        intel_rng::RdrandEntropy entropy;
        intel_rng::Prng hardware_seeded(entropy);
        volatile auto sample = hardware_seeded.next_u64();
        (void)sample;
    }

    std::cout << "All tests passed\n";
    return 0;
}

