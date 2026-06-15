#include <chrono>
#include <iomanip>
#include <iostream>

#include "cards.h"
#include "evaluator.h"
#include "prng.h"
#include "simulator.h"

// Skeleton CLI — Step 3. Hardcoded AA vs KK preflop matchup with 1M simulations.
// Replaced with proper argparse in Step 7.

int main() {
    using namespace poker;
    init_tables();

    Hand hero = mark_dealt(mark_dealt(Hand{0}, *card_from_str("As")), *card_from_str("Ah"));
    Hand vill = mark_dealt(mark_dealt(Hand{0}, *card_from_str("Ks")), *card_from_str("Kh"));

    constexpr uint64_t kSims = 1'000'000;
    Xoshiro256pp rng(0xC0FFEE5EEDULL);

    const auto start = std::chrono::steady_clock::now();
    const auto r = simulate(hero, vill, 0, kSims, rng);
    const auto end = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(end - start).count();

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "AA vs KK heads-up preflop\n";
    std::cout << "  Sims:    " << r.total << "\n";
    std::cout << "  Time:    " << secs << "s  ("
              << (kSims / secs / 1e6) << " M sims/sec)\n";
    std::cout << "  Equity:  " << r.equity() << "\n";
    std::cout << "  Win:     " << r.win_rate()  << "  (" << r.wins   << ")\n";
    std::cout << "  Tie:     " << r.tie_rate()  << "  (" << r.ties   << ")\n";
    std::cout << "  Loss:    " << r.loss_rate() << "  (" << r.losses << ")\n";
    return 0;
}
