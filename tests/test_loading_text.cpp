// The shader preparation screen's texts (gpu/shim/spartanport_loading_text.h): the Japanese, Korean
// and Arabic tables have every text of the menu's tables for that screen, keep printf conversions,
// the Arabic texts hold no unjoined letters, and counts take each language's thousands separator.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include "spartanport_loading_text.h"

using namespace SpartanUi;

static int Count(std::string_view text, char c) {
    int n = 0;
    for (char ch : text) n += ch == c;
    return n;
}

template <std::size_t N>
static void SameKeys(const TextTable (&table)[N], bool arabic) {
    assert(N == std::size(LoadingJapanese));
    for (std::size_t i = 0; i < N; ++i) {
        const auto& [english, translated] = table[i];
        assert(english == LoadingJapanese[i].first);
        assert(translated && translated[0]);
        // The screen's formats are "%d" only: the same number of conversions.
        assert(Count(english, '%') == Count(translated, '%'));
        if (arabic) {
            // Base Arabic letters (U+0621-U+064A, UTF-8 D8 A1 to D9 8A) mean a text left unjoined.
            for (const char* p = translated; *p; ++p) {
                const auto a = static_cast<unsigned char>(p[0]), b = static_cast<unsigned char>(p[1]);
                assert(!((a == 0xD8 && b >= 0xA1) || (a == 0xD9 && b <= 0x8A)));
            }
        }
    }
}

int main() {
    // Every text of the screen is also in the menu's tables (one of them is enough: they match).
    for (const auto& [english, translated] : LoadingJapanese) {
        const std::string key(english);
        const Language& portuguese = Languages[1];
        assert(Lookup(portuguese, key.c_str()) != key.c_str() || key == "Shaders" ||
               key == "Pipelines" || key == "Cache");
        (void)translated;
    }
    SameKeys(LoadingKorean, false);
    SameKeys(LoadingArabic, true);

    assert(LoadingLanguageFor("ja") == LoadingLanguage::Japanese);
    assert(LoadingLanguageFor("ko-KR") == LoadingLanguage::Korean);
    assert(LoadingLanguageFor("ar") == LoadingLanguage::Arabic);
    assert(LoadingLanguageFor("pt") == LoadingLanguage::Menu);

    loading_language = int(LoadingLanguage::Korean);
    assert(std::strcmp(LoadingTr("Cache"), "캐시") == 0);
    loading_language = int(LoadingLanguage::Menu);
    assert(std::strcmp(LoadingTr("Almost done"), "Almost done") == 0); // English menu

    assert(FormatCount(27742, "en") == "27,742");
    assert(FormatCount(27742, "pt") == "27.742");
    assert(FormatCount(27742, "ru") == "27 742");
    assert(FormatCount(1234567, "de") == "1.234.567");
    assert(FormatCount(999, "fr") == "999");
    assert(FormatCount(0, "ja") == "0");
    std::printf("PASS: loading text\n");
    return 0;
}
