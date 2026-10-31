/* Col: memory trace (src/runtime_memory.c, SPARTAN_MEM_TRACE=1): off by default; when on, one line
 * per allocation, mapping, fixed replacement (with the generations it replaces), retire, unmap and
 * release, filtered by size (SPARTAN_MEM_TRACE_MIN_KB) and capped (SPARTAN_MEM_TRACE_MAX).
 * Linux: build.sh --test (out/memory-trace-test). */
#include "../src/runtime_memory.c"

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }
static __thread int32_t errno_slot;
int32_t *runtime_errno(void) { return &errno_slot; }

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void on_map(uintptr_t a, uint64_t s) { (void)a; (void)s; }
static void on_unmap(uintptr_t a, uint64_t s) { (void)a; (void)s; }
static void on_invalidate(uintptr_t a, uint64_t s) { (void)a; (void)s; }

/* Restarts the trace with these settings, into a fresh capture file. */
static FILE *capture;
static void trace_with(const char *on, const char *min_kb, const char *max) {
    if (on) setenv("SPARTAN_MEM_TRACE", on, 1); else unsetenv("SPARTAN_MEM_TRACE");
    if (min_kb) setenv("SPARTAN_MEM_TRACE_MIN_KB", min_kb, 1); else unsetenv("SPARTAN_MEM_TRACE_MIN_KB");
    if (max) setenv("SPARTAN_MEM_TRACE_MAX", max, 1); else unsetenv("SPARTAN_MEM_TRACE_MAX");
    if (capture) fclose(capture);
    capture = tmpfile();
    col_sink = capture;
    col_state = -1;
    col_lines = 0;
}
static char text[65536];
static const char *captured(void) {
    fflush(capture);
    rewind(capture);
    size_t n = fread(text, 1, sizeof(text) - 1, capture);
    text[n] = 0;
    return text;
}
static int count_lines(const char *s) { int n = 0; for (; *s; ++s) n += *s == '\n'; return n; }
/* Every line is "Col: t=<ms> tid=<n> ...". */
static int lines_well_formed(const char *s) {
    for (const char *line = s; *line; ) {
        const char *end = strchr(line, '\n');
        if (!end || strncmp(line, "Col: ", 5)) return 0;
        if (strncmp(line, "Col: memory trace", 17) && (!strstr(line, " t=") || !strstr(line, " tid="))) return 0;
        line = end + 1;
    }
    return 1;
}
static int has(const char *s, const char *needle) { return strstr(s, needle) != NULL; }

static void off_prints_nothing(void) {
    trace_with(NULL, "0", NULL);
    int64_t phys = -1; void *p = NULL;
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 2 * PAGE, PAGE, 0, &phys) == 0);
    CHECK(direct_map(&p, 2 * PAGE, 3, 0, phys, 0) == 0);
    CHECK(direct_unmap(p, 2 * PAGE) == 0);
    CHECK(direct_release((uint64_t)phys, 2 * PAGE) == 0);
    CHECK(captured()[0] == 0);
    trace_with("0", "0", NULL);
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 2 * PAGE, PAGE, 0, &phys) == 0);
    CHECK(direct_release((uint64_t)phys, 2 * PAGE) == 0);
    CHECK(captured()[0] == 0);
}

static void on_traces_each_change(void) {
    trace_with("1", "0", NULL);
    int64_t phys = -1; char want[256];
    CHECK(direct_allocate(0, (int64_t)POOL_SIZE, 4 * PAGE, PAGE, 0, &phys) == 0);
    void *p = NULL;
    CHECK(direct_map(&p, 2 * PAGE, 3, 0, phys, 0) == 0);
    const uint64_t first = vmas[vma_index((uintptr_t)p)].gen;
    void *again = p;
    CHECK(direct_map(&again, 2 * PAGE, 3, MAP_FIXED_FLAG, phys + 2 * PAGE, 0) == 0);
    const uint64_t second = vmas[vma_index((uintptr_t)p)].gen;
    CHECK(second > first);
    void *bad = p;
    CHECK(direct_map(&bad, 2 * PAGE, 3, MAP_FIXED_FLAG, phys + 8 * PAGE, 0) != 0); /* unallocated */
    CHECK(direct_unmap(p, 2 * PAGE) == 0);
    void *f = NULL;
    CHECK(map_flexible(&f, 2 * PAGE, 3, 0) == 0);
    BatchEntry entry = {.start = f, .length = 2 * PAGE, .operation = 1};
    int processed = -1;
    CHECK(batch_map(&entry, 1, &processed) == 0 && processed == 1);
    void *r = NULL;
    CHECK(direct_map(&r, PAGE, 3, 0, phys, 0) == 0);
    CHECK(direct_release((uint64_t)phys, 4 * PAGE) == 0);

    const char *s = captured();
    CHECK(lines_well_formed(s));
    CHECK(has(s, "Col: memory trace on (min=0 KiB, max=20000 lines)\n"));
    snprintf(want, sizeof(want), " alloc phys=0x%" PRIx64 " size=65536 type=0\n", (uint64_t)phys);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " map kind=direct addr=%p size=32768 phys=0x%" PRIx64 " prot=0x3 gen=%" PRIu64 "\n", p, (uint64_t)phys, first);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " retire addr=%p size=32768 gen=%" PRIu64 "-%" PRIu64 "\n", p, first, first);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " map kind=direct addr=%p size=32768 phys=0x%" PRIx64 " prot=0x3 gen=%" PRIu64 " replaced=1 oldgen=%" PRIu64 "-%" PRIu64 "\n",
             p, (uint64_t)phys + 2 * PAGE, second, first, first);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " map-failed kind=direct addr=%p size=32768 result=0x80020016 retired=0\n", p);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " unmap addr=%p size=32768 gen=%" PRIu64 "-%" PRIu64 " src=munmap\n", p, second, second);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " map kind=flexible addr=%p size=32768 phys=0x", f);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " unmap addr=%p size=32768 gen=", f);
    CHECK(has(s, want) && has(s, " src=batch\n"));
    snprintf(want, sizeof(want), " unmap addr=%p size=16384 gen=", r);
    CHECK(has(s, want) && has(s, " src=release\n"));
    snprintf(want, sizeof(want), " release phys=0x%" PRIx64 " size=65536\n", (uint64_t)phys);
    CHECK(has(s, want));
    /* The retire comes before the replacing map; the release's unmap before the release. */
    snprintf(want, sizeof(want), " retire addr=%p", p);
    const char *retire = strstr(s, want), *replace = strstr(s, " replaced=1");
    CHECK(retire && replace && retire < replace);
    CHECK(strstr(s, " src=release") < strstr(s, " release phys="));
    if (failures) fputs(s, stderr);
}

static void small_changes_filtered(void) {
    trace_with("1", NULL, NULL); /* default minimum: 64 KiB */
    void *small = NULL, *large = NULL;
    CHECK(map_flexible(&small, 2 * PAGE, 3, 0) == 0);
    CHECK(map_flexible(&large, 8 * PAGE, 3, 0) == 0);
    CHECK(release_flexible(small, 2 * PAGE) == 0);
    CHECK(release_flexible(large, 8 * PAGE) == 0);
    const char *s = captured();
    char want[128];
    CHECK(has(s, "Col: memory trace on (min=64 KiB, max=20000 lines)\n"));
    snprintf(want, sizeof(want), "addr=%p size=32768", small);
    CHECK(!has(s, want));
    snprintf(want, sizeof(want), " map kind=flexible addr=%p size=131072", large);
    CHECK(has(s, want));
    snprintf(want, sizeof(want), " unmap addr=%p size=131072", large);
    CHECK(has(s, want));
    CHECK(count_lines(s) == 3);
}

/* A late write of a retired generation over a newer mapping is dropped and always traced. */
static void stale_write_traced(void) {
    trace_with("1", NULL, NULL);
    void *f = NULL;
    CHECK(map_flexible(&f, PAGE, 3, 0) == 0);
    memset(f, 0x22, PAGE);
    const uint64_t gen = vmas[vma_index((uintptr_t)f)].gen;
    const unsigned char late[16] = {0xEE};
    retiring = (Retiring){(uintptr_t)f, (uintptr_t)f + PAGE, gen - 1, 1};
    CHECK(runtime_memory_write_backing((uintptr_t)f, late, sizeof(late)) == 1);
    retiring.active = 0;
    CHECK(((unsigned char *)f)[0] == 0x22);
    char want[160];
    snprintf(want, sizeof(want), " stale-write addr=%p size=16 gen=%" PRIu64 " retiring=%" PRIu64 "\n", f, gen, gen - 1);
    CHECK(has(captured(), want));
    CHECK(release_flexible(f, PAGE) == 0);
}

static void line_cap(void) {
    trace_with("1", "0", "3");
    for (int i = 0; i < 4; ++i) {
        void *p = NULL;
        CHECK(map_flexible(&p, PAGE, 3, 0) == 0);
        CHECK(release_flexible(p, PAGE) == 0);
    }
    const char *s = captured();
    CHECK(count_lines(s) == 5); /* header, 3 lines, the limit line */
    CHECK(has(s, "Col: memory trace limit reached\n"));
    const char *limit = strstr(s, "limit reached\n");
    CHECK(limit && limit[14] == 0);
}

int main(void) {
    runtime_memory_set_gpu_hooks(on_map, on_unmap, on_invalidate);
    off_prints_nothing();
    on_traces_each_change();
    small_changes_filtered();
    stale_write_traced();
    line_cap();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    puts("PASS: memory trace off by default, one line per change with generations, size filter, late writes, line cap");
    return 0;
}
