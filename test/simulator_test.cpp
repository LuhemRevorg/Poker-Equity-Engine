#include <gtest/gtest.h>

#include <initializer_list>
#include <iostream>
#include <string_view>

#include "cards.h"
#include "evaluator.h"
#include "prng.h"
#include "simulator.h"

using namespace poker;

namespace {

Hand make_hand(std::initializer_list<std::string_view> cards) {
    Hand h = 0;
    for (auto s : cards) {
        h = mark_dealt(h, *card_from_str(s));
    }
    return h;
}

class SimulatorTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() { init_tables(); }
};

// 3σ tolerance at 1M sims for p ≈ 0.5: sqrt(0.25/1M) ≈ 5e-4, so 3σ ≈ 1.5e-3.
// We use 5e-3 as a comfortable upper bound for all preflop matchups.
constexpr double kTol = 0.005;
constexpr uint64_t kSims = 1'000'000;

}  // namespace

// ─── Validation gate ─────────────────────────────────────────────────────────
// Wrong equity here kills the project's credibility. Expected values are the
// exact published equities for the SPECIFIC suit combinations, not the
// averaged "AA vs KK = 81.5%" figure — suit overlap meaningfully shifts equity
// (e.g. AsAh vs KsKh is 82.8% because the shared spades+hearts mean every
// shared flush outcome is won by the higher card).

TEST_F(SimulatorTest, AAvsKK_MatchedSuits) {
    // AsAh vs KsKh — shared suits boost hero by ~1% vs the suit-averaged 81.5%.
    Hand hero = make_hand({"As", "Ah"});
    Hand vill = make_hand({"Ks", "Kh"});
    Xoshiro256pp rng(0xAA0011AAULL);
    auto r = simulate(hero, vill, 0, kSims, rng);
    std::cerr << "[INFO] AsAh vs KsKh: equity=" << r.equity()
              << " W=" << r.wins << " T=" << r.ties << " L=" << r.losses << "\n";
    EXPECT_NEAR(r.equity(), 0.8278, kTol);
}

TEST_F(SimulatorTest, AAvsKK_NoSuitOverlap) {
    // AsAh vs KcKd — disjoint suits. The flush wins split evenly between hero
    // and villain (vs all going to hero in the matched-suits case), so hero
    // equity drops from 82.78% to 81.23%.
    Hand hero = make_hand({"As", "Ah"});
    Hand vill = make_hand({"Kc", "Kd"});
    Xoshiro256pp rng(0xAA0022CCULL);
    auto r = simulate(hero, vill, 0, kSims, rng);
    std::cerr << "[INFO] AsAh vs KcKd: equity=" << r.equity() << "\n";
    EXPECT_NEAR(r.equity(), 0.8123, kTol);
}

TEST_F(SimulatorTest, AKsvsQQ_HeadsUp) {
    // AsKs vs QcQd — classic suited-AK vs underpair. Published ~46.16%.
    Hand hero = make_hand({"As", "Ks"});
    Hand vill = make_hand({"Qc", "Qd"});
    Xoshiro256pp rng(0xACE00550ULL);
    auto r = simulate(hero, vill, 0, kSims, rng);
    std::cerr << "[INFO] AsKs vs QcQd: equity=" << r.equity() << "\n";
    EXPECT_NEAR(r.equity(), 0.4616, kTol);
}

TEST_F(SimulatorTest, AKovsQQ_HeadsUp) {
    // AsKd vs QcQh — offsuit AK vs queens. Published ~42.86% for this specific combo.
    Hand hero = make_hand({"As", "Kd"});
    Hand vill = make_hand({"Qc", "Qh"});
    Xoshiro256pp rng(0xACE00770ULL);
    auto r = simulate(hero, vill, 0, kSims, rng);
    std::cerr << "[INFO] AsKd vs QcQh: equity=" << r.equity() << "\n";
    EXPECT_NEAR(r.equity(), 0.4286, kTol);
}

// ─── Flopped scenario ────────────────────────────────────────────────────────
// AA vs KK on AhKh2c flop: hero has set of aces, villain has set of kings.
// Villain only wins by hitting both remaining outs to quad kings; hero wins
// in essentially every other runout. Computed equity ≈ 0.9566.

TEST_F(SimulatorTest, SetOverSetOnFlop) {
    Hand hero  = make_hand({"As", "Ad"});
    Hand vill  = make_hand({"Ks", "Kd"});
    Hand board = make_hand({"Ah", "Kh", "2c"});
    Xoshiro256pp rng(0x5E70CE7ULL);
    auto r = simulate(hero, vill, board, kSims, rng);
    std::cerr << "[INFO] AA vs KK on AhKh2c: equity=" << r.equity() << "\n";
    EXPECT_NEAR(r.equity(), 0.9566, kTol);
}

// ─── Tie scenarios ───────────────────────────────────────────────────────────
// Both players play the board (royal flush on the table). Equity is exactly
// 0.5 (every sim is a tie; equity counts a tie as half a pot).

TEST_F(SimulatorTest, RoyalOnBoardForcesTie) {
    Hand hero  = make_hand({"2c", "3c"});
    Hand vill  = make_hand({"5d", "7d"});
    Hand board = make_hand({"Ah", "Kh", "Qh", "Jh", "Th"});
    Xoshiro256pp rng(0xDEADBEEFULL);
    // n_sims with all 5 board cards known is degenerate (1 outcome) but still works.
    auto r = simulate(hero, vill, board, 10'000, rng);
    std::cerr << "[INFO] Royal-on-board: W=" << r.wins << " T=" << r.ties
              << " L=" << r.losses << "\n";
    EXPECT_EQ(r.ties, 10'000u);
    EXPECT_EQ(r.wins, 0u);
    EXPECT_EQ(r.losses, 0u);
    EXPECT_EQ(r.equity(), 0.5);
}

// ─── Throughput info ─────────────────────────────────────────────────────────

TEST_F(SimulatorTest, ThroughputInfo) {
    Hand hero = make_hand({"As", "Ks"});
    Hand vill = make_hand({"Qc", "Qd"});
    Xoshiro256pp rng(0xABCDEF00ULL);

    auto t0 = std::chrono::steady_clock::now();
    auto r = simulate(hero, vill, 0, kSims, rng);
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::cerr << "[INFO] simulate(1M preflop): " << secs << "s ("
              << (kSims / secs / 1e6) << " M sims/sec, "
              << (1e9 * secs / kSims) << " ns/sim)\n";

    EXPECT_GT(r.total, 0u);
    // Soft sanity: should be at least 1M sims/sec single-threaded.
    EXPECT_LT(secs, 1.0);
}
