#pragma once

#include <array>
#include <cstdint>

namespace poker {

// xoshiro256++ from https://prng.di.unimi.it/xoshiro256plusplus.c
// State is 256 bits (four uint64s). Period 2^256 - 1.
// Each instance is independent; give each thread its own.
struct Xoshiro256pp {
    std::array<uint64_t, 4> s{};

    explicit Xoshiro256pp(uint64_t seed = 0xC0FFEEULL) {
        seed_from(seed);
    }

    static constexpr uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    // splitmix64 — seeds the four state words from a single uint64_t.
    // Required to avoid the all-zero state, which is invalid for xoshiro.
    static constexpr uint64_t splitmix64(uint64_t& state) {
        uint64_t result = (state += 0x9E3779B97F4A7C15ULL);
        result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9ULL;
        result = (result ^ (result >> 27)) * 0x94D049BB133111EBULL;
        return result ^ (result >> 31);
    }

    void seed_from(uint64_t seed) {
        uint64_t st = seed;
        for (auto& v : s) v = splitmix64(st);
    }

    inline uint64_t next() {
        const uint64_t result = rotl(s[0] + s[3], 23) + s[0];
        const uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }

    // Lemire's nearly-divisionless bounded RNG. Uniform in [0, n) for n > 0.
    // Branch predictor almost never hits the rejection path for small n.
    inline uint64_t next_bounded(uint64_t n) {
        uint64_t x = next();
        __uint128_t m = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(n);
        uint64_t l = static_cast<uint64_t>(m);
        if (l < n) {
            uint64_t t = (~n + 1) % n;  // -n % n, unsigned-safe
            while (l < t) {
                x = next();
                m = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(n);
                l = static_cast<uint64_t>(m);
            }
        }
        return static_cast<uint64_t>(m >> 64);
    }
};

}  // namespace poker
