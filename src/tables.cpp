#include "evaluator.h"

#include <bit>
#include <cstdint>
#include <mutex>

namespace poker {

// Lookup tables. All indexed by a 13-bit rank pattern or a prime-product hash.
//   flushes[pattern]  : for 5 cards of the same suit, returns rank.
//                       Covers straight flushes (10) and regular flushes (1277).
//   unique5[pattern]  : for 5 distinct-rank cards not all the same suit, returns rank.
//                       Covers straights (10) and high cards (1277).
//   hash_keys/values  : open-addressing hash table for the 4888 multisets with
//                       at least one repeated rank (pair / 2pair / trips / full / quads).
//
// Total static storage: 4 * 8192 * (2 or 4 bytes) ≈ 96 KB — comfortably L2-resident,
// much smaller than the ~10 MB I estimated in the plan.
constexpr size_t TABLE_SIZE = 8192;  // 2^13, covers every 13-bit pattern
constexpr size_t HASH_SIZE  = 8192;  // > 4888 entries, ~60% load factor

uint16_t g_flushes[TABLE_SIZE];
uint16_t g_unique5[TABLE_SIZE];
uint32_t g_hash_keys[HASH_SIZE];
uint16_t g_hash_values[HASH_SIZE];

namespace {

// Multiplicative hash mapping a 32-bit prime product to a 13-bit table index.
// Constant is the 64-bit golden ratio; high bits of the product are the most
// scrambled, so we shift right by 64 - 13 = 51.
constexpr uint64_t HASH_MULT = 0x9E3779B185EBCA87ULL;

inline uint32_t hash_index(uint32_t key) {
    return static_cast<uint32_t>((static_cast<uint64_t>(key) * HASH_MULT) >> 51);
}

void hash_insert(uint32_t key, uint16_t value) {
    uint32_t h = hash_index(key);
    while (g_hash_keys[h] != 0) {
        h = (h + 1) & (HASH_SIZE - 1);
    }
    g_hash_keys[h]   = key;
    g_hash_values[h] = value;
}

// Is this 13-bit pattern a 5-card straight (incl. the A-2-3-4-5 wheel)?
constexpr bool is_straight_pattern(uint32_t pattern) {
    if (std::popcount(pattern) != 5) return false;
    if (pattern == 0b1'0000'0000'1111u) return true;  // A-2-3-4-5 wheel
    for (int shift = 0; shift <= 8; ++shift) {
        if (pattern == (0b11111u << shift)) return true;
    }
    return false;
}

uint32_t prime_product_from_counts(const int counts[NUM_RANKS]) {
    uint32_t prod = 1;
    for (int r = 0; r < NUM_RANKS; ++r) {
        for (int k = 0; k < counts[r]; ++k) prod *= PRIMES[r];
    }
    return prod;
}

void build_tables() {
    for (auto& v : g_flushes)     v = 0;
    for (auto& v : g_unique5)     v = 0;
    for (auto& v : g_hash_keys)   v = 0;
    for (auto& v : g_hash_values) v = 0;

    // ───────────────────────────────────────────────────────────────────────
    // HIGH CARD (1277)  →  unique5[pattern]
    // Iterate popcount-5 patterns in ascending integer order, skipping straights.
    // Ascending pattern value ↔ ascending hand strength (high bits = high cards).
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (uint32_t pattern = 0; pattern < TABLE_SIZE; ++pattern) {
            if (std::popcount(pattern) != 5) continue;
            if (is_straight_pattern(pattern)) continue;
            g_unique5[pattern] = encode_rank(HIGH_CARD, subrank++);
        }
        // sanity: 1287 popcount-5 patterns minus 10 straights = 1277 high-card hands
    }

    // ───────────────────────────────────────────────────────────────────────
    // ONE PAIR (2860)  →  hash table
    // Outer loop: pair rank ascending. Inner: kicker pattern (3 distinct ranks
    // not equal to the pair rank), iterated in ascending integer order so the
    // weakest kicker set comes first within each pair rank.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (int pair_rank = 0; pair_rank < NUM_RANKS; ++pair_rank) {
            const uint32_t pair_bit = 1u << pair_rank;
            for (uint32_t kp = 0; kp < TABLE_SIZE; ++kp) {
                if (kp & pair_bit) continue;
                if (std::popcount(kp) != 3) continue;
                int counts[NUM_RANKS] = {};
                counts[pair_rank] = 2;
                for (int r = 0; r < NUM_RANKS; ++r) {
                    if (kp & (1u << r)) counts[r] = 1;
                }
                hash_insert(prime_product_from_counts(counts),
                            encode_rank(ONE_PAIR, subrank++));
            }
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // TWO PAIR (858)  →  hash table
    // Order: (high_pair ASC, low_pair ASC, kicker ASC). high_pair beats low_pair
    // beats kicker in the standard tiebreak.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (int hi = 1; hi < NUM_RANKS; ++hi) {
            for (int lo = 0; lo < hi; ++lo) {
                for (int kicker = 0; kicker < NUM_RANKS; ++kicker) {
                    if (kicker == hi || kicker == lo) continue;
                    int counts[NUM_RANKS] = {};
                    counts[hi] = 2;
                    counts[lo] = 2;
                    counts[kicker] = 1;
                    hash_insert(prime_product_from_counts(counts),
                                encode_rank(TWO_PAIR, subrank++));
                }
            }
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // TRIPS (858)  →  hash table
    // Order: trips_rank ASC, then kicker pattern (2 distinct ranks) ASC.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (int trips_rank = 0; trips_rank < NUM_RANKS; ++trips_rank) {
            const uint32_t trips_bit = 1u << trips_rank;
            for (uint32_t kp = 0; kp < TABLE_SIZE; ++kp) {
                if (kp & trips_bit) continue;
                if (std::popcount(kp) != 2) continue;
                int counts[NUM_RANKS] = {};
                counts[trips_rank] = 3;
                for (int r = 0; r < NUM_RANKS; ++r) {
                    if (kp & (1u << r)) counts[r] = 1;
                }
                hash_insert(prime_product_from_counts(counts),
                            encode_rank(TRIPS, subrank++));
            }
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // STRAIGHT (10)  →  unique5[pattern]
    // Wheel first (subrank 1), then 6-high through A-high.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        g_unique5[0b1'0000'0000'1111u] = encode_rank(STRAIGHT, subrank++);
        for (int shift = 0; shift <= 8; ++shift) {
            g_unique5[0b11111u << shift] = encode_rank(STRAIGHT, subrank++);
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // FLUSH (1277)  →  flushes[pattern]
    // Same iteration as high cards: popcount-5 patterns minus straights,
    // ascending integer value.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (uint32_t pattern = 0; pattern < TABLE_SIZE; ++pattern) {
            if (std::popcount(pattern) != 5) continue;
            if (is_straight_pattern(pattern)) continue;
            g_flushes[pattern] = encode_rank(FLUSH, subrank++);
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // FULL HOUSE (156)  →  hash table
    // Order: trips_rank ASC, then pair_rank ASC.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (int trips_rank = 0; trips_rank < NUM_RANKS; ++trips_rank) {
            for (int pair_rank = 0; pair_rank < NUM_RANKS; ++pair_rank) {
                if (pair_rank == trips_rank) continue;
                int counts[NUM_RANKS] = {};
                counts[trips_rank] = 3;
                counts[pair_rank] = 2;
                hash_insert(prime_product_from_counts(counts),
                            encode_rank(FULL_HOUSE, subrank++));
            }
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // QUADS (156)  →  hash table
    // Order: quad_rank ASC, then kicker ASC.
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        for (int quad_rank = 0; quad_rank < NUM_RANKS; ++quad_rank) {
            for (int kicker = 0; kicker < NUM_RANKS; ++kicker) {
                if (kicker == quad_rank) continue;
                int counts[NUM_RANKS] = {};
                counts[quad_rank] = 4;
                counts[kicker] = 1;
                hash_insert(prime_product_from_counts(counts),
                            encode_rank(QUADS, subrank++));
            }
        }
    }

    // ───────────────────────────────────────────────────────────────────────
    // STRAIGHT FLUSH (10)  →  flushes[pattern]
    // Wheel (steel wheel) first, then 6-high through A-high (royal).
    // ───────────────────────────────────────────────────────────────────────
    {
        uint16_t subrank = 1;
        g_flushes[0b1'0000'0000'1111u] = encode_rank(STRAIGHT_FLUSH, subrank++);
        for (int shift = 0; shift <= 8; ++shift) {
            g_flushes[0b11111u << shift] = encode_rank(STRAIGHT_FLUSH, subrank++);
        }
    }
}

}  // namespace

void init_tables() {
    static std::once_flag flag;
    std::call_once(flag, build_tables);
}

}  // namespace poker
