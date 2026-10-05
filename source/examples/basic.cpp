#include "rdrand_prng.hpp"

#include <cstdint>
#include <iostream>

int main() {
    try {
        intel_rng::RdrandEntropy hardware_entropy;
        intel_rng::Prng random(hardware_entropy);

        std::cout << "u64: " << random.next_u64() << '\n';
        std::cout << "0..99: " << random.bounded(100) << '\n';
        std::cout << "[0,1): " << random.uniform_double() << '\n';
    } catch (const intel_rng::entropy_error& error) {
        std::cerr << "Random hardware unavailable: " << error.what() << '\n';
        return 1;
    }
}

