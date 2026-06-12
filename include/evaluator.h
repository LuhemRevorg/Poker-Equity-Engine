#pragma once

#include <cstdint>

#include "cards.h"

namespace poker {

// Cactus Kev's 32-bit card encoding.
//   bits 28-16: rank-bit pattern (bit 16 = 2, bit 17 = 3, ..., bit 28 = A)
//   bits 15-12: suit bit (clubs=12, diamonds=13, hearts=14, spades=15)
//   bits 11-8:  rank index (0..12)
//   bits  5-0:  prime number for rank (2,3,5,7,11,13,17,19,23,29,31,37,41)
//
// The rank-bit pattern enables flush + straight detection via OR and AND.
// The prime allows perfect hashing of the rank multiset for pair/2pair/trips/full/quads.
using CKCard = uint32_t;

inline constexpr uint8_t PRIMES[NUM_RANKS] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41
};

constexpr CKCard to_ck(Card c) {
    const Rank r = rank_of(c);
    const Suit s = suit_of(c);
    return (CKCard{1} << (16 + r))
         | (CKCard{1} << (12 + s))
         | (CKCard{r} << 8)
         | CKCard{PRIMES[r]};
}

// Hand category. Higher value = stronger category.
// Encoded in the top 4 bits of the eval result (subrank in the low 12 bits).
enum HandCategory : uint8_t {
    HIGH_CARD       = 1,
    ONE_PAIR        = 2,
    TWO_PAIR        = 3,
    TRIPS           = 4,
    STRAIGHT        = 5,
    FLUSH           = 6,
    FULL_HOUSE      = 7,
    QUADS           = 8,
    STRAIGHT_FLUSH  = 9,
};

// Decode the category from an eval result. Subrank ordering inside a category
// is also higher = better, so a direct < / > comparison on the full uint16_t
// is the correct hand-strength comparison.
constexpr HandCategory hand_category(uint16_t rank) {
    return static_cast<HandCategory>(rank >> 12);
}

constexpr uint16_t hand_subrank(uint16_t rank) {
    return rank & 0x0FFF;
}

constexpr uint16_t encode_rank(HandCategory cat, uint16_t subrank) {
    return static_cast<uint16_t>((static_cast<uint16_t>(cat) << 12) | (subrank & 0x0FFF));
}

// Must be called once at process start before any eval5/eval7 call.
// Idempotent and thread-safe.
void init_tables();

// 5-card evaluator. Returns the encoded hand rank (higher = better).
uint16_t eval5(CKCard a, CKCard b, CKCard c, CKCard d, CKCard e);

// 7-card evaluator (Texas Hold'em). Enumerates the 21 5-card subsets via eval5
// and returns the best. `h` must be a 52-bit Hand mask with exactly 7 bits set.
uint16_t eval7(Hand h);

}  // namespace poker
