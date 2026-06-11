#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "prng.h"

using namespace poker;

TEST(Prng, DeterminismSameSeed) {
    Xoshiro256pp a(42);
    Xoshiro256pp b(42);
    for (int i = 0; i < 10'000; ++i) {
        ASSERT_EQ(a.next(), b.next()) << "Divergence at iteration " << i;
    }
}

TEST(Prng, DifferentSeedsDiverge) {
    Xoshiro256pp a(1);
    Xoshiro256pp b(2);
    bool any_different = false;
    for (int i = 0; i < 100; ++i) {
        if (a.next() != b.next()) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(Prng, NonZeroState) {
    Xoshiro256pp rng(0);
    bool nonzero = false;
    for (int i = 0; i < 10; ++i) {
        if (rng.next() != 0) {
            nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(nonzero) << "splitmix64 should produce non-zero state even from seed 0";
}

TEST(Prng, BoundedRangeRespected) {
    Xoshiro256pp rng(1234);
    for (int i = 0; i < 10'000; ++i) {
        EXPECT_LT(rng.next_bounded(52), 52u);
        EXPECT_LT(rng.next_bounded(7), 7u);
        EXPECT_LT(rng.next_bounded(1'000'000), 1'000'000u);
    }
}

// Chi-squared uniformity test for next_bounded(52) — the card-sampling case.
// 100M samples, expected ~1.923M per bucket.
// Critical value chi^2(0.001, df=51) is approx 85.35; we use that as a very
// loose upper bound — a good RNG essentially never trips it.
TEST(Prng, ChiSquaredUniformity52) {
    constexpr uint64_t kSamples = 100'000'000;
    constexpr uint64_t kBuckets = 52;
    std::vector<uint64_t> counts(kBuckets, 0);
    Xoshiro256pp rng(0xDEADBEEFULL);
    for (uint64_t i = 0; i < kSamples; ++i) {
        ++counts[rng.next_bounded(kBuckets)];
    }
    const double expected = static_cast<double>(kSamples) / kBuckets;
    double chi2 = 0.0;
    for (uint64_t c : counts) {
        double d = static_cast<double>(c) - expected;
        chi2 += d * d / expected;
    }
    std::cerr << "[INFO] chi^2 statistic over " << kSamples
              << " samples in " << kBuckets << " buckets: " << chi2
              << " (critical@0.001 = 85.35)\n";
    EXPECT_LT(chi2, 85.35)
        << "Chi-squared statistic " << chi2 << " exceeded 0.001 critical value";
}

TEST(Prng, ThroughputInfo) {
    constexpr uint64_t kIters = 100'000'000;
    Xoshiro256pp rng(0xC0FFEEULL);
    auto start = std::chrono::steady_clock::now();
    uint64_t acc = 0;
    for (uint64_t i = 0; i < kIters; ++i) acc += rng.next();
    auto end = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    double ns_per_call = ns / static_cast<double>(kIters);
    std::cerr << "[INFO] xoshiro256++ next(): " << ns_per_call
              << " ns/call (" << (1.0 / ns_per_call) << " G/sec)\n";
    // Anti-DCE: use acc.
    EXPECT_NE(acc, 0xCAFEBABEDEADBEEFULL);
    // Soft sanity: should be well under 10 ns/call on any modern CPU.
    EXPECT_LT(ns_per_call, 10.0);
}
