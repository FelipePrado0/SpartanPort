/* Loader side of the ISA rewrite (#33, #66): reads a G3ISA1 table (scripts/isa_rewrite.py), builds
 * one machine-code stub per site in an area next to the image and patches each site with a jmp
 * into its stub (or an int3). The stub saves the guest's registers, calls isa_emulate and jumps
 * back. Runs at load time, before the image is protected; libc is fine here. */
#ifndef SPARTAN_ISA_REWRITE_H
#define SPARTAN_ISA_REWRITE_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "isa_emulate.h"
#include "cpu_check.h"

typedef struct {
    IsaSite *sites; uint64_t *hits;      /* hits[i] belongs to sites[i] */
    uint32_t count, unsupported;
    uint64_t image_size; uint8_t sha256[32];
    unsigned char *image, *stubs;        /* set by isa_rewrite_apply */
} IsaTable;
/* rewritten = jmp + relocated + int3; skipped and unsupported_needed are outside it. */
typedef struct { uint32_t rewritten, jmp, relocated, int3, skipped, unsupported_needed; } IsaReport;
enum { ISA_STUB_BYTES = 256 };

/* Parses a G3ISA1 file (spec 4.4). NULL and one stderr line ("ISA: <reason>") when invalid. */
IsaTable *isa_table_load(const char *path);
/* 0 when size and sha256 match the image. */
int isa_table_matches(const IsaTable *t, const unsigned char *image, uint64_t size);
size_t isa_stub_area_size(const IsaTable *t);           /* count * ISA_STUB_BYTES */
unsigned isa_site_set(const IsaSite *s);                 /* CPU_SET_* of the kind */
/* Rewrites the sites whose set is in `sets` (spec 5.4). Writes nothing and returns
 * unsupported_needed > 0 when a needed site is UNSUPPORTED. `stubs` has isa_stub_area_size bytes, RW.
 * A JMP_RELOCATE site whose next instruction no longer matches becomes INT3 (its mode in t changes);
 * each skipped or downgraded site gets one stderr line. */
IsaReport isa_rewrite_apply(IsaTable *t, unsigned char *image, unsigned char *stubs, unsigned sets);

/* int3 at `address` (the CC byte): 1 when it is a rewritten INT3 site; regs updated, *next_rip = site + its length (4).
 * 0 otherwise, with regs and *next_rip untouched. */
int isa_rewrite_trap(IsaTable *t, uintptr_t address, IsaRegs *regs, uintptr_t *next_rip);
/* Site index whose stub holds rip, or -1. */
int isa_rewrite_site_of_stub(const IsaTable *t, uintptr_t rip);
/* "ISA: most executed sites:" then up to 10 lines "  0x<offset> <mnemonic> <hits>" (hits > 0 only),
 * descending hits, ties by ascending offset. */
void isa_rewrite_report(const IsaTable *t, FILE *out);
/* "movbe", "lzcnt", "tzcnt", "andn", "bextr", "blsi", "blsmsk", "blsr", "vcvtph2ps", "vcvtps2ph"; "?" otherwise. */
const char *isa_kind_name(uint8_t kind);

#endif
