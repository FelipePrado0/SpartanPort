/* The guest code (eboot.bin, libc.prx) is compiled for the PS4's Jaguar CPU and runs on the host
 * CPU as it is. A host without one of Jaguar's instruction sets stops with an illegal
 * instruction somewhere inside the game (#33, #66); this check names the missing sets at start. */
#ifndef GOW3_CPU_CHECK_H
#define GOW3_CPU_CHECK_H
#include <stddef.h>
#include <stdint.h>

/* CPUID words: leaf 1 ECX, leaf 7 subleaf 0 EBX, leaf 0x80000001 ECX. */
typedef struct { uint32_t leaf1_ecx, leaf7_ebx, ext1_ecx; } CpuFeatureWords;

CpuFeatureWords cpu_feature_words(void);
/* Names of the Jaguar instruction sets the host lacks, comma separated in out; returns how many. */
int cpu_missing_features(CpuFeatureWords words, char *out, size_t size);

#endif
