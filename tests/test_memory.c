/* PS4 memory manager contracts (src/runtime_memory.c): a flexible mapping split by a partial
 * protection change keeps each half's own physical pages (releasing one half leaves the other's
 * contents and pages alone), and a fixed map over mapped memory retires the GPU's view of the
 * old mapping while it is still in place, before the new one arrives.
 * Linux: build.sh --test (out/memory-test). */
#include "../src/runtime_memory.c"

/* runtime.c pieces runtime_memory.c uses. */
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }
static __thread int32_t errno_slot;
int32_t *runtime_errno(void) { return &errno_slot; }

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

/* The GPU's view of the mappings: every notification, in order. */
enum { EVENT_MAP, EVENT_UNMAP, EVENT_INVALIDATE };
typedef struct { int kind; uintptr_t address; uint64_t size; } Event;
static Event events[256];
static size_t event_count;
static void note(int kind, uintptr_t address, uint64_t size) { if (event_count < 256) events[event_count++] = (Event){kind, address, size}; }
static void on_map(uintptr_t address, uint64_t size) { note(EVENT_MAP, address, size); }
/* With gpu_readback set, an unmap writes gpu_data back to the guest, as the GPU's unmap does with
 * GPU-written buffers (readbacks on). */
static unsigned char gpu_data[2 * PAGE];
static int gpu_readback;
/* With release_size set, an unmap releases that direct memory, as another thread could while the
 * GPU's unmap runs without the lock. */
static uint64_t release_start, release_size;
static void on_unmap(uintptr_t address, uint64_t size) {
    note(EVENT_UNMAP, address, size);
    if (gpu_readback) runtime_memory_write_backing(address, gpu_data, size < sizeof(gpu_data) ? size : sizeof(gpu_data));
    if (release_size) { CHECK(direct_release(release_start, release_size) == 0); release_size = 0; }
}
static void on_invalidate(uintptr_t address, uint64_t size) { note(EVENT_INVALIDATE, address, size); }

static const Vma *vma_at(uintptr_t address) {
    size_t i = vma_index(address);
    return i < vma_count && vmas[i].start <= address ? &vmas[i] : NULL;
}

/* A flexible mapping split by mprotect: each half keeps its own physical pages. */
static void flexible_split_keeps_each_half(void) {
    void *p = NULL;
    CHECK(map_flexible(&p, 4 * PAGE, 3, 0) == 0 && p);
    unsigned char *m = p;
    for (unsigned i = 0; i < 4; ++i) memset(m + i * PAGE, 0x11 * (i + 1), PAGE);
    CHECK(kernel_mprotect(m + 2 * PAGE, 2 * PAGE, 1) == 0); /* read-only right half: the VMA splits */
    const Vma *left = vma_at((uintptr_t)m), *right = vma_at((uintptr_t)m + 2 * PAGE);
    CHECK(left && right && left != right);
    CHECK(left->end == (uintptr_t)m + 2 * PAGE && right->start == (uintptr_t)m + 2 * PAGE);
    CHECK(right->phys == left->phys + 2 * PAGE); /* was the left half's offset */
    CHECK(release_flexible(m + 2 * PAGE, 2 * PAGE) == 0);
    /* The left half is intact: releasing the right half freed (and zeroed) its own pages. */
    CHECK(m[0] == 0x11 && m[PAGE - 1] == 0x11 && m[PAGE] == 0x22 && m[2 * PAGE - 1] == 0x22);
    /* A new flexible mapping gets other pages, never the left half's. */
    void *q = NULL;
    CHECK(map_flexible(&q, 2 * PAGE, 3, 0) == 0 && q);
    memset(q, 0xEE, 2 * PAGE);
    CHECK(m[0] == 0x11 && m[PAGE] == 0x22);
    CHECK(release_flexible(q, 2 * PAGE) == 0);
    CHECK(release_flexible(m, 2 * PAGE) == 0);
}

/* A fixed map over mapped memory: the GPU hears the old mapping is gone, then the new one. */
static void fixed_map_replaces_for_the_gpu(void) {
    int64_t phys = -1;
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 2 * PAGE, PAGE, 0, &phys) == 0 && phys >= 0);
    void *p = NULL;
    CHECK(direct_map(&p, 2 * PAGE, 3, 0, phys, 0) == 0 && p);
    memset(p, 0x5A, 2 * PAGE);
    const size_t before = event_count, replaced = fixed_replaced;
    void *again = p;
    CHECK(direct_map(&again, 2 * PAGE, 3, MAP_FIXED_FLAG, phys, 0) == 0 && again == p);
    CHECK(fixed_replaced == replaced + 1);
    CHECK(event_count == before + 2);
    CHECK(events[before].kind == EVENT_UNMAP && events[before].address == (uintptr_t)p && events[before].size == 2 * PAGE);
    CHECK(events[before + 1].kind == EVENT_MAP && events[before + 1].address == (uintptr_t)p && events[before + 1].size == 2 * PAGE);
    CHECK(((unsigned char *)p)[0] == 0x5A); /* the same physical memory */
    CHECK(direct_unmap(p, 2 * PAGE) == 0);
    CHECK(direct_release((uint64_t)phys, 2 * PAGE) == 0);
}

/* A fixed map over other physical pages: the GPU's readback of the old contents lands in the
 * old pages, not in the new mapping. A fixed map its own checks refuse leaves the GPU's view
 * alone (the GPU's unmap drops images and pending readbacks), and one that fails after the GPU
 * was told gives the GPU back what stays mapped. */
static void fixed_map_retires_before_replacing(void) {
    int64_t phys = -1;
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 4 * PAGE, PAGE, 0, &phys) == 0 && phys >= 0);
    const uint64_t old_pages = (uint64_t)phys, new_pages = (uint64_t)phys + 2 * PAGE;
    void *p = NULL;
    CHECK(direct_map(&p, 2 * PAGE, 3, 0, (int64_t)old_pages, 0) == 0 && p);
    unsigned char *m = p;
    memset(m, 0x11, 2 * PAGE);
    memset(backing_base + new_pages, 0x22, 2 * PAGE);
    memset(gpu_data, 0xEE, sizeof(gpu_data));
    gpu_readback = 1;
    void *again = p;
    CHECK(direct_map(&again, 2 * PAGE, 3, MAP_FIXED_FLAG, (int64_t)new_pages, 0) == 0 && again == p);
    gpu_readback = 0;
    CHECK(m[0] == 0x22 && m[2 * PAGE - 1] == 0x22); /* the new contents survive */
    CHECK(backing_base[old_pages] == 0xEE && backing_base[old_pages + 2 * PAGE - 1] == 0xEE);

    const uint64_t unallocated = (uint64_t)phys + 4 * PAGE;
    CHECK(!direct_range_allocated(unallocated, 2 * PAGE));
    const size_t before = event_count;
    void *bad = p;
    CHECK(direct_map(&bad, 2 * PAGE, 3, MAP_FIXED_FLAG, (int64_t)unallocated, 0) != 0);
    CHECK(event_count == before);
    CHECK(vma_at((uintptr_t)p) && vma_at((uintptr_t)p)->phys == new_pages && m[0] == 0x22);

    int64_t gone = -1;
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 2 * PAGE, PAGE, 0, &gone) == 0 && gone >= 0);
    release_start = (uint64_t)gone; release_size = 2 * PAGE;
    const size_t raced = event_count;
    bad = p;
    CHECK(direct_map(&bad, 2 * PAGE, 3, MAP_FIXED_FLAG, gone, 0) != 0);
    CHECK(release_size == 0 && event_count == raced + 2);
    CHECK(events[raced].kind == EVENT_UNMAP && events[raced].address == (uintptr_t)p && events[raced].size == 2 * PAGE);
    CHECK(events[raced + 1].kind == EVENT_MAP && events[raced + 1].address == (uintptr_t)p && events[raced + 1].size == 2 * PAGE);
    CHECK(vma_at((uintptr_t)p) && vma_at((uintptr_t)p)->phys == new_pages && m[0] == 0x22);
    CHECK(direct_unmap(p, 2 * PAGE) == 0);
    CHECK(direct_release((uint64_t)phys, 4 * PAGE) == 0);
}

/* A batch runs its entries in order and stops at the first failure: a fixed map its checks refuse,
 * and the maps after it, leave the GPU's view of what they would replace alone. */
static void batch_retires_only_maps_that_run(void) {
    int64_t phys = -1;
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 4 * PAGE, PAGE, 0, &phys) == 0 && phys >= 0);
    void *a = NULL, *b = NULL;
    CHECK(direct_map(&a, PAGE, 3, 0, phys, 0) == 0 && a);
    CHECK(direct_map(&b, PAGE, 3, 0, phys + PAGE, 0) == 0 && b);
    const size_t before = event_count;
    BatchEntry entries[3] = {
        {.start = a, .offset = phys + 2 * PAGE, .length = PAGE, .prot = 3, .operation = 0},
        {.start = b, .offset = phys + 4 * PAGE, .length = PAGE, .prot = 3, .operation = 0}, /* unallocated */
        {.start = a, .offset = phys + 3 * PAGE, .length = PAGE, .prot = 3, .operation = 0},
    };
    int processed = -1;
    CHECK(batch_map(entries, 3, &processed) != 0 && processed == 1);
    CHECK(event_count == before + 2); /* the first entry only: a's old view goes, the new one comes */
    CHECK(events[before].kind == EVENT_UNMAP && events[before].address == (uintptr_t)a);
    CHECK(events[before + 1].kind == EVENT_MAP && events[before + 1].address == (uintptr_t)a);
    CHECK(vma_at((uintptr_t)a)->phys == (uint64_t)phys + 2 * PAGE && vma_at((uintptr_t)b)->phys == (uint64_t)phys + PAGE);
    CHECK(direct_unmap(a, PAGE) == 0 && direct_unmap(b, PAGE) == 0);
    CHECK(direct_release((uint64_t)phys, 4 * PAGE) == 0);
}

/* More notifications than the queue's first allocation: one protect over many pieces. */
static void hook_queue_grows(void) {
    enum { PIECES = 100 };
    void *p = NULL;
    CHECK(map_flexible(&p, PIECES * PAGE, 3, 0) == 0 && p);
    unsigned char *m = p;
    for (unsigned i = 0; i < PIECES; i += 2) CHECK(kernel_mprotect(m + i * PAGE, PAGE, 1) == 0);
    const size_t before = pending_capacity;
    write_lock();
    CHECK(unmap_locked((uintptr_t)m, PIECES * PAGE) == 0); /* one HOOK_UNMAP per piece */
    CHECK(pending_count >= PIECES && pending_capacity >= pending_count && pending_capacity > before);
    write_unlock();
    flush_hooks();
    CHECK(pending_count == 0);
}

/* Backing-view writes (fence labels) land whole at any alignment, below and above 64 bytes. */
static void write_backing_stores_each_byte(void) {
    void *p = NULL;
    CHECK(map_flexible(&p, PAGE, 3, 0) == 0 && p);
    unsigned char *m = p, data[100];
    for (unsigned i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)(i + 1);
    for (unsigned offset = 0; offset < 8; ++offset)
        for (unsigned n = 1; n <= sizeof(data); n += 13) {
            memset(m, 0, 2 * sizeof(data));
            CHECK(runtime_memory_write_backing((uintptr_t)m + offset, data, n));
            CHECK(memcmp(m + offset, data, n) == 0 && m[offset + n] == 0 && (!offset || m[offset - 1] == 0));
        }
    CHECK(release_flexible(p, PAGE) == 0);
}

int main(void) {
    runtime_memory_set_gpu_hooks(on_map, on_unmap, on_invalidate);
    flexible_split_keeps_each_half();
    fixed_map_replaces_for_the_gpu();
    fixed_map_retires_before_replacing();
    batch_retires_only_maps_that_run();
    hook_queue_grows();
    write_backing_stores_each_byte();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("PASS: flexible split keeps each half's pages, fixed maps retire the GPU's view before replacing, batches retire only maps that run, hook queue grows, backing writes (%zu GPU notifications)\n", event_count);
    return 0;
}
