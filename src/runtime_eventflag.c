/* Kernel event flags for native PS4 guest threads.
 * Supports AND/OR waits, clear modes, single/multi waiter attributes and microsecond timeouts. */
#define _GNU_SOURCE
#include "runtime.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
#define EVF_WAIT_AND 0x01u
#define EVF_WAIT_OR 0x02u
#define EVF_CLEAR_ALL 0x10u
#define EVF_CLEAR_BITS 0x20u
#define EVF_ATTR_FIFO 0x01u
#define EVF_ATTR_PRIO 0x02u
#define EVF_ATTR_SINGLE 0x10u
#define EVF_ATTR_MULTI 0x20u

typedef struct EventFlag {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    pthread_cond_t drained;
    uint64_t bits;
    uint32_t attr;
    unsigned waiters;
    int deleted;
    char name[32];
} EventFlag;

static size_t created_count, deleted_count, set_count, wait_count, timeout_count;

static void host_check(int e) {
    if (e) {
        fprintf(stderr,"STOP: event flag host error %d\n",e);
        exit(21);
    }
}

static uint64_t realtime_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME,&t)) {
        perror("clock_gettime");
        exit(1);
    }
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}

static int valid_attr(uint32_t attr) {
    uint32_t queue=attr&0x0f, threads=attr&0xf0;
    return !(attr&~UINT32_C(0x33)) &&
           (queue==0 || queue==EVF_ATTR_FIFO || queue==EVF_ATTR_PRIO) &&
           (threads==0 || threads==EVF_ATTR_SINGLE || threads==EVF_ATTR_MULTI);
}

static int valid_wait(uint32_t mode) {
    uint32_t condition=mode&0x0f, clear=mode&0xf0;
    return !(mode&~UINT32_C(0x33)) &&
           (condition==EVF_WAIT_AND || condition==EVF_WAIT_OR) &&
           (clear==0 || clear==EVF_CLEAR_ALL || clear==EVF_CLEAR_BITS);
}

static int condition_met(uint64_t bits,uint64_t pattern,uint32_t mode) {
    return (mode&0x0f)==EVF_WAIT_AND ? (bits&pattern)==pattern : (bits&pattern)!=0;
}

static void apply_clear(EventFlag *ef,uint64_t pattern,uint32_t mode) {
    if ((mode&0xf0)==EVF_CLEAR_ALL) ef->bits=0;
    else if ((mode&0xf0)==EVF_CLEAR_BITS) ef->bits&=~pattern;
}

static ABI int32_t eventflag_create(EventFlag **out,const char *name,uint32_t attr,
                                    uint64_t initial,const void *options) {
    if (!out || !name || options || !valid_attr(attr)) return ERR(22);
    if (strlen(name)>=32) return ERR(63);
    EventFlag *ef=calloc(1,sizeof(*ef));
    if (!ef) return ERR(12);
    int e=pthread_mutex_init(&ef->lock,NULL);
    if (!e) e=pthread_cond_init(&ef->changed,NULL);
    if (!e) e=pthread_cond_init(&ef->drained,NULL);
    if (e) {
        if (!pthread_mutex_destroy(&ef->lock)) {}
        free(ef);
        return ERR(12);
    }
    ef->bits=initial;
    ef->attr=attr;
    strcpy(ef->name,name);
    *out=ef;
    ++created_count;
    printf("Runtime: event flag %s created attr=0x%x bits=0x%llx\n",
           name,attr,(unsigned long long)initial);
    return 0;
}

static ABI int32_t eventflag_delete(EventFlag *ef) {
    if (!ef) return ERR(3);
    host_check(pthread_mutex_lock(&ef->lock));
    ef->deleted=1;
    host_check(pthread_cond_broadcast(&ef->changed));
    while (ef->waiters) host_check(pthread_cond_wait(&ef->drained,&ef->lock));
    host_check(pthread_mutex_unlock(&ef->lock));
    host_check(pthread_cond_destroy(&ef->changed));
    host_check(pthread_cond_destroy(&ef->drained));
    host_check(pthread_mutex_destroy(&ef->lock));
    free(ef);
    ++deleted_count;
    return 0;
}

static ABI int32_t eventflag_set(EventFlag *ef,uint64_t pattern) {
    if (!ef) return ERR(3);
    host_check(pthread_mutex_lock(&ef->lock));
    if (ef->deleted) {
        host_check(pthread_mutex_unlock(&ef->lock));
        return ERR(13);
    }
    ef->bits|=pattern;
    ++set_count;
    host_check(pthread_cond_broadcast(&ef->changed));
    host_check(pthread_mutex_unlock(&ef->lock));
    return 0;
}

static ABI int32_t eventflag_wait(EventFlag *ef,uint64_t pattern,uint32_t mode,
                                  uint64_t *result,uint32_t *timeout) {
    if (!ef) return ERR(3);
    if (!pattern || !valid_wait(mode)) return ERR(22);
    host_check(pthread_mutex_lock(&ef->lock));
    if (ef->deleted) {
        host_check(pthread_mutex_unlock(&ef->lock));
        return ERR(13);
    }
    if ((ef->attr&0xf0)!=EVF_ATTR_MULTI && ef->waiters) {
        host_check(pthread_mutex_unlock(&ef->lock));
        return ERR(1);
    }

    if (condition_met(ef->bits,pattern,mode)) {
        if (result) *result=ef->bits;
        apply_clear(ef,pattern,mode);
        ++wait_count;
        host_check(pthread_mutex_unlock(&ef->lock));
        return 0;
    }

    if (timeout && !*timeout) {
        if (result) *result=ef->bits;
        ++timeout_count;
        host_check(pthread_mutex_unlock(&ef->lock));
        return ERR(60);
    }

    ++ef->waiters;
    uint64_t deadline=timeout ? realtime_ns()+(uint64_t)*timeout*1000 : 0;
    int32_t rc=0;
    for (;;) {
        if (ef->deleted) { rc=ERR(13); break; }
        if (condition_met(ef->bits,pattern,mode)) {
            if (result) *result=ef->bits;
            apply_clear(ef,pattern,mode);
            ++wait_count;
            break;
        }
        int e;
        if (timeout) {
            struct timespec end={
                .tv_sec=(time_t)(deadline/UINT64_C(1000000000)),
                .tv_nsec=(long)(deadline%UINT64_C(1000000000))
            };
            e=pthread_cond_timedwait(&ef->changed,&ef->lock,&end);
            if (e==ETIMEDOUT) {
                if (result) *result=ef->bits;
                *timeout=0;
                ++timeout_count;
                rc=ERR(60);
                break;
            }
        } else {
            e=pthread_cond_wait(&ef->changed,&ef->lock);
        }
        if (e && e!=ETIMEDOUT) host_check(e);
    }

    if (!--ef->waiters && ef->deleted) host_check(pthread_cond_signal(&ef->drained));
    host_check(pthread_mutex_unlock(&ef->lock));
    return rc;
}

uintptr_t runtime_eventflag_resolve(const char *name) {
    if (!strcmp(name,"BpFoboUJoZU#libkernel")) return (uintptr_t)eventflag_create;
    if (!strcmp(name,"8mql9OcQnd4#libkernel")) return (uintptr_t)eventflag_delete;
    if (!strcmp(name,"IOnSvHzqu6A#libkernel")) return (uintptr_t)eventflag_set;
    if (!strcmp(name,"JTvBflhYazQ#libkernel")) return (uintptr_t)eventflag_wait;
    return 0;
}

void runtime_eventflag_report(void) {
    printf("Runtime: event flags created=%zu, deleted=%zu, set=%zu, waits=%zu, timeouts=%zu\n",
           created_count,deleted_count,set_count,wait_count,timeout_count);
}
