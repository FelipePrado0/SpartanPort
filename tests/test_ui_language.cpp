// The in-game menu's languages (gpu/shim/spartanport_ui_language.h): every table has the same English
// texts, nothing is left empty, a printf format keeps its conversions in order, no translation is
// another English text (help texts go through Tr twice), and the language in use switches at once.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "spartanport_ui_language.h"

using namespace SpartanUi;

// The conversions of a printf format, without flags, width or precision ("%d", "%.0s" -> "d", "s").
static std::string Conversions(std::string_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '%') continue;
        size_t j = i + 1;
        if (j < text.size() && text[j] == '%') {
            i = j;
            continue;
        }
        while (j < text.size() && std::strchr("-+#0123456789.", text[j])) ++j;
        if (j < text.size() && std::strchr("diouxXfFeEgGcsp", text[j])) out += text[j];
        i = j;
    }
    return out;
}

int main() {
    const Language& chinese_table = Languages[LanguageCount - 1];
    assert(Languages[0].code == "en" && Languages[0].count == 0 && chinese_table.code == "zh");
    std::set<std::string_view> keys;
    for (size_t i = 0; i < chinese_table.count; ++i) keys.insert(chinese_table.texts[i].first);
    assert(keys.size() == chinese_table.count); // no text twice
    for (int l = 1; l < LanguageCount; ++l) {
        const Language& table = Languages[l];
        assert(table.count == keys.size());
        std::set<std::string_view> mine;
        for (size_t i = 0; i < table.count; ++i) {
            const auto& [english, translated] = table.texts[i];
            mine.insert(english);
            assert(translated && translated[0]);
            // Only texts with a conversion are formats ("0% is no background." is not one).
            if (!Conversions(english).empty()) assert(Conversions(english) == Conversions(translated));
            if (keys.count(translated) && english != translated) {
                const char* again = Lookup(table, translated);
                if (std::strcmp(again, translated)) {
                    std::printf("%.*s: \"%s\" is also the English \"%s\"\n", int(table.code.size()), table.code.data(),
                                translated, translated);
                    assert(false);
                }
            }
        }
        assert(mine == keys);
    }

    // The launcher's codes.
    assert(FindLanguage("pt") == 1 && FindLanguage("pt-BR") == 1 && FindLanguage("ru") == 9);
    assert(FindLanguage("zh_CN") == LanguageCount - 1 && FindLanguage("ja") == 0 && FindLanguage("") == 0);

    // Switching: at once, and Chinese only with its font.
    chinese_font = false;
    assert(SetLanguage(1) == 1 && std::string(Tr("Close")) == "Fechar" && std::string(Tr("Trophy unlocked!")) == "Troféu conquistado!");
    assert(std::string(Tr("not a menu text")) == "not a menu text" && Tr(nullptr) == nullptr);
    assert(SetLanguage(9) == 9 && std::string(Tr("Close")) == "Закрыть");
    assert(SetLanguage(LanguageCount - 1) == 0 && !chinese && std::string(Tr("Close")) == "Close");
    chinese_font = true;
    assert(SetLanguage(LanguageCount - 1) == LanguageCount - 1 && chinese && std::string(Tr("Close")) == "关闭");
    assert(SetLanguage(0) == 0 && !chinese && std::string(Tr("Close")) == "Close");
    assert(SetLanguage(99) == 0);
    char text[256];
    SetLanguage(1);
    std::snprintf(text, sizeof(text), Tr("%d change%s need%s a restart (unsaved progress is lost)."), 2, "s", "");
    assert(std::string(text) == "2 alteração(ões) exige(m) reiniciar (o progresso não salvo é perdido).");
    std::printf("UI languages: %d tables of %zu texts passed\n", LanguageCount - 1, keys.size());
    return 0;
}
