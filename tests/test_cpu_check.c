/* Host CPU check against the PS4 Jaguar instruction set (src/cpu_check.c). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/cpu_check.c"

enum {
    L1_SSE42 = 1u << 20, L1_MOVBE = 1u << 22, L1_POPCNT = 1u << 23, L1_AVX = 1u << 28,
    L1_F16C = 1u << 29, L7_BMI1 = 1u << 3, E1_LZCNT = 1u << 5,
};
static const uint32_t LEAF1_ALL = L1_SSE42 | L1_MOVBE | L1_POPCNT | L1_AVX | L1_F16C;

int main(void) {
    char out[128];
    /* A Jaguar-class host lacks nothing. */
    assert(cpu_missing_fatal((CpuFeatureWords){LEAF1_ALL, L7_BMI1, E1_LZCNT}, out, sizeof out) == 0 && !strcmp(out, ""));
    assert(cpu_missing_rewritable((CpuFeatureWords){LEAF1_ALL, L7_BMI1, E1_LZCNT}, out, sizeof out) == 0 && !strcmp(out, ""));
    /* Sandy Bridge (#33): AVX, but no MOVBE, F16C, BMI1 or LZCNT, all rewritable. */
    assert(cpu_missing_fatal((CpuFeatureWords){L1_SSE42 | L1_POPCNT | L1_AVX, 0, 0}, out, sizeof out) == 0);
    assert(cpu_missing_rewritable((CpuFeatureWords){L1_SSE42 | L1_POPCNT | L1_AVX, 0, 0}, out, sizeof out)
           == (CPU_SET_MOVBE | CPU_SET_F16C | CPU_SET_BMI1 | CPU_SET_LZCNT));
    assert(!strcmp(out, "MOVBE, F16C, BMI1, LZCNT"));
    /* Ivy Bridge (#66): F16C arrived, the rest did not. */
    assert(cpu_missing_rewritable((CpuFeatureWords){L1_SSE42 | L1_POPCNT | L1_AVX | L1_F16C, 0, 0}, out, sizeof out)
           == (CPU_SET_MOVBE | CPU_SET_BMI1 | CPU_SET_LZCNT));
    assert(!strcmp(out, "MOVBE, BMI1, LZCNT"));
    /* Nehalem: no AVX, which is fatal. */
    assert(cpu_missing_fatal((CpuFeatureWords){L1_SSE42 | L1_POPCNT, 0, 0}, out, sizeof out) == 1 && !strcmp(out, "AVX"));
    /* A short buffer is cut, never overrun; the count stays complete. */
    memset(out, 'x', sizeof out);
    assert(cpu_missing_fatal((CpuFeatureWords){0, 0, 0}, out, 8) == 3 && strlen(out) < 8);
    /* The words come from CPUID: the machine running this test is newer than 2013. */
    assert(cpu_missing_fatal(cpu_feature_words(), out, sizeof out) == 0 && cpu_missing_rewritable(cpu_feature_words(), out, sizeof out) == 0);
    puts("cpu-check-test ok");
    return 0;
}
