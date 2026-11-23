// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_trophy_texts.h"

#include <charconv>

namespace SpartanTrophyTexts {

namespace {

std::string_view Trim(std::string_view s) {
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return s;
}

// The first `max` bytes, never ending inside a UTF-8 character.
std::string Cut(std::string_view s, size_t max) {
    if (s.size() <= max) return std::string(s);
    size_t end = max;
    while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80) --end;
    return std::string(s.substr(0, end));
}

} // namespace

Texts Parse(std::string_view text) {
    Texts out;
    if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    while (!text.empty()) {
        const size_t end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        text.remove_prefix(std::min(end + 1, text.size()));
        if (line.empty() || line.front() == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string_view key = line.substr(0, eq), value = Trim(line.substr(eq + 1));
        if (key.front() == '@') {
            out.labels.emplace_back(std::string(key.substr(1)), std::string(value));
            continue;
        }
        int id = -1;
        const auto [at, ec] = std::from_chars(key.data(), key.data() + key.size(), id);
        if (ec != std::errc() || at == key.data() || id < 0 || id >= kMaxTrophies) continue;
        std::string_view name = value, detail;
        if (const size_t bar = value.find('|'); bar != std::string_view::npos) {
            name = Trim(value.substr(0, bar));
            detail = Trim(value.substr(bar + 1));
        }
        if (name.empty()) continue;
        if (out.trophies[size_t(id)].first.empty()) ++out.count;
        out.trophies[size_t(id)] = {Cut(name, kMaxName), Cut(detail, kMaxDetail)};
    }
    return out;
}

} // namespace SpartanTrophyTexts
