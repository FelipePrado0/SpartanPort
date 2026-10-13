/* The guest's TSC: sceKernelReadTsc and sceKernelGetTscFrequency (src/runtime_kernel.c). */
#define _GNU_SOURCE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/runtime_kernel.c"

/* Neighbours of runtime_kernel.c that the clocks never call. */
int32_t *runtime_errno(void) { static int32_t value; return &value; }
void runtime_report(void) {}
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

static double seconds(uint64_t ticks, uint64_t hz) { return (double)ticks / (double)hz; }

/* The TSC and the process counter read the same instant, within 10 ms. */
static void assert_same_clock(uint64_t hz) {
    const uint64_t tsc = read_tsc(), counter = process_time_counter();
    assert(fabs(seconds(tsc, hz) - seconds(counter, 1000000000)) < 0.01);
}

int main(void) {
    const uint64_t hz = tsc_frequency();
    /* One clock for the guest: the TSC counts on sceKernelGetProcessTimeCounter's clock. A core
       whose raw TSC is offset (3.9 s on the Xeon E5-2680 v4 of #42) would otherwise make the
       game jump forward in time whenever a thread moved to it. */
    assert_same_clock(hz);
    /* It still measures real time: a 50 ms nap reads as 50 ms, give or take scheduling. */
    const uint64_t before = read_tsc();
    sleep_ns(50000000);
    const double slept = seconds(read_tsc() - before, hz);
    assert(slept > 0.04 && slept < 0.5);
#ifdef _WIN32
    /* Reads stay on the process clock and never go backwards while the thread hops cores. */
    DWORD_PTR process_mask = 0, system_mask = 0;
    assert(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask));
    uint64_t last = read_tsc();
    for (unsigned core = 0; core < 64; ++core) {
        if (!(process_mask >> core & 1)) continue;
        assert(SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << core));
        sleep_ns(1000000);
        const uint64_t now = read_tsc();
        assert(now >= last);
        last = now;
        assert_same_clock(hz);
    }
#endif
    puts("tsc-clock-test ok");
    return 0;
}
