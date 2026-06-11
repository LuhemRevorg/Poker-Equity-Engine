#include <gtest/gtest.h>

#include "cards.h"

using namespace poker;

TEST(Cards, RoundTripAll52) {
    for (int r = 0; r < NUM_RANKS; ++r) {
        for (int s = 0; s < NUM_SUITS; ++s) {
            Card c = make_card(static_cast<Rank>(r), static_cast<Suit>(s));
            std::string str = card_to_str(c);
            auto parsed = card_from_str(str);
            ASSERT_TRUE(parsed.has_value()) << "Failed to parse " << str;
            EXPECT_EQ(*parsed, c) << "Round-trip mismatch for " << str;
        }
    }
}

TEST(Cards, ParseInvalid) {
    EXPECT_FALSE(card_from_str("").has_value());
    EXPECT_FALSE(card_from_str("A").has_value());
    EXPECT_FALSE(card_from_str("Xh").has_value());
    EXPECT_FALSE(card_from_str("Ax").has_value());
    EXPECT_FALSE(card_from_str("AhX").has_value());
    EXPECT_FALSE(card_from_str("1h").has_value());  // rank 1 doesn't exist
}

TEST(Cards, ParseCaseInsensitive) {
    EXPECT_EQ(*card_from_str("AH"), make_card(RA, HEARTS));
    EXPECT_EQ(*card_from_str("aH"), make_card(RA, HEARTS));
    EXPECT_EQ(*card_from_str("Ah"), make_card(RA, HEARTS));
    EXPECT_EQ(*card_from_str("ah"), make_card(RA, HEARTS));
}

TEST(Cards, ParseSpecificCards) {
    EXPECT_EQ(*card_from_str("Ah"), make_card(RA, HEARTS));
    EXPECT_EQ(*card_from_str("2c"), make_card(R2, CLUBS));
    EXPECT_EQ(*card_from_str("Ts"), make_card(RT, SPADES));
    EXPECT_EQ(*card_from_str("Kd"), make_card(RK, DIAMONDS));
}

TEST(Cards, RankSuitExtraction) {
    Card c = make_card(RQ, SPADES);
    EXPECT_EQ(rank_of(c), RQ);
    EXPECT_EQ(suit_of(c), SPADES);

    Card two_clubs = make_card(R2, CLUBS);
    EXPECT_EQ(rank_of(two_clubs), R2);
    EXPECT_EQ(suit_of(two_clubs), CLUBS);
    EXPECT_EQ(two_clubs, 0);

    Card ace_spades = make_card(RA, SPADES);
    EXPECT_EQ(ace_spades, 51);
}

TEST(Cards, BitOfDistinct) {
    Hand mask = 0;
    for (int i = 0; i < NUM_CARDS; ++i) {
        Card c = static_cast<Card>(i);
        Hand b = bit_of(c);
        EXPECT_EQ(popcount(b), 1);
        EXPECT_EQ(mask & b, 0u) << "Duplicate bit for card " << static_cast<int>(c);
        mask |= b;
    }
    EXPECT_EQ(mask, FULL_DECK);
}

TEST(Cards, BitmaskSetOps) {
    Hand h = 0;
    Card a = *card_from_str("Ah");
    Card k = *card_from_str("Kh");

    EXPECT_FALSE(is_dealt(h, a));
    h = mark_dealt(h, a);
    EXPECT_TRUE(is_dealt(h, a));
    EXPECT_FALSE(is_dealt(h, k));

    h = mark_dealt(h, k);
    EXPECT_TRUE(is_dealt(h, a));
    EXPECT_TRUE(is_dealt(h, k));
    EXPECT_EQ(popcount(h), 2);
}

TEST(Cards, FullDeckPopcount) {
    EXPECT_EQ(popcount(FULL_DECK), NUM_CARDS);
}

TEST(Cards, AllCardsDistinct) {
    Hand h = 0;
    for (int r = 0; r < NUM_RANKS; ++r) {
        for (int s = 0; s < NUM_SUITS; ++s) {
            Card c = make_card(static_cast<Rank>(r), static_cast<Suit>(s));
            EXPECT_FALSE(is_dealt(h, c));
            h = mark_dealt(h, c);
        }
    }
    EXPECT_EQ(h, FULL_DECK);
    EXPECT_EQ(popcount(h), NUM_CARDS);
}
