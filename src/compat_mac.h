/* macOS (x86-64 under Rosetta 2) counterparts of the Linux calls the runtime uses. Included
 * through runtime.h; the C library headers come first so the macros below only replace calls in
 * this project's own sources.
 * Host heap objects handed to the guest (thread handles, TLS blocks, mutexes) must sit below
 * 1 TiB: guest code packs pointers into 40 bits. Linux keeps glibc's brk heap there; macOS
 * malloc returns 0x7f... addresses, so the runtime's allocations go to a heap in low memory
 * (compat_mac.c). Pointers from the system allocator (realpath, getline) are still freed here. */
#ifndef SPARTAN_COMPAT_MAC_H
#define SPARTAN_COMPAT_MAC_H
#ifdef __APPLE__
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

void *spartanport_low_malloc(size_t size);
void *spartanport_low_calloc(size_t count, size_t size);
void *spartanport_low_realloc(void *pointer, size_t size);
void *spartanport_low_aligned_alloc(size_t alignment, size_t size);
int spartanport_low_posix_memalign(void **out, size_t alignment, size_t size);
char *spartanport_low_strdup(const char *text);
char *spartanport_low_strndup(const char *text, size_t size);
void spartanport_low_free(void *pointer);
#define malloc(size) spartanport_low_malloc(size)
#define calloc(count, size) spartanport_low_calloc(count, size)
#define realloc(pointer, size) spartanport_low_realloc(pointer, size)
#define aligned_alloc(alignment, size) spartanport_low_aligned_alloc(alignment, size)
#define posix_memalign(out, alignment, size) spartanport_low_posix_memalign(out, alignment, size)
#define strdup(text) spartanport_low_strdup(text)
#define strndup(text, size) spartanport_low_strndup(text, size)
#define free(pointer) spartanport_low_free(pointer)
/* Reserves the low heap; runtime_low_map stays below it. */
#define SPARTAN_LOW_HEAP_MIN UINT64_C(0x0e00000000)
#define SPARTAN_LOW_HEAP_MAX UINT64_C(0x0fc0000000)

#ifndef CLOCK_REALTIME_COARSE
#define CLOCK_REALTIME_COARSE CLOCK_REALTIME
#endif
#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif
/* struct stat: Darwin names the timespec fields st_*timespec. */
#define st_atim st_atimespec
#define st_mtim st_mtimespec
#define st_ctim st_ctimespec

static inline pid_t compat_gettid(void) {
    uint64_t id = 0;
    pthread_threadid_np(NULL, &id);
    return (pid_t)id;
}
#define gettid compat_gettid
/* Darwin names only the calling thread. */
#define pthread_setname_np(thread, name) ((void)(thread), pthread_setname_np(name))

int compat_clock_nanosleep(clockid_t clock, int flags, const struct timespec *request, struct timespec *remain);
#define clock_nanosleep compat_clock_nanosleep
int compat_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *deadline);
#define pthread_mutex_timedlock compat_mutex_timedlock
int compat_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *deadline);
int compat_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *deadline);
#define pthread_rwlock_timedrdlock compat_rwlock_timedrdlock
#define pthread_rwlock_timedwrlock compat_rwlock_timedwrlock
/* Darwin condition variables always time out on CLOCK_REALTIME. */
#define pthread_condattr_setclock(attr, clock) ((void)(attr), (clock) == CLOCK_REALTIME ? 0 : EINVAL)
/* User and system CPU time of the calling thread (getrusage has no RUSAGE_THREAD). */
int compat_thread_rusage(struct timeval *user, struct timeval *system);
/* POSIX barriers (tests): Darwin has none. */
#ifndef PTHREAD_BARRIER_SERIAL_THREAD
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)
typedef struct { pthread_mutex_t mutex; pthread_cond_t cond; unsigned count, waiting, cycle; } pthread_barrier_t;
static inline int pthread_barrier_init(pthread_barrier_t *b, const void *attr, unsigned count) {
    (void)attr;
    if (!count) return EINVAL;
    b->count = count; b->waiting = 0; b->cycle = 0;
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    return 0;
}
static inline int pthread_barrier_destroy(pthread_barrier_t *b) {
    pthread_cond_destroy(&b->cond);
    return pthread_mutex_destroy(&b->mutex);
}
static inline int pthread_barrier_wait(pthread_barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    unsigned cycle = b->cycle;
    if (++b->waiting == b->count) {
        b->waiting = 0; ++b->cycle;
        pthread_cond_broadcast(&b->cond);
        pthread_mutex_unlock(&b->mutex);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    while (cycle == b->cycle) pthread_cond_wait(&b->cond, &b->mutex);
    pthread_mutex_unlock(&b->mutex);
    return 0;
}
#endif
/* Fills buffer with random bytes; returns size or -1 (getentropy takes 256 bytes at a time). */
ssize_t compat_getrandom(void *buffer, size_t size, unsigned flags);
#define getrandom compat_getrandom
#endif
#endif
