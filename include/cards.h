#pragma once

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace poker {

using Card = uint8_t;
using Hand = uint64_t;
using Deck = uint64_t;

constexpr Card INVALID_CARD = 255;
constexpr int NUM_RANKS = 13;
constexpr int NUM_SUITS = 4;
constexpr int NUM_CARDS = 52;

enum Rank : uint8_t {
    R2 = 0, R3, R4, R5, R6, R7, R8, R9, RT, RJ, RQ, RK, RA
};

enum Suit : uint8_t {
    CLUBS = 0, DIAMONDS = 1, HEARTS = 2, SPADES = 3
};

constexpr Hand FULL_DECK = (Hand{1} << NUM_CARDS) - 1;

constexpr Card make_card(Rank r, Suit s) {
    return static_cast<Card>(static_cast<int>(r) * NUM_SUITS + static_cast<int>(s));
}

constexpr Rank rank_of(Card c) {
    return static_cast<Rank>(c / NUM_SUITS);
}

constexpr Suit suit_of(Card c) {
    return static_cast<Suit>(c % NUM_SUITS);
}

constexpr Hand bit_of(Card c) {
    return Hand{1} << c;
}

inline int popcount(Hand h) {
    return std::popcount(h);
}

constexpr bool is_dealt(Hand h, Card c) {
    return (h & bit_of(c)) != 0;
}

constexpr Hand mark_dealt(Hand h, Card c) {
    return h | bit_of(c);
}

inline std::optional<Card> card_from_str(std::string_view s) {
    if (s.size() != 2) return std::nullopt;

    int r;
    switch (s[0]) {
        case '2': r = R2; break;
        case '3': r = R3; break;
        case '4': r = R4; break;
        case '5': r = R5; break;
        case '6': r = R6; break;
        case '7': r = R7; break;
        case '8': r = R8; break;
        case '9': r = R9; break;
        case 'T': case 't': r = RT; break;
        case 'J': case 'j': r = RJ; break;
        case 'Q': case 'q': r = RQ; break;
        case 'K': case 'k': r = RK; break;
        case 'A': case 'a': r = RA; break;
        default: return std::nullopt;
    }

    int suit;
    switch (s[1]) {
        case 'c': case 'C': suit = CLUBS; break;
        case 'd': case 'D': suit = DIAMONDS; break;
        case 'h': case 'H': suit = HEARTS; break;
        case 's': case 'S': suit = SPADES; break;
        default: return std::nullopt;
    }

    return make_card(static_cast<Rank>(r), static_cast<Suit>(suit));
}

inline std::string card_to_str(Card c) {
    static constexpr char rank_chars[] = "23456789TJQKA";
    static constexpr char suit_chars[] = "cdhs";
    char buf[3] = {rank_chars[rank_of(c)], suit_chars[suit_of(c)], '\0'};
    return std::string{buf};
}

}  // namespace poker
