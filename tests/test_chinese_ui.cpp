// Headless ImGui/CJK tests: no game dump, GPU or Vulkan device required.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "spartanport_ui_font.h"

int main() {
    using namespace SpartanUi;
    assert(WantsChinese("zh") && WantsChinese("zh-CN") && WantsChinese("zh_CN"));
    assert(!WantsChinese("en") && !WantsChinese("ru") && !WantsChinese("zhx"));
    chinese = false;
    assert(std::strcmp(Tr("Close"), "Close") == 0);
    chinese = true;
    assert(std::strcmp(Tr("Close"), "关闭") == 0);
    assert(std::strcmp(Tr("Automatic QTE"), "自动完成 QTE") == 0);
    assert(std::strcmp(Tr("Fullscreen"), "全屏") == 0);
    assert(std::strcmp(Tr("cheats"), "cheats") == 0); // Internal directory, not a UI label.
    assert(std::strcmp(Tr("##value"), "##value") == 0);
    assert(Tr(nullptr) == nullptr);
    char text[256];
    std::snprintf(text, sizeof(text), Tr("%d change%s need%s a restart (unsaved progress is lost)."), 2, "s", "s");
    assert(std::strcmp(text, "2 项更改需要重启（未保存进度会丢失）。") == 0);

    ImGui::CreateContext();
    auto* atlas = ImGui::GetIO().Fonts;
    ImFontConfig base_config;
    base_config.SizePixels = 18.0f;
    atlas->AddFontDefault(&base_config);
    assert(!ConfigureChineseFont(atlas, "zh", {}));
    assert(!chinese && std::strcmp(Tr("Close"), "Close") == 0);
    const auto system_font = FindChineseFont();
    assert(!ConfigureChineseFont(atlas, "en", system_font));
    assert(!chinese);
    if (!system_font.path.empty()) {
        assert(ConfigureChineseFont(atlas, "zh", system_font));
        assert(chinese);
        assert(atlas->Build());
        for (ImWchar c : {ImWchar(0x5173), ImWchar(0x95ed), ImWchar(0x6e38), ImWchar(0x620f)}) {
            assert(atlas->Fonts[0]->IsGlyphInFont(c)); // 关闭 / 游戏
        }
        std::puts("CJK system font merge and glyph checks passed");
    } else {
        std::puts("No system CJK font: English fallback passed; glyph checks skipped");
    }
    ImGui::DestroyContext();
    std::puts("Chinese UI tests passed");
}
