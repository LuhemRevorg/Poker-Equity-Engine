#include "simulator.h"

#include <utility>
#include <vector>

#include "evaluator.h"

namespace poker {

namespace {

// Filter a range down to combos that don't conflict with the board's dead cards.
std::vector<Hand> filter_combos(const Range& r, Hand board) {
    std::vector<Hand> out;
    out.reserve(r.size());
    for (const auto& c : r.combos()) {
        if (!(c.cards & board)) out.push_back(c.cards);
    }
    return out;
}

}  // namespace

EquityResult simulate(Hand hero, Hand villain, Hand board,
                      uint64_t n_sims, Xoshiro256pp& rng) {
    init_tables();

    // Build the list of undealt cards once, outside the hot loop. The Fisher-Yates
    // shuffle below mutates the front of this array per sim, then we restore it.
    uint8_t available[NUM_CARDS];
    int n_available = 0;
    const Hand dead = hero | villain | board;
    for (int c = 0; c < NUM_CARDS; ++c) {
        const Card card = static_cast<Card>(c);
        if (!is_dealt(dead, card)) {
            available[n_available++] = card;
        }
    }

    const int cards_to_deal = 5 - popcount(board);

    uint64_t wins = 0, ties = 0, losses = 0;

    for (uint64_t sim = 0; sim < n_sims; ++sim) {
        // Fisher-Yates partial shuffle of `available` over its first `cards_to_deal`
        // positions. Each chosen card is OR'd into sim_board. Track the partner
        // index of each swap so we can restore `available` after the eval.
        Hand sim_board = board;
        int picked_j[5];

        for (int i = 0; i < cards_to_deal; ++i) {
            const int j = i + static_cast<int>(rng.next_bounded(n_available - i));
            std::swap(available[i], available[j]);
            sim_board = mark_dealt(sim_board, available[i]);
            picked_j[i] = j;
        }

        const uint16_t hero_rank = eval7(hero    | sim_board);
        const uint16_t vill_rank = eval7(villain | sim_board);

        if      (hero_rank > vill_rank) ++wins;
        else if (hero_rank < vill_rank) ++losses;
        else                            ++ties;

        // Undo the swaps in reverse to restore `available` for the next sim.
        // Reverse order is required because swap i may have swapped a position
        // touched by an earlier swap.
        for (int i = cards_to_deal - 1; i >= 0; --i) {
            std::swap(available[i], available[picked_j[i]]);
        }
    }

    return {wins, ties, losses, n_sims};
}

EquityResult simulate(const Range& hero_range, const Range& vill_range, Hand board,
                      uint64_t n_sims, Xoshiro256pp& rng) {
    init_tables();

    const auto heros = filter_combos(hero_range, board);
    const auto vills = filter_combos(vill_range, board);
    if (heros.empty() || vills.empty()) return {0, 0, 0, 0};

    const int needed = 5 - popcount(board);

    uint64_t wins = 0, ties = 0, losses = 0;
    uint64_t completed = 0;

    // Cap retries when picking a villain combo compatible with hero. In normal
    // ranges this fires zero times; only pathological cases (e.g. AA vs AA with
    // hero = AsAh and villain = AsAh) hit this.
    constexpr int kMaxVillRetries = 1000;

    for (uint64_t sim = 0; sim < n_sims; ++sim) {
        const Hand hero = heros[rng.next_bounded(heros.size())];

        Hand vill = 0;
        bool found = false;
        for (int retries = 0; retries < kMaxVillRetries; ++retries) {
            vill = vills[rng.next_bounded(vills.size())];
            if (!(hero & vill)) { found = true; break; }
        }
        if (!found) continue;

        // Rejection-sample the remaining board cards from the full 52-card
        // deck against the running dead set. ~13% reject rate per draw with
        // 7 dead cards — averages 5.8 RNG calls to deal a 5-card runout.
        Hand sim_board = board;
        Hand dead = hero | vill | board;
        for (int i = 0; i < needed; ++i) {
            Card c;
            do {
                c = static_cast<Card>(rng.next_bounded(NUM_CARDS));
            } while (is_dealt(dead, c));
            sim_board = mark_dealt(sim_board, c);
            dead      = mark_dealt(dead, c);
        }

        const uint16_t hr = eval7(hero | sim_board);
        const uint16_t vr = eval7(vill | sim_board);

        if      (hr > vr) ++wins;
        else if (hr < vr) ++losses;
        else              ++ties;
        ++completed;
    }

    return {wins, ties, losses, completed};
}

EquityResult simulate(Hand hero, const Range& villain, Hand board,
                      uint64_t n_sims, Xoshiro256pp& rng) {
    return simulate(Range::from_hand(hero), villain, board, n_sims, rng);
}

}  // namespace poker
