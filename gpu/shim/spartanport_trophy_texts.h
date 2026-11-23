// SPDX-License-Identifier: GPL-2.0-or-later
// SpartanPort: a trophy_texts.txt read by the overlay, for the language chosen inside the game. The
// launcher writes one per language in <user>/trophy_texts/<language>.txt, in the format the runtime
// reads at start (src/runtime_services.c, trophy_load_names; docs/TROPHIES.md): "@label=text" lines,
// then "ID=Name | Description", '#' starting a comment.

#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SpartanTrophyTexts {

constexpr int kMaxTrophies = 128;   ///< as the runtime (TROPHY_MAX)
constexpr size_t kMaxName = 63;     ///< bytes, as the runtime keeps them
constexpr size_t kMaxDetail = 159;

struct Texts {
    std::vector<std::pair<std::string, std::string>> labels; ///< "@key=text", in the file's order
    std::array<std::pair<std::string, std::string>, kMaxTrophies> trophies; ///< (name, description) by id
    int count = 0; ///< trophies with a name
};

/// The file's text; lines it does not understand are skipped, as the runtime does.
Texts Parse(std::string_view text);

} // namespace SpartanTrophyTexts
