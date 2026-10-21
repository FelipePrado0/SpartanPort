/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_SIGNATURE_H
#define SPARTAN_SIGNATURE_H
#include <stdint.h>
#include <string.h>

/* A hook site found by its original bytes wherever a build of the game moved the code: "??"
 * marks bytes another build may change (rip-relative displacements, rel32 branch targets).
 * tools/make_signatures.py writes them from the reference eboot.bin; scripts/patches.py does the
 * same search for the XML patches. at: the site's offset in the pattern. */
typedef struct { const char *pattern; unsigned at; } SpartanSignature;

enum { SPARTAN_SIGNATURE_MAX = 64 };

/* Bytes of text into out (-1 for "??"); the count, or 0 when the text is not a pattern. */
static inline unsigned spartanport_signature_parse(const char *text, int *out) {
    unsigned n = 0;
    while (*text) {
        if (*text == ' ') { ++text; continue; }
        if (n == SPARTAN_SIGNATURE_MAX || !text[1]) return 0;
        if (text[0] == '?' && text[1] == '?') {
            out[n++] = -1;
        } else {
            int value = 0;
            for (int i = 0; i < 2; ++i) {
                const char c = text[i];
                value = value * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -256);
            }
            if (value < 0) return 0;
            out[n++] = value;
        }
        text += 2;
    }
    return n;
}

/* The image offset of the signature's site when the pattern occurs exactly once, else UINT64_MAX. */
static inline uint64_t spartanport_signature_find(const unsigned char *image, uint64_t size, const SpartanSignature *sig) {
    int pattern[SPARTAN_SIGNATURE_MAX];
    const unsigned n = spartanport_signature_parse(sig->pattern, pattern);
    if (!n || n > size || sig->at >= n) return UINT64_MAX;
    /* Scan for the longest run of fixed bytes, then check the whole pattern around it. */
    unsigned best = 0, best_length = 0;
    for (unsigned i = 0; i < n;) {
        unsigned j = i;
        while (j < n && pattern[j] >= 0) ++j;
        if (j - i > best_length) { best = i; best_length = j - i; }
        i = j + 1;
    }
    if (!best_length) return UINT64_MAX;
    unsigned char anchor[SPARTAN_SIGNATURE_MAX];
    for (unsigned i = 0; i < best_length; ++i) anchor[i] = (unsigned char)pattern[best + i];
    uint64_t found = UINT64_MAX;
    for (uint64_t at = best; at + (n - best) <= size;) {
        const unsigned char *hit = (const unsigned char *)memchr(image + at, anchor[0], size - (n - best) - at + 1);
        if (!hit) break;
        at = (uint64_t)(hit - image);
        if (!memcmp(hit, anchor, best_length)) {
            const uint64_t start = at - best;
            unsigned i = 0;
            while (i < n && (pattern[i] < 0 || image[start + i] == pattern[i])) ++i;
            if (i == n) {
                if (found != UINT64_MAX) return UINT64_MAX;  /* more than once: ambiguous */
                found = start + sig->at;
            }
        }
        ++at;
    }
    return found;
}
#endif
