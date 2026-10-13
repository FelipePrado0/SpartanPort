#include "cpu_check.h"
#include <cpuid.h>
#include <stdio.h>

CpuFeatureWords cpu_feature_words(void) {
    CpuFeatureWords words = {0, 0, 0};
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (__get_cpuid(1, &a, &b, &c, &d)) words.leaf1_ecx = c;
    if (__get_cpuid_count(7, 0, &a, &b, &c, &d)) words.leaf7_ebx = b;
    if (__get_cpuid(0x80000001, &a, &b, &c, &d)) words.ext1_ecx = c;
    (void)a; (void)d;
    return words;
}

/* Jaguar's sets beyond the x86-64 baseline that Intel CPUs also have (SSE4A is left out: the
 * game runs on Intel). Each one arrived on Intel with Haswell at the latest. */
enum { LEAF1, LEAF7, EXT1 };
static const struct { const char *name; int word; uint32_t bit; } sets[] = {
    {"SSE4.2", LEAF1, 1u << 20}, {"POPCNT", LEAF1, 1u << 23}, {"MOVBE", LEAF1, 1u << 22},
    {"AVX", LEAF1, 1u << 28},    {"F16C", LEAF1, 1u << 29},   {"BMI1", LEAF7, 1u << 3},
    {"LZCNT", EXT1, 1u << 5},
};

int cpu_missing_features(CpuFeatureWords words, char *out, size_t size) {
    const uint32_t values[] = {words.leaf1_ecx, words.leaf7_ebx, words.ext1_ecx};
    int missing = 0;
    size_t used = 0;
    if (size) out[0] = 0;
    for (size_t i = 0; i < sizeof sets / sizeof *sets; ++i) {
        if (values[sets[i].word] & sets[i].bit) continue;
        if (used < size) {
            const int n = snprintf(out + used, size - used, "%s%s", missing ? ", " : "", sets[i].name);
            used = n < 0 ? size : used + (size_t)n;
            if (used >= size) used = size - 1;
        }
        ++missing;
    }
    return missing;
}
