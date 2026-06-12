#include "evaluator.h"

#include <bit>
#include <cstdint>

namespace poker {

// Defined in tables.cpp.
extern uint16_t g_flushes[8192];
extern uint16_t g_unique5[8192];
extern uint32_t g_hash_keys[8192];
extern uint16_t g_hash_values[8192];

namespace {

constexpr uint64_t HASH_MULT = 0x9E3779B185EBCA87ULL;
constexpr size_t   HASH_SIZE = 8192;

inline uint16_t lookup_hash(uint32_t key) {
    uint32_t h = static_cast<uint32_t>((static_cast<uint64_t>(key) * HASH_MULT) >> 51);
    while (g_hash_keys[h] != key) {
        h = (h + 1) & (HASH_SIZE - 1);
    }
    return g_hash_values[h];
}

}  // namespace

uint16_t eval5(CKCard a, CKCard b, CKCard c, CKCard d, CKCard e) {
    // 13-bit OR of the rank patterns of all five cards.
    const uint32_t q = (a | b | c | d | e) >> 16;

    // Flush detection: every CKCard has exactly one bit set in [12..15].
    // AND-ing across all 5 cards leaves a non-zero suit bit iff all five share a suit.
    if ((a & b & c & d & e) & 0xF000u) {
        return g_flushes[q];
    }

    // Five distinct ranks ⇒ straight or high card.
    if (std::popcount(q) == 5) {
        return g_unique5[q];
    }

    // Otherwise: pair / 2-pair / trips / full house / quads. The rank multiset
    // is uniquely identified by the prime product of the five rank-primes.
    const uint32_t prod = (a & 0xFFu) * (b & 0xFFu) * (c & 0xFFu) * (d & 0xFFu) * (e & 0xFFu);
    return lookup_hash(prod);
}

uint16_t eval7(Hand h) {
    // Unpack the 7 cards from the 52-bit bitmask.
    CKCard ck[7];
    int n = 0;
    Hand bits = h;
    while (bits) {
        const int idx = std::countr_zero(bits);
        ck[n++] = to_ck(static_cast<Card>(idx));
        bits &= bits - 1;
    }

    // 21 = C(7,5). Unrolled subset list — the compiler will hoist common
    // sub-expressions across iterations of eval5.
    static constexpr int S[21][5] = {
        {0,1,2,3,4}, {0,1,2,3,5}, {0,1,2,3,6},
        {0,1,2,4,5}, {0,1,2,4,6}, {0,1,2,5,6},
        {0,1,3,4,5}, {0,1,3,4,6}, {0,1,3,5,6}, {0,1,4,5,6},
        {0,2,3,4,5}, {0,2,3,4,6}, {0,2,3,5,6}, {0,2,4,5,6},
        {0,3,4,5,6},
        {1,2,3,4,5}, {1,2,3,4,6}, {1,2,3,5,6}, {1,2,4,5,6},
        {1,3,4,5,6},
        {2,3,4,5,6},
    };

    uint16_t best = 0;
    for (const auto& s : S) {
        const uint16_t r = eval5(ck[s[0]], ck[s[1]], ck[s[2]], ck[s[3]], ck[s[4]]);
        if (r > best) best = r;
    }
    return best;
}

}  // namespace poker
