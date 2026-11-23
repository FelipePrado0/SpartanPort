// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the shader preparation screen in the launcher languages the in-game menu does not have
// (Japanese, Korean, Arabic), keyed by the English text like the menu's tables. Japanese and Korean
// are drawn with an installed CJK font (as Chinese is); the Arabic texts are stored already joined
// (presentation forms) and in display order, since ImGui neither joins Arabic letters nor lays text
// out right to left. tests/test_loading_text.cpp checks them.
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

#include "spartanport_ui_language.h"

namespace SpartanUi {
inline constexpr TextTable LoadingJapanese[] = {
    {"Preparing shaders", "シェーダーを準備中"},
    {"Preparing pipelines", "パイプラインを準備中"},
    {"Reading the shader cache", "シェーダーキャッシュを読み込み中"},
    {"First start: preparing the graphics once", "初回起動：グラフィックを一度だけ準備しています"},
    {"New graphics driver: preparing the pipelines again", "新しいグラフィックドライバー：パイプラインを再準備しています"},
    {"New shader list: preparing what changed", "新しいシェーダーリスト：変更分を準備しています"},
    {"Reading the saved shaders from the disk", "保存済みシェーダーをディスクから読み込み中"},
    {"Port update: preparing the graphics again", "ポートの更新：グラフィックを再準備しています"},
    {"Shaders", "シェーダー"},
    {"Pipelines", "パイプライン"},
    {"Cache", "キャッシュ"},
    {"About %d s left", "残り約 %d 秒"},
    {"About %d min left", "残り約 %d 分"},
    {"Almost done", "まもなく完了"},
    {"Esc or Circle: start playing now", "Esc または〇ボタン：今すぐプレイ"},
};
inline constexpr TextTable LoadingKorean[] = {
    {"Preparing shaders", "셰이더 준비 중"},
    {"Preparing pipelines", "파이프라인 준비 중"},
    {"Reading the shader cache", "셰이더 캐시 읽는 중"},
    {"First start: preparing the graphics once", "첫 실행: 그래픽을 한 번만 준비합니다"},
    {"New graphics driver: preparing the pipelines again", "새 그래픽 드라이버: 파이프라인을 다시 준비합니다"},
    {"New shader list: preparing what changed", "새 셰이더 목록: 바뀐 부분을 준비합니다"},
    {"Reading the saved shaders from the disk", "저장된 셰이더를 디스크에서 읽는 중"},
    {"Port update: preparing the graphics again", "포트 업데이트: 그래픽을 다시 준비합니다"},
    {"Shaders", "셰이더"},
    {"Pipelines", "파이프라인"},
    {"Cache", "캐시"},
    {"About %d s left", "약 %d초 남음"},
    {"About %d min left", "약 %d분 남음"},
    {"Almost done", "거의 완료"},
    {"Esc or Circle: start playing now", "Esc 또는 ○ 버튼: 지금 플레이"},
};
inline constexpr TextTable LoadingArabic[] = {
    {"Preparing shaders", "ﺕﻼﻠﻈﻤﻟﺍ ﺰﻴﻬﺠﺗ ﺭﺎﺟ"},
    {"Preparing pipelines", "ﺔﺠﻟﺎﻌﻤﻟﺍ ﻁﻮﻄﺧ ﺰﻴﻬﺠﺗ ﺭﺎﺟ"},
    {"Reading the shader cache", "ﺔﺘﻗﺆﻤﻟﺍ ﺕﻼﻠﻈﻤﻟﺍ ﺓﺮﻛﺍﺫ ﺓﺀﺍﺮﻗ ﺭﺎﺟ"},
    {"First start: preparing the graphics once", "ﺓﺪﺣﺍﻭ ﺓﺮﻣ ﺕﺎﻴﻣﻮﺳﺮﻟﺍ ﺰﻴﻬﺠﺗ :ﻝﻭﻷﺍ ﻞﻴﻐﺸﺘﻟﺍ"},
    {"New graphics driver: preparing the pipelines again", "ﺔﺠﻟﺎﻌﻤﻟﺍ ﻁﻮﻄﺧ ﺰﻴﻬﺠﺗ ﺓﺩﺎﻋﺇ :ﺪﻳﺪﺟ ﺕﺎﻴﻣﻮﺳﺭ ﻞﻴﻐﺸﺗ ﺞﻣﺎﻧﺮﺑ"},
    {"New shader list: preparing what changed", "ﺮﻴﻐﺗ ﺎﻣ ﺰﻴﻬﺠﺗ :ﺓﺪﻳﺪﺟ ﺕﻼﻠﻈﻣ ﺔﻤﺋﺎﻗ"},
    {"Reading the saved shaders from the disk", "ﺹﺮﻘﻟﺍ ﻦﻣ ﺔﻇﻮﻔﺤﻤﻟﺍ ﺕﻼﻠﻈﻤﻟﺍ ﺓﺀﺍﺮﻗ ﺭﺎﺟ"},
    {"Port update: preparing the graphics again", "ﺕﺎﻴﻣﻮﺳﺮﻟﺍ ﺰﻴﻬﺠﺗ ﺓﺩﺎﻋﺇ :ﺬﻔﻨﻤﻟﺍ ﺚﻳﺪﺤﺗ"},
    {"Shaders", "ﺕﻼﻠﻈﻤﻟﺍ"},
    {"Pipelines", "ﺔﺠﻟﺎﻌﻤﻟﺍ ﻁﻮﻄﺧ"},
    {"Cache", "ﺔﺘﻗﺆﻤﻟﺍ ﺓﺮﻛﺍﺬﻟﺍ"},
    {"About %d s left", "ﺙ %d ﻲﻟﺍﻮﺣ ﻲﻘﺒﺘﻣ"},
    {"About %d min left", "ﺩ %d ﻲﻟﺍﻮﺣ ﻲﻘﺒﺘﻣ"},
    {"Almost done", "ﺀﺎﻬﺘﻧﻻﺍ ﻚﺷﻭ ﻰﻠﻋ"},
    {"Esc or Circle: start playing now", "ﻥﻵﺍ ﺐﻌﻠﻟﺍ ﺃﺪﺑﺍ :ﺓﺮﺋﺍﺪﻟﺍ ﻭﺃ Esc"},
};

/// The launcher language the screen follows when the menu has no table for it.
enum class LoadingLanguage : int { Menu, Japanese, Korean, Arabic };
inline std::atomic<int> loading_language{0}; // a LoadingLanguage

inline LoadingLanguage LoadingLanguageFor(std::string_view code) {
    const std::string_view two = code.substr(0, 2);
    if (two == "ja") return LoadingLanguage::Japanese;
    if (two == "ko") return LoadingLanguage::Korean;
    if (two == "ar") return LoadingLanguage::Arabic;
    return LoadingLanguage::Menu;
}

template <std::size_t N>
inline const char* LookupIn(const TextTable (&table)[N], const char* text) {
    for (const auto& [english, translated] : table) {
        if (english == text) return translated;
    }
    return text;
}

/// A text of the preparation screen in the language in use.
inline const char* LoadingTr(const char* text) {
    switch (LoadingLanguage(loading_language.load())) {
    case LoadingLanguage::Japanese: return LookupIn(LoadingJapanese, text);
    case LoadingLanguage::Korean: return LookupIn(LoadingKorean, text);
    case LoadingLanguage::Arabic: return LookupIn(LoadingArabic, text);
    default: return Tr(text);
    }
}

/// A count with the thousands separator of the language ("27,742", "27.742", "27 742").
inline std::string FormatCount(std::uint64_t value, std::string_view code) {
    const std::string_view two = code.substr(0, 2);
    const char* sep = ",";
    for (std::string_view dot : {"pt", "es", "de", "it", "tr", "id"}) {
        if (two == dot) sep = ".";
    }
    for (std::string_view space : {"fr", "pl", "ru", "cs"}) {
        if (two == space) sep = " ";
    }
    std::string digits = std::to_string(value), out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += sep;
        out += digits[i];
    }
    return out;
}
} // namespace SpartanUi
