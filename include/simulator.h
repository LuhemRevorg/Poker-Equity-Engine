#pragma once

#include <cstdint>

#include "cards.h"
#include "prng.h"
#include "range.h"

namespace poker {

struct EquityResult {
    uint64_t wins   = 0;
    uint64_t ties   = 0;
    uint64_t losses = 0;
    uint64_t total  = 0;

    // Equity = (wins + ties/2) / total. Chopped pots count as half a win.
    double equity() const {
        if (total == 0) return 0.0;
        return (static_cast<double>(wins) + 0.5 * static_cast<double>(ties))
             / static_cast<double>(total);
    }
    double win_rate()  const { return total ? static_cast<double>(wins)   / total : 0.0; }
    double tie_rate()  const { return total ? static_cast<double>(ties)   / total : 0.0; }
    double loss_rate() const { return total ? static_cast<double>(losses) / total : 0.0; }
};

// Single-threaded Monte Carlo equity simulation, heads-up.
//
//   hero    : hero hole cards. Hand mask with exactly 2 bits set.
//   villain : villain hole cards. Hand mask with exactly 2 bits set, disjoint from hero.
//   board   : known community cards. 0..5 bits set, disjoint from hero and villain.
//   n_sims  : number of Monte Carlo simulations to run.
//   rng     : per-call mutable PRNG state.
//
// Each simulation samples the missing board cards (5 - popcount(board)) uniformly
// without replacement from the undealt cards via a Fisher-Yates partial shuffle,
// then evaluates both 7-card hands and records win/tie/loss.
EquityResult simulate(Hand hero, Hand villain, Hand board,
                      uint64_t n_sims, Xoshiro256pp& rng);

// Range-vs-range Monte Carlo equity. Each sim samples one combo uniformly from
// each side, rejection-sampling the villain combo if it conflicts with the
// chosen hero combo, then samples the missing board cards. If a sim cannot
// find a compatible (hero, villain) pair within an internal retry budget it
// is skipped and not counted in `EquityResult::total`.
EquityResult simulate(const Range& hero, const Range& villain, Hand board,
                      uint64_t n_sims, Xoshiro256pp& rng);

// Convenience: hero is a single fixed combo, villain is a range.
EquityResult simulate(Hand hero, const Range& villain, Hand board,
                      uint64_t n_sims, Xoshiro256pp& rng);

}  // namespace poker
