// SPDX-License-Identifier: GPL-2.0-or-later
// SpartanPort: how many players earned each trophy of God of War III Remastered, for the trophy
// list's order and its "% of players". The port has no PSN connection, so these are fixed numbers:
// the share of the players tracked by Exophase (exophase.com) who earned each trophy, as listed in
// October 2026. Without this table the list keeps its own order and shows no percentage.

#pragma once

#include <array>

namespace SpartanTrophyRarity {

/// By trophy id (0 platinum, 1-18 bronze, 19-29 silver, 30-34 gold), in percent.
constexpr std::array<float, 35> kEarned{
    17.58f,                                                                         // 0
    96.17f, 88.31f, 77.38f, 73.00f, 68.89f, 63.78f, 74.60f, 65.73f, 61.36f, 59.96f, // 1-10
    20.99f, 74.21f, 22.06f, 22.69f, 25.90f, 24.58f, 75.05f, 61.74f,                 // 11-18
    81.22f, 67.16f, 65.39f, 58.79f, 76.16f, 62.61f, 63.58f, 38.23f, 44.92f, 36.38f, // 19-28
    32.85f,                                                                         // 29
    54.48f, 20.21f, 22.39f, 20.98f, 20.05f,                                         // 30-34
};

/// The share of players who earned trophy `id` (percent), or -1 when it is not known.
inline float Earned(int id, int trophy_count) {
    // Only for this game's list of 35: another package (a different game or region) has others.
    if (trophy_count != int(kEarned.size()) || id < 0 || id >= int(kEarned.size())) return -1.0f;
    return kEarned[size_t(id)];
}

/// The list's orders: 0 the game's own, 1 most earned first, 2 rarest first.
enum Order : int { ListOrder, MostEarned, Rarest };

/// Whether a trophy earned by `a` percent of players comes before one earned by `b` (-1: unknown) in
/// `order`. Unknown ones come after the others; equal ones keep the list's order (a stable sort).
inline bool Before(float a, float b, int order) {
    if (order == ListOrder) return false;
    if ((a < 0.0f) != (b < 0.0f)) return b < 0.0f;
    return order == MostEarned ? a > b : a < b;
}

/// PlayStation's tiers: 0 common (over 50%), 1 rare (over 15%), 2 very rare (over 5%), 3 ultra rare.
inline int Tier(float percent) { return percent > 50.0f ? 0 : percent > 15.0f ? 1 : percent > 5.0f ? 2 : 3; }

} // namespace SpartanTrophyRarity
