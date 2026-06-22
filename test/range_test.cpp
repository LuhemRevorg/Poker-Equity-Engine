#include <gtest/gtest.h>

#include <set>

#include "cards.h"
#include "range.h"

using namespace poker;

namespace {

size_t count(std::string_view s) {
    auto r = Range::parse(s);
    return r ? r->size() : 0;
}

}  // namespace

// ─── Single-hand expressions ─────────────────────────────────────────────────

TEST(Range, PocketPair) {
    EXPECT_EQ(count("QQ"), 6u);   // C(4,2) suit pairs
    EXPECT_EQ(count("22"), 6u);
    EXPECT_EQ(count("AA"), 6u);
}

TEST(Range, SuitedAndOffsuit) {
    EXPECT_EQ(count("AKs"), 4u);   // one combo per suit
    EXPECT_EQ(count("AKo"), 12u);  // 4 × 3 ordered different-suit pairs
    EXPECT_EQ(count("AK"),  16u);  // 4 suited + 12 offsuit
}

// ─── Plus expressions ────────────────────────────────────────────────────────

TEST(Range, PairPlus) {
    EXPECT_EQ(count("AA+"), 6u);    // AA only
    EXPECT_EQ(count("KK+"), 12u);   // KK, AA
    EXPECT_EQ(count("QQ+"), 18u);   // QQ, KK, AA
    EXPECT_EQ(count("22+"), 78u);   // all 13 pairs × 6
}

TEST(Range, SuitedPlus) {
    EXPECT_EQ(count("A2s+"), 48u);  // A2s..AKs = 12 rank pairs × 4 suits
    EXPECT_EQ(count("K2s+"), 44u);  // K2s..KQs = 11 × 4
    EXPECT_EQ(count("AKs+"), 4u);   // AKs only
}

TEST(Range, OffsuitPlus) {
    EXPECT_EQ(count("A2o+"), 144u); // 12 rank pairs × 12 offsuit combos
    EXPECT_EQ(count("AKo+"), 12u);
}

TEST(Range, NonPairPlusIncludesBothModes) {
    EXPECT_EQ(count("AK+"), 16u);          // = AKs + AKo
    EXPECT_EQ(count("A2+"), 12u * 16);     // 12 rank pairs × (4 + 12)
}

// ─── Dash expressions ────────────────────────────────────────────────────────

TEST(Range, PairDashRange) {
    EXPECT_EQ(count("22-55"), 24u);  // 22, 33, 44, 55
    EXPECT_EQ(count("88-AA"), 7u * 6);
    EXPECT_EQ(count("AA-22"), 78u);  // reversed bounds still works (lo..hi)
}

TEST(Range, SuitedDashRange) {
    EXPECT_EQ(count("JTs-J7s"), 16u);    // J7s..JTs = 4 × 4
    EXPECT_EQ(count("ATs-A2s"), 9u * 4); // A2s..ATs
}

TEST(Range, OffsuitDashRange) {
    EXPECT_EQ(count("AKo-AQo"), 24u);  // AQo + AKo = 12 + 12
}

TEST(Range, MixedDashRange) {
    EXPECT_EQ(count("AT-A8"), 3u * 16);  // A8, A9, AT × (4 + 12)
}

// ─── random / any ────────────────────────────────────────────────────────────

TEST(Range, RandomIsAll1326) {
    auto r = Range::parse("random");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->size(), 1326u);    // C(52,2)

    auto r2 = Range::parse("any");
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->size(), 1326u);
}

TEST(Range, RandomCombosAreAllDistinct2CardHands) {
    auto r = Range::parse("random");
    ASSERT_TRUE(r.has_value());

    std::set<Hand> seen;
    for (const auto& c : r->combos()) {
        EXPECT_EQ(popcount(c.cards), 2);
        EXPECT_TRUE(seen.insert(c.cards).second) << "duplicate combo";
    }
    EXPECT_EQ(seen.size(), 1326u);
}

// ─── Lists, dedup, whitespace ────────────────────────────────────────────────

TEST(Range, CommaList) {
    EXPECT_EQ(count("QQ+,AKs,AKo"), 18u + 4 + 12);
    EXPECT_EQ(count("AA, KK, QQ"),  18u);
    EXPECT_EQ(count("  QQ  ,  AK  "), 6u + 16);
}

TEST(Range, DedupOverlappingChunks) {
    EXPECT_EQ(count("QQ+,KK"),       18u);  // KK is in QQ+
    EXPECT_EQ(count("QQ+,AA+"),      18u);
    EXPECT_EQ(count("AKs,AKs,AKs"),  4u);   // self-overlap
    EXPECT_EQ(count("AK,AKs"),       16u);  // AKs ⊂ AK
}

TEST(Range, CaseInsensitive) {
    EXPECT_EQ(count("qq"),  6u);
    EXPECT_EQ(count("aks"), 4u);
    EXPECT_EQ(count("AKO"), 12u);
    EXPECT_EQ(count("Random"), 1326u);
    EXPECT_EQ(count("aNy"),    1326u);
}

// ─── Invalid input rejection ─────────────────────────────────────────────────

TEST(Range, InvalidInput) {
    EXPECT_FALSE(Range::parse("").has_value());
    EXPECT_FALSE(Range::parse(",,,").has_value());
    EXPECT_FALSE(Range::parse("XX").has_value());      // X not a rank
    EXPECT_FALSE(Range::parse("As").has_value());      // single-card notation, not range
    EXPECT_FALSE(Range::parse("A").has_value());
    EXPECT_FALSE(Range::parse("AAo").has_value());     // pair can't have suit mod
    EXPECT_FALSE(Range::parse("AKx").has_value());     // x not valid suit mod
    EXPECT_FALSE(Range::parse("JTs-J7o").has_value()); // mismatched suit mods
    EXPECT_FALSE(Range::parse("JTs-A7s").has_value()); // mismatched first rank
    EXPECT_FALSE(Range::parse("AT-A2s").has_value());  // mismatched lengths
    EXPECT_FALSE(Range::parse("22-AAo").has_value());  // can't mix pair with non-pair
    EXPECT_FALSE(Range::parse("QQ,XX").has_value());   // one bad chunk poisons the whole range
}

// ─── Combo correctness — every combo is 2 cards, properly formed ─────────────

TEST(Range, EveryComboIsValidTwoCardHand) {
    for (auto spec : {"QQ+", "AKs", "AKo", "A2s+", "JTs-J7s", "random", "AT-A8"}) {
        auto r = Range::parse(spec);
        ASSERT_TRUE(r.has_value()) << "Failed to parse: " << spec;
        for (const auto& c : r->combos()) {
            EXPECT_EQ(popcount(c.cards), 2) << "spec=" << spec;
            EXPECT_EQ(c.weight, 1.0);
        }
    }
}
