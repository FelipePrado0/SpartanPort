/* The PS4 address range when something else in the process already holds part of it (#1075,
 * src/runtime_memory.c): usually a DLL another program loads into every process. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include "../src/runtime_memory.c"

/* Neighbours of runtime_memory.c that these mappings never call. */
int32_t *runtime_errno(void) { static int32_t value; return &value; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

int main(void) {
    /* 64 MiB held 2 GiB into the range before the game reserves it. */
    const uintptr_t taken = USER_MIN + (UINT64_C(2) << 30), taken_end = taken + (UINT64_C(64) << 20);
    const uint64_t grain = UINT64_C(0x10000), size = UINT64_C(1) << 20;
    assert(VirtualAlloc((void *)taken, taken_end - taken, MEM_RESERVE, PAGE_NOACCESS) == (void *)taken);
    runtime_memory_reserve();
    int64_t phys = -1;
    assert(!direct_allocate(0, (int64_t)POOL_SIZE, size, 0, 0, &phys));
    /* A mapping that would cross the held part is placed after it. */
    void *at = (void *)(taken - grain);
    assert(!direct_map(&at, size, 3, 0, phys, 0));
    assert((uintptr_t)at >= taken_end);
    memset(at, 0x5a, size);
    /* A fixed mapping over it fails, as on a full PS4 range; right before it, one maps. */
    void *fixed = (void *)taken;
    assert(direct_map(&fixed, grain, 3, MAP_FIXED_FLAG, phys, 0) == NO_MEMORY);
    fixed = (void *)(taken - grain);
    assert(!direct_map(&fixed, grain, 3, MAP_FIXED_FLAG, phys, 0) && fixed == (void *)(taken - grain));
    assert(*(volatile unsigned char *)fixed == 0x5a);
    /* The held memory is left as it was. */
    MEMORY_BASIC_INFORMATION m;
    assert(VirtualQuery((void *)taken, &m, sizeof m) && m.AllocationBase == (void *)taken);
    assert(m.State == MEM_RESERVE && m.Type == MEM_PRIVATE && m.RegionSize == taken_end - taken);
    puts("memory-reserve-test ok");
    return 0;
}
