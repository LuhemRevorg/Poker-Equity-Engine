#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "cards.h"

namespace poker {

// One specific 2-card combo in a range, with its weight (default 1.0).
// `cards` is a Hand bitmask with exactly 2 bits set.
struct ComboEntry {
    Hand   cards;
    double weight;
};

// A poker hand range, expanded into its enumerated combos.
//
// Grammar (standard PokerStove / Equilab-style notation):
//   range   = chunk ("," chunk)*
//   chunk   = "random" | "any" | hand_expr
//   hand_expr
//           = single | plus | dash
//   single  = RR | RRs | RRo | RR        (e.g. "AA", "AKs", "AKo", "AK")
//   plus    = single "+"                 (e.g. "QQ+", "A2s+", "AK+")
//   dash    = single "-" single          (e.g. "22-55", "JTs-J7s", "AKo-AQo")
//   R       = "2".."9" | "T" | "J" | "Q" | "K" | "A"   (case-insensitive)
//
// Semantics:
//   * Pair plus (XX+):    XX and every higher pair, up to AA.
//   * Suited plus (XYs+): keep X fixed, vary Y up to one below X. e.g. A2s+ = A2s..AKs.
//   * Non-pair without s/o: include BOTH suited and offsuit forms.
//   * Dash range: inclusive on both ends; second-rank delta only.
//
// Overlapping chunks (e.g. "QQ+,KK") are silently deduplicated.
class Range {
  public:
    // Parse a range expression. Returns std::nullopt if the input is invalid
    // (unknown rank char, malformed plus/dash, suit mod on a pair, etc.) or
    // if the result would be empty.
    static std::optional<Range> parse(std::string_view text);

    size_t size() const { return combos_.size(); }
    bool   empty() const { return combos_.empty(); }
    const std::vector<ComboEntry>& combos() const { return combos_; }
    const ComboEntry& operator[](size_t i) const { return combos_[i]; }

  private:
    std::vector<ComboEntry> combos_;
};

}  // namespace poker
