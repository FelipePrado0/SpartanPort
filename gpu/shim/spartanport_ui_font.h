// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <fstream>
#include "imgui.h"
#include "spartanport_ui_language.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace SpartanUi {
struct ChineseFontFile {
    std::filesystem::path path;
    int index = 0;
};

inline bool UsableFont(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return false;
    std::ifstream input(path, std::ios::binary);
    char signature[4]{};
    return input.read(signature, 4) &&
           (std::string_view(signature, 4) == std::string_view("\0\1\0\0", 4) ||
            std::string_view(signature, 4) == "OTTO" || std::string_view(signature, 4) == "ttcf");
}

// Read installed fonts only. No Microsoft or third-party font is bundled/copied by the port.
inline ChineseFontFile FindChineseFont() {
#ifdef _WIN32
    wchar_t directory[MAX_PATH]{};
    const UINT size = GetWindowsDirectoryW(directory, MAX_PATH);
    if (size && size < MAX_PATH) {
        for (const wchar_t* name : {L"msyh.ttc", L"msyh.ttf", L"simhei.ttf"}) {
            auto path = std::filesystem::path(directory) / L"Fonts" / name;
            if (UsableFont(path)) return {path, 0};
        }
    }
#else
    for (const char* name : {
             "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
             "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
             "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
             "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
             "/System/Library/Fonts/PingFang.ttc"}) {
        std::filesystem::path path(name);
        if (UsableFont(path)) {
            // Noto's collection orders Japanese, Korean, Simplified Chinese, Traditional, HK.
            return {path, path.filename() == "NotoSansCJK-Regular.ttc" ? 2 : 0};
        }
    }
#endif
    return {};
}

inline bool ConfigureChineseFont(ImFontAtlas* atlas, std::string_view language, const ChineseFontFile& file) {
    chinese = false;
    if (!WantsChinese(language) || atlas->Fonts.empty() || !UsableFont(file.path)) return false;
    // Keep ranges alive until the atlas is built. Load only UI characters on legacy renderers;
    // ImGui 1.92's texture-capable renderer loads additional characters on demand.
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(atlas->GetGlyphRangesDefault());
    for (const auto& [english, translated] : ChineseText) builder.AddText(translated);
    builder.BuildRanges(&ranges);
    ImFontConfig config;
    config.MergeMode = true;
    config.FontNo = file.index;
    const auto utf8 = file.path.u8string();
    chinese = atlas->AddFontFromFileTTF(reinterpret_cast<const char*>(utf8.c_str()), 18.0f,
                                      &config, ranges.Data) != nullptr;
    return chinese;
}
} // namespace SpartanUi
