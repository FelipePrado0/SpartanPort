/* isa_emulate (src/isa_emulate.c) against the host's own MOVBE, BMI1 and LZCNT instructions. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa_emulate.h"
#include "../src/cpu_check.c"

static const uint64_t EDGES[] = {0, 1, 2, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff, 0x7fffffff, 0x80000000, 0xffffffff,
                                 0x7fffffffffffffff, 0x8000000000000000, 0xffffffffffffffff};
/* ctrl for BEXTR: start and len in {0, 1, 7, 8, 15, 16, 31, 32, 63, 64, 255} crossed */
static const uint64_t BEXTR_FIELDS[] = {0, 1, 7, 8, 15, 16, 31, 32, 63, 64, 255};
enum { EDGE_COUNT = sizeof EDGES / sizeof EDGES[0], FIELD_COUNT = sizeof BEXTR_FIELDS / sizeof BEXTR_FIELDS[0] };
enum { RANDOM_INPUTS = 20000 };
static const uint64_t RAX_START = 0x1122334455667788;
static const uint64_t FLAG_STARTS[] = {0, 0xcd5};

static uint64_t random_state = 0x9e3779b97f4a7c15;
static uint64_t xorshift64(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}

/* The red zone below rsp is stepped over before pushfq. */
#define FLAGS_OUT "\n\tleaq -128(%%rsp), %%rsp\n\tpushfq\n\tpopq %[f]\n\tleaq 128(%%rsp), %%rsp"
/* insn src, dst */
#define NATIVE1(mnemonic, size) \
    __asm__(mnemonic " %" size "[s1], %" size "[d]" FLAGS_OUT : [d] "+r"(dst), [f] "=r"(flags) : [s1] "r"(src1) : "cc")
/* insn src2, src1, dst (Intel: insn dst, src1, src2) */
#define NATIVE2(mnemonic, size) \
    __asm__(mnemonic " %" size "[s2], %" size "[s1], %" size "[d]" FLAGS_OUT \
            : [d] "+r"(dst), [f] "=r"(flags) : [s1] "r"(src1), [s2] "r"(src2) : "cc")
#define BY_WIDTH(macro, mnemonic) \
    do { if (width == 32) macro(mnemonic "l", "k"); else macro(mnemonic "q", "q"); } while (0)
#define BY_WIDTH16(macro, mnemonic) \
    do { if (width == 16) macro(mnemonic "w", "w"); else BY_WIDTH(macro, mnemonic); } while (0)

/* Runs the register form natively: dst = rax, src1 = rcx, src2 = rdx. Returns the new rax. */
static uint64_t native_register(uint8_t kind, int width, uint64_t src1, uint64_t src2, uint64_t *flags_out) {
    uint64_t dst = RAX_START, flags = 0;
    switch (kind) {
    case ISA_LZCNT: BY_WIDTH16(NATIVE1, "lzcnt"); break;
    case ISA_TZCNT: BY_WIDTH16(NATIVE1, "tzcnt"); break;
    case ISA_BLSI: BY_WIDTH(NATIVE1, "blsi"); break;
    case ISA_BLSMSK: BY_WIDTH(NATIVE1, "blsmsk"); break;
    case ISA_BLSR: BY_WIDTH(NATIVE1, "blsr"); break;
    case ISA_ANDN: BY_WIDTH(NATIVE2, "andn"); break;
    case ISA_BEXTR: BY_WIDTH(NATIVE2, "bextr"); break;
    default: abort();
    }
    *flags_out = flags;
    return dst;
}

static const char *kind_name(uint8_t kind) {
    static const char *const names[] = {"?", "MOVBE_LOAD", "MOVBE_STORE", "LZCNT", "TZCNT", "ANDN", "BEXTR",
                                        "BLSI", "BLSMSK", "BLSR"};
    return kind < sizeof names / sizeof names[0] ? names[kind] : "?";
}

static void fail(uint8_t kind, int width, uint64_t a, uint64_t b, const char *what, uint64_t expected, uint64_t got) {
    printf("isa-emulate-test FAILED: %s width %d input %#" PRIx64 ", %#" PRIx64 ": %s expected %#" PRIx64
           " got %#" PRIx64 "\n", kind_name(kind), width, a, b, what, expected, got);
    exit(1);
}

static IsaSite register_site(uint8_t kind, int width) {
    IsaSite site;
    memset(&site, 0, sizeof site);
    site.kind = kind;
    site.width = (uint8_t)width;
    site.dst = 0;
    site.src1 = 1;
    site.src2 = (kind == ISA_ANDN || kind == ISA_BEXTR) ? 2 : ISA_NO_REG;
    site.mem_slot = ISA_MEM_NONE;
    site.base = site.index = ISA_NO_REG;
    site.scale = 1;
    return site;
}

static void check_register(uint8_t kind, int width, uint64_t src1, uint64_t src2, unsigned round) {
    uint64_t native_flags;
    uint64_t expected = native_register(kind, width, src1, src2, &native_flags);
    IsaSite site = register_site(kind, width);
    IsaRegs regs;
    for (int r = 0; r < 16; r++) regs.gpr[r] = 0x0101010101010101ull * (uint64_t)r;
    regs.gpr[0] = RAX_START;
    regs.gpr[1] = src1;
    regs.gpr[2] = src2;
    uint64_t flags_start = FLAG_STARTS[round & 1];
    regs.rflags = flags_start;
    isa_emulate(&site, &regs);
    uint64_t defined = isa_defined_flags(kind);
    if (regs.gpr[0] != expected) fail(kind, width, src1, src2, "rax", expected, regs.gpr[0]);
    if ((regs.rflags ^ native_flags) & defined) fail(kind, width, src1, src2, "flags", native_flags & defined, regs.rflags & defined);
    if ((regs.rflags ^ flags_start) & ~defined) fail(kind, width, src1, src2, "undefined flags", flags_start, regs.rflags);
    if (regs.gpr[1] != src1 || regs.gpr[2] != src2) fail(kind, width, src1, src2, "sources", src1 ^ src2, regs.gpr[1] ^ regs.gpr[2]);
    for (int r = 3; r < 16; r++)
        if (regs.gpr[r] != 0x0101010101010101ull * (uint64_t)r) fail(kind, width, src1, src2, "other register", (uint64_t)r, regs.gpr[r]);
}

static void check_registers(void) {
    static const uint8_t kinds[] = {ISA_LZCNT, ISA_TZCNT, ISA_ANDN, ISA_BEXTR, ISA_BLSI, ISA_BLSMSK, ISA_BLSR};
    static const int widths[] = {16, 32, 64};
    unsigned round = 0;
    for (size_t k = 0; k < sizeof kinds; k++) {
        uint8_t kind = kinds[k];
        for (size_t w = 0; w < 3; w++) {
            int width = widths[w];
            if (width == 16 && kind != ISA_LZCNT && kind != ISA_TZCNT) continue;   /* VEX forms: 32/64 only */
            for (int a = 0; a < EDGE_COUNT; a++) {
                if (kind == ISA_BEXTR) {
                    for (int s = 0; s < FIELD_COUNT; s++)
                        for (int l = 0; l < FIELD_COUNT; l++)
                            check_register(kind, width, EDGES[a], BEXTR_FIELDS[s] | BEXTR_FIELDS[l] << 8, round++);
                } else {
                    for (int b = 0; b < EDGE_COUNT; b++) check_register(kind, width, EDGES[a], EDGES[b], round++);
                }
            }
            for (int i = 0; i < RANDOM_INPUTS; i++) {
                uint64_t a = xorshift64(), b = xorshift64();
                if (i & 1) a >>= a & 63;   /* leading zeros of every count */
                if (i & 2) a <<= b & 63;   /* trailing zeros of every count */
                if (kind == ISA_BEXTR && (i & 4)) b &= 0x7f7f;
                check_register(kind, width, a, b, round++);
            }
        }
    }
}

static IsaSite memory_site(uint8_t kind, int width) {
    IsaSite site = register_site(kind, width);
    site.base = 1;
    site.disp = 8;
    if (kind == ISA_MOVBE_LOAD) { site.src1 = ISA_NO_REG; site.mem_slot = ISA_MEM_SRC; }
    else { site.dst = ISA_NO_REG; site.src1 = 2; site.mem_slot = ISA_MEM_DST; }
    site.src2 = ISA_NO_REG;
    return site;
}

static void check_movbe(void) {
    static const int widths[] = {16, 32, 64};
    for (size_t w = 0; w < 3; w++) {
        int width = widths[w];
        for (int i = 0; i < RANDOM_INPUTS; i++) {
            unsigned char native_buffer[32], emulated_buffer[32];
            for (int j = 0; j < 32; j += 8) {
                uint64_t r = xorshift64();
                memcpy(native_buffer + j, &r, 8);
            }
            memcpy(emulated_buffer, native_buffer, sizeof emulated_buffer);
            uint64_t value = xorshift64(), dst = RAX_START;
            unsigned char *at = native_buffer + 8;
            /* load: rax <- [rcx + 8] */
            if (width == 16) __asm__("movbew %[m], %w[d]" : [d] "+r"(dst) : [m] "m"(*(uint16_t (*)[1])at));
            else if (width == 32) __asm__("movbel %[m], %k[d]" : [d] "+r"(dst) : [m] "m"(*(uint32_t (*)[1])at));
            else __asm__("movbeq %[m], %q[d]" : [d] "+r"(dst) : [m] "m"(*(uint64_t (*)[1])at));
            IsaSite site = memory_site(ISA_MOVBE_LOAD, width);
            IsaRegs regs = {.rflags = 0xcd5};
            regs.gpr[0] = RAX_START;
            regs.gpr[1] = (uint64_t)(uintptr_t)emulated_buffer;
            isa_emulate(&site, &regs);
            if (regs.gpr[0] != dst) fail(ISA_MOVBE_LOAD, width, regs.gpr[1], 0, "rax", dst, regs.gpr[0]);
            if (regs.rflags != 0xcd5) fail(ISA_MOVBE_LOAD, width, 0, 0, "flags", 0xcd5, regs.rflags);
            /* store: [rcx + 8] <- rdx */
            if (width == 16) __asm__("movbew %w[s], %[m]" : [m] "=m"(*(uint16_t (*)[1])at) : [s] "r"(value));
            else if (width == 32) __asm__("movbel %k[s], %[m]" : [m] "=m"(*(uint32_t (*)[1])at) : [s] "r"(value));
            else __asm__("movbeq %q[s], %[m]" : [m] "=m"(*(uint64_t (*)[1])at) : [s] "r"(value));
            site = memory_site(ISA_MOVBE_STORE, width);
            regs.gpr[2] = value;
            isa_emulate(&site, &regs);
            for (int j = 0; j < 32; j++)
                if (native_buffer[j] != emulated_buffer[j])
                    fail(ISA_MOVBE_STORE, width, value, (uint64_t)j, "byte", native_buffer[j], emulated_buffer[j]);
            if (regs.gpr[0] != dst || regs.gpr[2] != value) fail(ISA_MOVBE_STORE, width, value, 0, "rax", dst, regs.gpr[0]);
        }
    }
    /* Base, scaled index and negative displacement: rcx + rbx * 4 - 8. */
    unsigned char buffer[32];
    for (int j = 0; j < 32; j++) buffer[j] = (unsigned char)(j * 17 + 3);
    uint64_t dst = RAX_START;
    __asm__("movbel %[m], %k[d]" : [d] "+r"(dst) : [m] "m"(*(uint32_t (*)[1])(buffer + 12)));
    IsaSite site = memory_site(ISA_MOVBE_LOAD, 32);
    site.index = 3;
    site.scale = 4;
    site.disp = -8;
    IsaRegs regs = {.rflags = 0};
    regs.gpr[1] = (uint64_t)(uintptr_t)buffer;
    regs.gpr[3] = 5;
    isa_emulate(&site, &regs);
    if (regs.gpr[0] != dst) fail(ISA_MOVBE_LOAD, 32, 12, 0, "rax (indexed)", dst, regs.gpr[0]);
}

/* BEXTR r32, [rip + disp], r32: the address is image + disp. */
static void check_rip_relative(void) {
    static unsigned char image[256];
    enum { BUFFER_OFFSET = 100 };
    isa_emulate_set_image(image);
    for (int i = 0; i < RANDOM_INPUTS; i++) {
        uint32_t word = (uint32_t)xorshift64();
        uint64_t ctrl = xorshift64() & 0x3f3f, dst = RAX_START, flags = 0;
        memcpy(image + BUFFER_OFFSET, &word, 4);
        __asm__("bextrl %k[c], %[m], %k[d]" FLAGS_OUT
                : [d] "+r"(dst), [f] "=r"(flags) : [c] "r"(ctrl), [m] "m"(*(uint32_t (*)[1])(image + BUFFER_OFFSET)) : "cc");
        IsaSite site = register_site(ISA_BEXTR, 32);
        site.src1 = ISA_NO_REG;
        site.mem_slot = ISA_MEM_SRC;
        site.mem_flags = ISA_MEM_RIP_RELATIVE;
        site.disp = BUFFER_OFFSET;
        IsaRegs regs = {.rflags = FLAG_STARTS[i & 1]};
        regs.gpr[0] = RAX_START;
        regs.gpr[2] = ctrl;
        isa_emulate(&site, &regs);
        uint64_t defined = isa_defined_flags(ISA_BEXTR);
        if (regs.gpr[0] != dst) fail(ISA_BEXTR, 32, word, ctrl, "rax (rip)", dst, regs.gpr[0]);
        if ((regs.rflags ^ flags) & defined) fail(ISA_BEXTR, 32, word, ctrl, "flags (rip)", flags & defined, regs.rflags & defined);
    }
}

/* ANDN r, r, [rbx + 8] (32, 64) and BEXTR r32, [rbx - 4], r32: base + displacement memory sources. */
static void check_memory_sources(void) {
    for (int i = 0; i < RANDOM_INPUTS; i++) {
        unsigned char buffer[32];
        for (int j = 0; j < 32; j += 8) {
            uint64_t r = xorshift64();
            memcpy(buffer + j, &r, 8);
        }
        uint64_t src1 = xorshift64(), ctrl = xorshift64() & 0x3f3f, flags_start = FLAG_STARTS[i & 1];
        uint64_t defined = isa_defined_flags(ISA_ANDN);
        for (int width = 32; width <= 64; width += 32) {
            uint64_t dst = RAX_START, flags = 0;
            if (width == 32)
                __asm__("andnl %[m], %k[s1], %k[d]" FLAGS_OUT
                        : [d] "+r"(dst), [f] "=r"(flags) : [s1] "r"(src1), [m] "m"(*(uint32_t (*)[1])(buffer + 8)) : "cc");
            else
                __asm__("andnq %[m], %q[s1], %q[d]" FLAGS_OUT
                        : [d] "+r"(dst), [f] "=r"(flags) : [s1] "r"(src1), [m] "m"(*(uint64_t (*)[1])(buffer + 8)) : "cc");
            IsaSite site = register_site(ISA_ANDN, width);
            site.src2 = ISA_NO_REG;
            site.mem_slot = ISA_MEM_SRC;
            site.base = 3;
            site.disp = 8;
            IsaRegs regs = {.rflags = flags_start};
            regs.gpr[0] = RAX_START;
            regs.gpr[1] = src1;
            regs.gpr[3] = (uint64_t)(uintptr_t)buffer;
            isa_emulate(&site, &regs);
            if (regs.gpr[0] != dst) fail(ISA_ANDN, width, src1, 0, "rax (memory)", dst, regs.gpr[0]);
            if ((regs.rflags ^ flags) & defined) fail(ISA_ANDN, width, src1, 0, "flags (memory)", flags & defined, regs.rflags & defined);
        }
        uint64_t dst = RAX_START, flags = 0;
        __asm__("bextrl %k[c], %[m], %k[d]" FLAGS_OUT
                : [d] "+r"(dst), [f] "=r"(flags) : [c] "r"(ctrl), [m] "m"(*(uint32_t (*)[1])(buffer + 12)) : "cc");
        IsaSite site = register_site(ISA_BEXTR, 32);
        site.src1 = ISA_NO_REG;
        site.mem_slot = ISA_MEM_SRC;
        site.base = 3;
        site.disp = -4;
        IsaRegs regs = {.rflags = flags_start};
        regs.gpr[0] = RAX_START;
        regs.gpr[2] = ctrl;
        regs.gpr[3] = (uint64_t)(uintptr_t)(buffer + 16);
        isa_emulate(&site, &regs);
        defined = isa_defined_flags(ISA_BEXTR);
        if (regs.gpr[0] != dst) fail(ISA_BEXTR, 32, ctrl, 0, "rax (memory)", dst, regs.gpr[0]);
        if ((regs.rflags ^ flags) & defined) fail(ISA_BEXTR, 32, ctrl, 0, "flags (memory)", flags & defined, regs.rflags & defined);
    }
}

int main(void) {
    char missing[128];
    if (cpu_missing_rewritable(cpu_feature_words(), missing, sizeof missing) & (CPU_SET_MOVBE | CPU_SET_BMI1 | CPU_SET_LZCNT)) {
        puts("isa-emulate-test skipped: CPU lacks BMI1, LZCNT or MOVBE");
        return 0;
    }
    if (isa_defined_flags(ISA_MOVBE_LOAD) != 0 || isa_defined_flags(ISA_LZCNT) != (ISA_CF | ISA_ZF)
        || isa_defined_flags(ISA_BEXTR) != (ISA_ZF | ISA_CF | ISA_OF)
        || isa_defined_flags(ISA_BLSR) != (ISA_SF | ISA_ZF | ISA_CF | ISA_OF)) {
        puts("isa-emulate-test FAILED: isa_defined_flags table");
        return 1;
    }
    check_registers();
    check_movbe();
    check_rip_relative();
    check_memory_sources();
    puts("isa-emulate-test ok");
    return 0;
}
