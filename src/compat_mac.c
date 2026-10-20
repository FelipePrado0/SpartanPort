/* macOS counterparts of Linux calls (compat_mac.h) and the runtime's low heap. */
#ifdef __APPLE__
#include "compat_mac.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <malloc/malloc.h>
#include <os/lock.h>
#include <mach/mach.h>

/* ---- low heap ----
 * Power-of-two size classes carved from one reservation in [SPARTAN_LOW_HEAP_MIN,
 * SPARTAN_LOW_HEAP_MAX), each class with its own free list. A 16-byte header before every
 * pointer names the block and its class; freed blocks of 64 KiB or more give their pages
 * back (MADV_FREE) but keep their addresses. */
#define MIN_CLASS 5 /* 32 bytes */
#define MAX_CLASS 34 /* 16 GiB */
#define MAGIC UINT32_C(0x67337768)
typedef struct { void *block; uint32_t size_class, magic; } Header;
_Static_assert(sizeof(Header) == 16, "low heap header keeps 16-byte alignment");
static os_unfair_lock heap_lock = OS_UNFAIR_LOCK_INIT;
static unsigned char *heap_base, *heap_next, *heap_end;
static void *free_lists[MAX_CLASS + 1];
static int heap_state; /* 0 not tried, 1 ready, -1 unavailable */
static pthread_once_t heap_once = PTHREAD_ONCE_INIT;

static void initialize_heap(void) {
    size_t span = (size_t)(SPARTAN_LOW_HEAP_MAX - SPARTAN_LOW_HEAP_MIN);
    void *p = mmap((void *)(uintptr_t)SPARTAN_LOW_HEAP_MIN, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == (void *)(uintptr_t)SPARTAN_LOW_HEAP_MIN) {
        heap_base = heap_next = p; heap_end = heap_base + span; heap_state = 1;
    } else {
        if (p != MAP_FAILED) munmap(p, span);
        fputs("Runtime: cannot reserve the low heap; refusing invalid guest pointers\n", stderr);
        heap_state = -1;
    }
}
static int owned(const void *p) { return heap_base && (const unsigned char *)p >= heap_base && (const unsigned char *)p < heap_end; }
static unsigned class_for(size_t bytes) {
    unsigned c = MIN_CLASS;
    while (c <= MAX_CLASS && ((size_t)1 << c) < bytes) ++c;
    return c;
}
/* A block of at least bytes; NULL when the reservation is exhausted. Called with the lock held. */
static void *take_block(size_t bytes, unsigned *size_class) {
    unsigned c = class_for(bytes);
    if (c > MAX_CLASS) return NULL;
    *size_class = c;
    void *block = free_lists[c];
    if (block) { free_lists[c] = *(void **)block; return block; }
    size_t size = (size_t)1 << c;
    /* Blocks are aligned to their size up to a page, so aligned requests stay cheap. */
    size_t align = size < 16384 ? size : 16384;
    unsigned char *at = (unsigned char *)(((uintptr_t)heap_next + align - 1) & ~(uintptr_t)(align - 1));
    if (at > heap_end || (size_t)(heap_end - at) < size) return NULL;
    heap_next = at + size;
    return at;
}
static void *allocate(size_t size, size_t alignment) {
    if (alignment < 16) alignment = 16;
    pthread_once(&heap_once, initialize_heap);
    if (heap_state < 0 || size > ((size_t)1 << MAX_CLASS) || alignment > ((size_t)1 << MAX_CLASS)) {
        errno = ENOMEM;
        return NULL;
    }
    size_t bytes = size + sizeof(Header) + (alignment > 16 ? alignment : 0);
    unsigned c;
    os_unfair_lock_lock(&heap_lock);
    unsigned char *block = take_block(bytes, &c);
    os_unfair_lock_unlock(&heap_lock);
    if (!block) { errno = ENOMEM; return NULL; }
    unsigned char *user = (unsigned char *)(((uintptr_t)block + sizeof(Header) + alignment - 1) & ~(uintptr_t)(alignment - 1));
    Header *h = (Header *)user - 1;
    h->block = block; h->size_class = c; h->magic = MAGIC;
    return user;
}
static Header *header_of(void *p) {
    Header *h = (Header *)p - 1;
    if (h->magic != MAGIC || h->size_class < MIN_CLASS || h->size_class > MAX_CLASS) {
        fprintf(stderr, "STOP: low heap: bad pointer %p\n", p);
        abort();
    }
    return h;
}
static size_t capacity(void *p) {
    Header *h = header_of(p);
    return ((size_t)1 << h->size_class) - (size_t)((unsigned char *)p - (unsigned char *)h->block);
}

void *spartanport_low_malloc(size_t size) { return allocate(size, 16); }
void *spartanport_low_calloc(size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) { errno = ENOMEM; return NULL; }
    void *p = allocate(count * size, 16);
    /* Reused blocks hold old data; fresh ones are zero pages already. */
    if (p) memset(p, 0, count * size);
    return p;
}
void spartanport_low_free(void *p) {
    if (!p) return;
    if (!owned(p)) { (free)(p); return; }
    Header *h = header_of(p);
    unsigned c = h->size_class;
    unsigned char *block = h->block;
    h->magic = 0;
    size_t size = (size_t)1 << c;
    if (size >= 65536) madvise(block + 16384, size - 16384, MADV_FREE);
    os_unfair_lock_lock(&heap_lock);
    *(void **)block = free_lists[c]; free_lists[c] = block;
    os_unfair_lock_unlock(&heap_lock);
}
void *spartanport_low_realloc(void *p, size_t size) {
    if (!p) return spartanport_low_malloc(size);
    if (!size) { spartanport_low_free(p); return NULL; }
    size_t old = owned(p) ? capacity(p) : malloc_size(p);
    if (owned(p) && size <= old) return p;
    void *q = spartanport_low_malloc(size);
    if (!q) return NULL;
    memcpy(q, p, old < size ? old : size);
    spartanport_low_free(p);
    return q;
}
void *spartanport_low_aligned_alloc(size_t alignment, size_t size) {
    if (!alignment || (alignment & (alignment - 1))) { errno = EINVAL; return NULL; }
    return allocate(size, alignment);
}
int spartanport_low_posix_memalign(void **out, size_t alignment, size_t size) {
    if (!alignment || (alignment & (alignment - 1)) || alignment % sizeof(void *)) return EINVAL;
    void *p = allocate(size, alignment);
    if (!p) return ENOMEM;
    *out = p;
    return 0;
}
char *spartanport_low_strdup(const char *text) {
    size_t n = strlen(text) + 1;
    char *p = spartanport_low_malloc(n);
    if (p) memcpy(p, text, n);
    return p;
}
char *spartanport_low_strndup(const char *text, size_t size) {
    size_t n = strnlen(text, size);
    char *p = spartanport_low_malloc(n + 1);
    if (p) { memcpy(p, text, n); p[n] = 0; }
    return p;
}

/* ---- time and locks ---- */
static int64_t ns_of(const struct timespec *t) { return (int64_t)t->tv_sec * 1000000000 + t->tv_nsec; }
static int64_t now_on(clockid_t clock) {
    struct timespec t;
    clock_gettime(clock, &t);
    return ns_of(&t);
}
int compat_clock_nanosleep(clockid_t clock, int flags, const struct timespec *request, struct timespec *remain) {
    int64_t left = (flags & TIMER_ABSTIME) ? ns_of(request) - now_on(clock) : ns_of(request);
    if (left <= 0) return 0;
    struct timespec t = {(time_t)(left / 1000000000), (long)(left % 1000000000)};
    if (nanosleep(&t, remain) == 0) return 0;
    return errno;
}
/* Polls until the CLOCK_REALTIME deadline: Darwin has no timed mutex or rwlock locks. */
static void pause_before(const struct timespec *deadline) {
    int64_t left = ns_of(deadline) - now_on(CLOCK_REALTIME);
    struct timespec t = {0, (long)(left < 100000 ? (left > 0 ? left : 0) : 100000)};
    nanosleep(&t, NULL);
}
int compat_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *deadline) {
    if (deadline->tv_nsec < 0 || deadline->tv_nsec >= 1000000000) return EINVAL;
    for (;;) {
        int e = pthread_mutex_trylock(mutex);
        if (e != EBUSY) return e;
        if (now_on(CLOCK_REALTIME) >= ns_of(deadline)) return ETIMEDOUT;
        pause_before(deadline);
    }
}
static int rwlock_timed(pthread_rwlock_t *lock, const struct timespec *deadline, int writer) {
    if (deadline->tv_nsec < 0 || deadline->tv_nsec >= 1000000000) return EINVAL;
    for (;;) {
        int e = writer ? pthread_rwlock_trywrlock(lock) : pthread_rwlock_tryrdlock(lock);
        if (e != EBUSY) return e;
        if (now_on(CLOCK_REALTIME) >= ns_of(deadline)) return ETIMEDOUT;
        pause_before(deadline);
    }
}
int compat_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *deadline) { return rwlock_timed(lock, deadline, 0); }
int compat_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *deadline) { return rwlock_timed(lock, deadline, 1); }
int compat_thread_rusage(struct timeval *user, struct timeval *system) {
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    mach_port_t self = mach_thread_self();
    kern_return_t kr = thread_info(self, THREAD_BASIC_INFO, (thread_info_t)&info, &count);
    mach_port_deallocate(mach_task_self(), self);
    if (kr != KERN_SUCCESS) return -1;
    *user = (struct timeval){info.user_time.seconds, info.user_time.microseconds};
    *system = (struct timeval){info.system_time.seconds, info.system_time.microseconds};
    return 0;
}
ssize_t compat_getrandom(void *buffer, size_t size, unsigned flags) {
    (void)flags;
    for (size_t done = 0; done < size; done += 256) {
        size_t n = size - done < 256 ? size - done : 256;
        if (getentropy((unsigned char *)buffer + done, n)) return -1;
    }
    return (ssize_t)size;
}
#endif
