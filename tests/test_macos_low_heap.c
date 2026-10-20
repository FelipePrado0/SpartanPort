/* Low guest-visible allocations, including concurrent first use on Darwin. */
#include "compat_mac.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

static void *allocate_on_thread(void *unused) {
    (void)unused;
    for (int i = 0; i < 1000; ++i) {
        unsigned char *p = calloc(128, 1);
        assert(p && (uintptr_t)p >= SPARTAN_LOW_HEAP_MIN && (uintptr_t)p < SPARTAN_LOW_HEAP_MAX);
        for (int j = 0; j < 128; ++j) assert(p[j] == 0);
        memset(p, 0xab, 128);
        p = realloc(p, 8192);
        assert(p);
        for (int j = 0; j < 128; ++j) assert(p[j] == 0xab);
        free(p);
    }
    return NULL;
}

int main(void) {
    pthread_t workers[8];
    for (int i = 0; i < 8; ++i) assert(!pthread_create(&workers[i], NULL, allocate_on_thread, NULL));
    for (int i = 0; i < 8; ++i) assert(!pthread_join(workers[i], NULL));
    void *p = aligned_alloc(4096, 8192);
    assert(p && !((uintptr_t)p & 4095));
    free(p);
    assert(!calloc(SIZE_MAX, 2));
    assert(!aligned_alloc(SIZE_MAX - 15, 32));
    puts("macOS low heap OK");
    return 0;
}
