#include "simulator.h"

#include <utility>

#include "evaluator.h"

namespace poker {

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

}  // namespace poker
