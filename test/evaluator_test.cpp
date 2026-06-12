#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <iostream>

#include "cards.h"
#include "evaluator.h"

using namespace poker;

namespace {

class EvaluatorTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() { init_tables(); }
};

CKCard ck(std::string_view s) {
    auto c = card_from_str(s);
    EXPECT_TRUE(c.has_value()) << "Failed to parse card " << s;
    return to_ck(*c);
}

Hand hand_from_strs(std::initializer_list<std::string_view> cards) {
    Hand h = 0;
    for (auto s : cards) {
        auto c = card_from_str(s);
        EXPECT_TRUE(c.has_value()) << "Failed to parse card " << s;
        h = mark_dealt(h, *c);
    }
    return h;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Per-category correctness for 5-card hands
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(EvaluatorTest, HighCard) {
    auto r = eval5(ck("Ah"), ck("Kd"), ck("Qc"), ck("9s"), ck("7h"));
    EXPECT_EQ(hand_category(r), HIGH_CARD);
}

TEST_F(EvaluatorTest, OnePair) {
    auto r = eval5(ck("Ah"), ck("Ad"), ck("Kc"), ck("Qs"), ck("2h"));
    EXPECT_EQ(hand_category(r), ONE_PAIR);
}

TEST_F(EvaluatorTest, TwoPair) {
    auto r = eval5(ck("Ah"), ck("Ad"), ck("Kc"), ck("Ks"), ck("2h"));
    EXPECT_EQ(hand_category(r), TWO_PAIR);
}

TEST_F(EvaluatorTest, Trips) {
    auto r = eval5(ck("Ah"), ck("Ad"), ck("Ac"), ck("Ks"), ck("Qh"));
    EXPECT_EQ(hand_category(r), TRIPS);
}

TEST_F(EvaluatorTest, Straight) {
    auto r = eval5(ck("Ah"), ck("Ks"), ck("Qc"), ck("Jd"), ck("Th"));
    EXPECT_EQ(hand_category(r), STRAIGHT);
}

TEST_F(EvaluatorTest, WheelStraight) {
    auto r = eval5(ck("Ah"), ck("2s"), ck("3c"), ck("4d"), ck("5h"));
    EXPECT_EQ(hand_category(r), STRAIGHT);
}

TEST_F(EvaluatorTest, Flush) {
    auto r = eval5(ck("Ah"), ck("Kh"), ck("Qh"), ck("Jh"), ck("9h"));
    EXPECT_EQ(hand_category(r), FLUSH);
}

TEST_F(EvaluatorTest, FullHouse) {
    auto r = eval5(ck("Ah"), ck("Ad"), ck("As"), ck("Kc"), ck("Kh"));
    EXPECT_EQ(hand_category(r), FULL_HOUSE);
}

TEST_F(EvaluatorTest, Quads) {
    auto r = eval5(ck("Ah"), ck("Ad"), ck("Ac"), ck("As"), ck("Kh"));
    EXPECT_EQ(hand_category(r), QUADS);
}

TEST_F(EvaluatorTest, StraightFlush) {
    auto r = eval5(ck("9h"), ck("8h"), ck("7h"), ck("6h"), ck("5h"));
    EXPECT_EQ(hand_category(r), STRAIGHT_FLUSH);
}

TEST_F(EvaluatorTest, RoyalFlush) {
    auto r = eval5(ck("Ah"), ck("Kh"), ck("Qh"), ck("Jh"), ck("Th"));
    EXPECT_EQ(hand_category(r), STRAIGHT_FLUSH);
    EXPECT_EQ(hand_subrank(r), 10);  // top subrank within STRAIGHT_FLUSH
}

TEST_F(EvaluatorTest, SteelWheelStraightFlush) {
    // A-2-3-4-5 same suit — the worst straight flush.
    auto r = eval5(ck("Ah"), ck("2h"), ck("3h"), ck("4h"), ck("5h"));
    EXPECT_EQ(hand_category(r), STRAIGHT_FLUSH);
    EXPECT_EQ(hand_subrank(r), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Category ordering — a hand in a higher category always beats a hand in a
// lower one, regardless of subrank.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(EvaluatorTest, CategoryOrdering) {
    const uint16_t royal      = eval5(ck("Ah"), ck("Kh"), ck("Qh"), ck("Jh"), ck("Th"));
    const uint16_t quads      = eval5(ck("Ah"), ck("Ad"), ck("Ac"), ck("As"), ck("Kh"));
    const uint16_t full_house = eval5(ck("Ah"), ck("Ad"), ck("As"), ck("Kc"), ck("Kh"));
    const uint16_t flush      = eval5(ck("Ah"), ck("Kh"), ck("Qh"), ck("Jh"), ck("9h"));
    const uint16_t straight   = eval5(ck("Ah"), ck("Ks"), ck("Qc"), ck("Jd"), ck("Th"));
    const uint16_t trips      = eval5(ck("Ah"), ck("Ad"), ck("Ac"), ck("Ks"), ck("Qh"));
    const uint16_t two_pair   = eval5(ck("Ah"), ck("Ad"), ck("Kc"), ck("Ks"), ck("2h"));
    const uint16_t one_pair   = eval5(ck("Ah"), ck("Ad"), ck("Kc"), ck("Qs"), ck("2h"));
    const uint16_t high_card  = eval5(ck("Ah"), ck("Kd"), ck("Qc"), ck("9s"), ck("7h"));

    EXPECT_GT(royal, quads);
    EXPECT_GT(quads, full_house);
    EXPECT_GT(full_house, flush);
    EXPECT_GT(flush, straight);
    EXPECT_GT(straight, trips);
    EXPECT_GT(trips, two_pair);
    EXPECT_GT(two_pair, one_pair);
    EXPECT_GT(one_pair, high_card);
}

// ─────────────────────────────────────────────────────────────────────────────
// Within-category subrank ordering
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(EvaluatorTest, BetterPairBeatsWorsePair) {
    auto aces  = eval5(ck("Ah"), ck("Ad"), ck("9c"), ck("7s"), ck("3h"));
    auto kings = eval5(ck("Kh"), ck("Kd"), ck("Qc"), ck("Js"), ck("Th"));
    EXPECT_GT(aces, kings);
}

TEST_F(EvaluatorTest, HigherKickerBeatsLower) {
    auto k_kicker = eval5(ck("Ah"), ck("Ad"), ck("Kc"), ck("7s"), ck("3h"));
    auto q_kicker = eval5(ck("Ah"), ck("Ad"), ck("Qc"), ck("7s"), ck("3h"));
    EXPECT_GT(k_kicker, q_kicker);
}

TEST_F(EvaluatorTest, AhighStraightBeatsWheel) {
    auto a_high = eval5(ck("Ah"), ck("Ks"), ck("Qc"), ck("Jd"), ck("Th"));
    auto wheel  = eval5(ck("Ah"), ck("2s"), ck("3c"), ck("4d"), ck("5h"));
    EXPECT_GT(a_high, wheel);
}

TEST_F(EvaluatorTest, AcesFullBeatsKingsFull) {
    auto aces_full  = eval5(ck("Ah"), ck("Ad"), ck("As"), ck("Kc"), ck("Kh"));
    auto kings_full = eval5(ck("Kh"), ck("Kd"), ck("Ks"), ck("Ac"), ck("Ad"));
    EXPECT_GT(aces_full, kings_full);
}

TEST_F(EvaluatorTest, QuadAcesBeatQuadKings) {
    auto qa = eval5(ck("Ah"), ck("Ad"), ck("Ac"), ck("As"), ck("2h"));
    auto qk = eval5(ck("Kh"), ck("Kd"), ck("Kc"), ck("Ks"), ck("Ah"));
    EXPECT_GT(qa, qk);
}

// ─────────────────────────────────────────────────────────────────────────────
// Exhaustive C(52,5) = 2,598,960 histogram against the textbook distribution.
// This is the strongest correctness check we have for the evaluator.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(EvaluatorTest, ExhaustiveCategoryHistogram) {
    std::array<uint64_t, 10> counts = {};  // categories 1..9

    CKCard ck_cards[NUM_CARDS];
    for (int i = 0; i < NUM_CARDS; ++i) {
        ck_cards[i] = to_ck(static_cast<Card>(i));
    }

    for (int a = 0; a < NUM_CARDS - 4; ++a) {
        for (int b = a + 1; b < NUM_CARDS - 3; ++b) {
            for (int c = b + 1; c < NUM_CARDS - 2; ++c) {
                for (int d = c + 1; d < NUM_CARDS - 1; ++d) {
                    for (int e = d + 1; e < NUM_CARDS; ++e) {
                        const uint16_t r = eval5(
                            ck_cards[a], ck_cards[b], ck_cards[c],
                            ck_cards[d], ck_cards[e]);
                        ++counts[hand_category(r)];
                    }
                }
            }
        }
    }

    // Textbook distribution for 5-card poker hands.
    EXPECT_EQ(counts[HIGH_CARD],      1'302'540u);
    EXPECT_EQ(counts[ONE_PAIR],       1'098'240u);
    EXPECT_EQ(counts[TWO_PAIR],         123'552u);
    EXPECT_EQ(counts[TRIPS],             54'912u);
    EXPECT_EQ(counts[STRAIGHT],          10'200u);
    EXPECT_EQ(counts[FLUSH],              5'108u);
    EXPECT_EQ(counts[FULL_HOUSE],         3'744u);
    EXPECT_EQ(counts[QUADS],                624u);
    EXPECT_EQ(counts[STRAIGHT_FLUSH],        40u);

    const uint64_t total = counts[HIGH_CARD] + counts[ONE_PAIR] + counts[TWO_PAIR]
                         + counts[TRIPS]    + counts[STRAIGHT] + counts[FLUSH]
                         + counts[FULL_HOUSE] + counts[QUADS]  + counts[STRAIGHT_FLUSH];
    EXPECT_EQ(total, 2'598'960u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7-card sanity: the board (Hold'em "best of 7")
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(EvaluatorTest, Eval7RoyalFlushOnBoard) {
    // Board plays the royal flush; hole cards are irrelevant junk.
    Hand h = hand_from_strs({"Ah", "Kh", "Qh", "Jh", "Th", "2c", "3d"});
    auto r = eval7(h);
    EXPECT_EQ(hand_category(r), STRAIGHT_FLUSH);
    EXPECT_EQ(hand_subrank(r), 10);  // royal
}

TEST_F(EvaluatorTest, Eval7TripsBeatsTwoPair) {
    // Hero makes trips with a pocket pair on a paired board.
    Hand trips    = hand_from_strs({"Ah", "Ad", "As", "Kc", "Qh", "5d", "2c"});
    Hand two_pair = hand_from_strs({"Ah", "Ad", "Kc", "Kd", "Qh", "5s", "2c"});
    EXPECT_GT(eval7(trips), eval7(two_pair));
}

TEST_F(EvaluatorTest, Eval7FlushPrefersHighest5) {
    // 6 hearts on the board+hole; best 5 = A-K-Q-J-9.
    Hand h = hand_from_strs({"Ah", "Kh", "Qh", "Jh", "9h", "2h", "3c"});
    auto r = eval7(h);
    EXPECT_EQ(hand_category(r), FLUSH);
}

TEST_F(EvaluatorTest, Eval7StraightFlushBeatsQuads) {
    // 6-7-8-9-T of hearts + Ah Ad Ac As are not co-occurrable in one deal,
    // so construct a 7-card hand that contains both a straight flush AND quads.
    // 4h-5h-6h-7h-8h (straight flush) + 8c-8d would be 8h being a flush card
    // AND part of quads. Use: 4h 5h 6h 7h 8h 8c 8d.
    Hand h = hand_from_strs({"4h", "5h", "6h", "7h", "8h", "8c", "8d"});
    auto r = eval7(h);
    EXPECT_EQ(hand_category(r), STRAIGHT_FLUSH);
}

// ─────────────────────────────────────────────────────────────────────────────
// Throughput info — printed, not asserted as a hard limit. We'll dial in the
// numbers via Google Benchmark in Step 5; for now we just want to see them.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(EvaluatorTest, ThroughputInfo) {
    constexpr int kIters = 1'000'000;
    Hand h = hand_from_strs({"Ah", "Kh", "Qh", "Jh", "9h", "2c", "3d"});

    // eval5 throughput
    CKCard a = ck("Ah"), b = ck("Kd"), c = ck("Qc"), d = ck("9s"), e = ck("7h");
    auto t0 = std::chrono::steady_clock::now();
    uint64_t acc5 = 0;
    for (int i = 0; i < kIters; ++i) {
        acc5 += eval5(a, b, c, d, e);
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns5 = std::chrono::duration<double, std::nano>(t1 - t0).count() / kIters;
    std::cerr << "[INFO] eval5: " << ns5 << " ns/call (acc=" << acc5 << ")\n";

    // eval7 throughput
    auto t2 = std::chrono::steady_clock::now();
    uint64_t acc7 = 0;
    for (int i = 0; i < kIters; ++i) {
        acc7 += eval7(h);
    }
    auto t3 = std::chrono::steady_clock::now();
    double ns7 = std::chrono::duration<double, std::nano>(t3 - t2).count() / kIters;
    std::cerr << "[INFO] eval7: " << ns7 << " ns/call (acc=" << acc7 << ")\n";

    // Anti-DCE: use the accumulators.
    EXPECT_NE(acc5, 0xCAFEBABEDEADBEEFULL);
    EXPECT_NE(acc7, 0xCAFEBABEDEADBEEFULL);
    // Loose ceiling — sanity check only. Tightened in the bench step.
    EXPECT_LT(ns5, 200.0);
    EXPECT_LT(ns7, 1000.0);
}
