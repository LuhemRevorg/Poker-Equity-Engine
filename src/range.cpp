#include "range.h"

#include <algorithm>
#include <cctype>

namespace poker {

namespace {

std::optional<Rank> parse_rank_char(char c) {
    switch (c) {
        case '2': return R2;
        case '3': return R3;
        case '4': return R4;
        case '5': return R5;
        case '6': return R6;
        case '7': return R7;
        case '8': return R8;
        case '9': return R9;
        case 'T': case 't': return RT;
        case 'J': case 'j': return RJ;
        case 'Q': case 'q': return RQ;
        case 'K': case 'k': return RK;
        case 'A': case 'a': return RA;
        default:  return std::nullopt;
    }
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.remove_suffix(1);
    return s;
}

bool ieq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

constexpr bool is_suit_mod(char c) { return c == 's' || c == 'o' || c == 'S' || c == 'O'; }
constexpr bool is_suited(char c)   { return c == 's' || c == 'S'; }

// ─── combo emitters ──────────────────────────────────────────────────────────

void emit_pair(Rank r, std::vector<ComboEntry>& out) {
    for (int s1 = 0; s1 < NUM_SUITS; ++s1) {
        for (int s2 = s1 + 1; s2 < NUM_SUITS; ++s2) {
            const Hand h = bit_of(make_card(r, Suit(s1))) | bit_of(make_card(r, Suit(s2)));
            out.push_back({h, 1.0});
        }
    }
}

void emit_suited(Rank r1, Rank r2, std::vector<ComboEntry>& out) {
    for (int s = 0; s < NUM_SUITS; ++s) {
        const Hand h = bit_of(make_card(r1, Suit(s))) | bit_of(make_card(r2, Suit(s)));
        out.push_back({h, 1.0});
    }
}

void emit_offsuit(Rank r1, Rank r2, std::vector<ComboEntry>& out) {
    for (int s1 = 0; s1 < NUM_SUITS; ++s1) {
        for (int s2 = 0; s2 < NUM_SUITS; ++s2) {
            if (s1 == s2) continue;
            const Hand h = bit_of(make_card(r1, Suit(s1))) | bit_of(make_card(r2, Suit(s2)));
            out.push_back({h, 1.0});
        }
    }
}

void emit_random(std::vector<ComboEntry>& out) {
    for (int hi = 0; hi <= RA; ++hi) {
        for (int lo = 0; lo <= hi; ++lo) {
            if (hi == lo) {
                emit_pair(Rank(hi), out);
            } else {
                emit_suited (Rank(hi), Rank(lo), out);
                emit_offsuit(Rank(hi), Rank(lo), out);
            }
        }
    }
}

// ─── per-chunk parsers ───────────────────────────────────────────────────────
//
// Each returns true on success. On failure, `out` may have been partially
// appended; the top-level parse() throws away the whole Range in that case.

bool parse_single(std::string_view s, std::vector<ComboEntry>& out) {
    if (s.size() < 2 || s.size() > 3) return false;
    const auto r1 = parse_rank_char(s[0]);
    const auto r2 = parse_rank_char(s[1]);
    if (!r1 || !r2) return false;

    if (s.size() == 2) {
        if (*r1 == *r2) {
            emit_pair(*r1, out);
        } else {
            emit_suited (*r1, *r2, out);
            emit_offsuit(*r1, *r2, out);
        }
        return true;
    }
    if (*r1 == *r2) return false;            // pair can't carry a suit modifier
    if (!is_suit_mod(s[2])) return false;
    if (is_suited(s[2])) emit_suited (*r1, *r2, out);
    else                 emit_offsuit(*r1, *r2, out);
    return true;
}

bool parse_plus(std::string_view s, std::vector<ComboEntry>& out) {
    if (s.size() < 2 || s.size() > 3) return false;
    const auto r1 = parse_rank_char(s[0]);
    const auto r2 = parse_rank_char(s[1]);
    if (!r1 || !r2) return false;

    if (s.size() == 2) {
        if (*r1 == *r2) {
            for (int r = *r1; r <= RA; ++r) emit_pair(Rank(r), out);
            return true;
        }
        const int hi = std::max(*r1, *r2);
        const int lo = std::min(*r1, *r2);
        for (int y = lo; y < hi; ++y) {
            emit_suited (Rank(hi), Rank(y), out);
            emit_offsuit(Rank(hi), Rank(y), out);
        }
        return true;
    }
    if (*r1 == *r2) return false;
    if (!is_suit_mod(s[2])) return false;
    const bool suited = is_suited(s[2]);
    const int  hi = std::max(*r1, *r2);
    const int  lo = std::min(*r1, *r2);
    for (int y = lo; y < hi; ++y) {
        if (suited) emit_suited (Rank(hi), Rank(y), out);
        else        emit_offsuit(Rank(hi), Rank(y), out);
    }
    return true;
}

bool parse_dash(std::string_view lhs, std::string_view rhs, std::vector<ComboEntry>& out) {
    if (lhs.size() != rhs.size()) return false;
    if (lhs.size() < 2 || lhs.size() > 3) return false;

    const auto l1 = parse_rank_char(lhs[0]);
    const auto l2 = parse_rank_char(lhs[1]);
    const auto r1 = parse_rank_char(rhs[0]);
    const auto r2 = parse_rank_char(rhs[1]);
    if (!l1 || !l2 || !r1 || !r2) return false;

    const bool lhs_pair = (*l1 == *l2);
    const bool rhs_pair = (*r1 == *r2);

    if (lhs.size() == 2) {
        if (lhs_pair && rhs_pair) {
            const int hi = std::max(*l1, *r1);
            const int lo = std::min(*l1, *r1);
            for (int r = lo; r <= hi; ++r) emit_pair(Rank(r), out);
            return true;
        }
        if (lhs_pair || rhs_pair) return false;     // can't mix pair with non-pair
        if (*l1 != *r1) return false;               // first rank must match (e.g. AT-A2)
        const int fixed = *l1;
        const int y_hi  = std::max(*l2, *r2);
        const int y_lo  = std::min(*l2, *r2);
        for (int y = y_lo; y <= y_hi; ++y) {
            if (y == fixed) continue;
            emit_suited (Rank(fixed), Rank(y), out);
            emit_offsuit(Rank(fixed), Rank(y), out);
        }
        return true;
    }
    // 3 chars: suit-modified non-pair range, e.g. JTs-J7s.
    if (lhs[2] != rhs[2]) return false;
    if (!is_suit_mod(lhs[2])) return false;
    if (lhs_pair || rhs_pair) return false;
    if (*l1 != *r1) return false;
    const bool suited = is_suited(lhs[2]);
    const int  fixed  = *l1;
    const int  y_hi   = std::max(*l2, *r2);
    const int  y_lo   = std::min(*l2, *r2);
    for (int y = y_lo; y <= y_hi; ++y) {
        if (y == fixed) continue;
        if (suited) emit_suited (Rank(fixed), Rank(y), out);
        else        emit_offsuit(Rank(fixed), Rank(y), out);
    }
    return true;
}

bool parse_chunk(std::string_view chunk, std::vector<ComboEntry>& out) {
    chunk = trim(chunk);
    if (chunk.empty()) return false;

    if (ieq(chunk, "random") || ieq(chunk, "any")) {
        emit_random(out);
        return true;
    }

    if (const auto dash = chunk.find('-'); dash != std::string_view::npos) {
        return parse_dash(trim(chunk.substr(0, dash)),
                          trim(chunk.substr(dash + 1)), out);
    }

    if (chunk.back() == '+') {
        return parse_plus(chunk.substr(0, chunk.size() - 1), out);
    }

    return parse_single(chunk, out);
}

}  // namespace

Range Range::from_hand(Hand cards) {
    Range r;
    r.combos_.push_back({cards, 1.0});
    return r;
}

std::optional<Range> Range::parse(std::string_view text) {
    Range r;

    size_t pos = 0;
    while (true) {
        const size_t end = std::min(text.find(',', pos), text.size());
        const std::string_view chunk = text.substr(pos, end - pos);
        if (!trim(chunk).empty()) {
            if (!parse_chunk(chunk, r.combos_)) return std::nullopt;
        }
        if (end == text.size()) break;
        pos = end + 1;
    }

    // Dedupe overlapping chunks (e.g. "QQ+,KK" → 18 combos, not 24).
    std::sort(r.combos_.begin(), r.combos_.end(),
              [](const ComboEntry& a, const ComboEntry& b) { return a.cards < b.cards; });
    r.combos_.erase(
        std::unique(r.combos_.begin(), r.combos_.end(),
                    [](const ComboEntry& a, const ComboEntry& b) { return a.cards == b.cards; }),
        r.combos_.end());

    if (r.combos_.empty()) return std::nullopt;
    return r;
}

}  // namespace poker
