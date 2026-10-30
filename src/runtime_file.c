/* Guest file system: PS4 mount points mapped onto host directories.
 *   /app0, /hostapp  -> game package root (read-only by convention)
 *   /temp0, /download0, /data, and mounts added by SaveData -> user directory
 * Guest descriptors are small integers in our own table; stdio 0-2 pass through.
 * Paths containing ".." components are rejected rather than normalized. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
/* 64-bit sizes and times; directories are never opened as CRT descriptors. */
typedef struct _stat64 HostStat;
#define host_stat(path,s) _stat64(path,s)
#define host_fstat(fd,s) _fstat64(fd,s)
#define host_lseek _lseeki64
#define O_DIRECTORY 0x40000000 /* spartanport marker, removed before _open */
#else
typedef struct stat HostStat;
#define host_stat(path,s) stat(path,s)
#define host_fstat(fd,s) fstat(fd,s)
#define host_lseek lseek
#endif
#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
#define MAX_FILES 1024
#define MAX_MOUNTS 32  /* glyphs (3), pause menu (1), switch attack tutorials (16 languages) */

typedef struct { int64_t sec, nsec; } GuestTimespec;
typedef struct {
    uint32_t dev, ino;
    uint16_t mode, nlink;
    uint32_t uid, gid, rdev;
    GuestTimespec atime, mtime, ctime;
    int64_t size, blocks;
    uint32_t blksize, flags, gen;
    int32_t lspare;
    GuestTimespec birthtime;
} GuestStat;
_Static_assert(sizeof(GuestStat)==120,"FreeBSD stat layout");

typedef struct { char *names; size_t count, *offsets; unsigned char *types; } Listing;
/* traced..last_us: Load: trace state of a game file (SPARTAN_FILE_TRACE / SPARTAN_LOAD_WATCHDOG). */
typedef struct {
    int used, host; Listing *dir; size_t position; char path[512];
    int traced; int64_t size; uint64_t covered, bytes, reads, opened_us, last_us;
} File;
typedef struct { char guest[64]; char host[512]; } Mount;
static File files[MAX_FILES];
static Mount mounts[MAX_MOUNTS];
static size_t mount_count, opens, reads, writes, missing;
static uint64_t bytes_read;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

int runtime_file_mount(const char *guest,const char *host) {
    pthread_mutex_lock(&lock);
    for (size_t i=0;i<mount_count;++i) if (!strcmp(mounts[i].guest,guest)) {
        snprintf(mounts[i].host,sizeof(mounts[i].host),"%s",host);
        pthread_mutex_unlock(&lock); return 0;
    }
    if (mount_count==MAX_MOUNTS || strlen(guest)>=64 || strlen(host)>=512) { pthread_mutex_unlock(&lock); return -1; }
    snprintf(mounts[mount_count].guest,64,"%s",guest);
    snprintf(mounts[mount_count].host,512,"%s",host);
    ++mount_count;
    pthread_mutex_unlock(&lock);
    return 0;
}
void runtime_file_unmount(const char *guest) {
    pthread_mutex_lock(&lock);
    for (size_t i=0;i<mount_count;++i) if (!strcmp(mounts[i].guest,guest)) {
        mounts[i]=mounts[--mount_count]; break;
    }
    pthread_mutex_unlock(&lock);
}
static char user_root[512]="user";
const char *runtime_file_user_dir(void) { return user_root; }
void runtime_file_configure(const char *app0,const char *user) {
    char path[600];
    snprintf(user_root,sizeof(user_root),"%s",user);
    runtime_file_mount("/app0",app0);
    runtime_file_mount("/hostapp",app0);
    const char *writable[]={"temp0","download0","data"};
    mkdir(user,0755);
    for (int i=0;i<3;++i) {
        snprintf(path,sizeof(path),"%s/%s",user,writable[i]);
        mkdir(path,0755);
        char guest[32]; snprintf(guest,sizeof(guest),"/%s",writable[i]);
        runtime_file_mount(guest,path);
    }
}
/* Resolve a guest path to a host path; returns 0 or a host errno. */
static int translate(const char *guest,char *out,size_t size) {
    if (!guest || !*guest) return ENOENT;
    char buffer[1024];
    if (guest[0]!='/') snprintf(buffer,sizeof(buffer),"/app0/%s",guest);
    else snprintf(buffer,sizeof(buffer),"%s",guest);
    for (const char *p=buffer;(p=strstr(p,".."));p+=2)
        if ((p==buffer || p[-1]=='/') && (p[2]==0 || p[2]=='/')) return EACCES;
    pthread_mutex_lock(&lock);
    size_t best=0; const Mount *m=NULL;
    for (size_t i=0;i<mount_count;++i) {
        size_t n=strlen(mounts[i].guest);
        if (!strncmp(buffer,mounts[i].guest,n) && (buffer[n]=='/' || !buffer[n]) && n>best) { best=n; m=&mounts[i]; }
    }
    int result=0;
    if (!m) result=ENOENT;
    else if ((size_t)snprintf(out,size,"%s%s",m->host,buffer+best)>=size) result=ENAMETOOLONG;
    pthread_mutex_unlock(&lock);
    if (result==ENOENT) fprintf(stderr,"Runtime: no mount for guest path %s\n",guest);
    return result;
}
static int host_flags(int flags) {
    int r;
    switch (flags&3) { case 0: r=O_RDONLY; break; case 1: r=O_WRONLY; break; default: r=O_RDWR; }
    if (flags&0x4) r|=O_NONBLOCK;
    if (flags&0x8) r|=O_APPEND;
    if (flags&0x80) r|=O_SYNC;
    if (flags&0x200) r|=O_CREAT;
    if (flags&0x400) r|=O_TRUNC;
    if (flags&0x800) r|=O_EXCL;
    if (flags&0x20000) r|=O_DIRECTORY;
#ifdef _WIN32
    r|=O_BINARY;
#endif
    return r|O_CLOEXEC;
}
static void free_listing(Listing *l) { if (l) { free(l->names); free(l->offsets); free(l->types); free(l); } }
static Listing *list_directory(const char *path) {
    DIR *d=opendir(path);
    if (!d) return NULL;
    Listing *l=calloc(1,sizeof(*l));
    size_t capacity=0,bytes=0,cap_names=0;
    struct dirent *e;
    while (l && (e=readdir(d))) {
        size_t n=strlen(e->d_name)+1;
        if (l->count==capacity) {
            capacity=capacity ? capacity*2 : 64;
            l->offsets=realloc(l->offsets,capacity*sizeof(size_t));
            l->types=realloc(l->types,capacity);
        }
        if (bytes+n>cap_names) { cap_names=(bytes+n)*2; l->names=realloc(l->names,cap_names); }
        if (!l->offsets || !l->types || !l->names) { fputs("Out of memory listing directory\n",stderr); exit(1); }
        memcpy(l->names+bytes,e->d_name,n);
        l->offsets[l->count]=bytes;
#ifdef _WIN32
        /* MinGW's dirent has no d_type. */
        char child[1100]; HostStat entry;
        snprintf(child,sizeof(child),"%s/%s",path,e->d_name);
        l->types[l->count]=host_stat(child,&entry) ? 0 : S_ISDIR(entry.st_mode) ? 4 : S_ISREG(entry.st_mode) ? 8 : 0;
#else
        unsigned char type=e->d_type;
        if (type==DT_LNK || type==DT_UNKNOWN) {
            struct stat entry;
            if (!fstatat(dirfd(d),e->d_name,&entry,0))
                type=S_ISDIR(entry.st_mode) ? DT_DIR : S_ISREG(entry.st_mode) ? DT_REG : type;
        }
        l->types[l->count]=type==DT_DIR ? 4 : type==DT_REG ? 8 : type==DT_LNK ? 10 : 0;
#endif
        ++l->count; bytes+=n;
    }
    closedir(d);
    return l;
}
static void convert_stat(const HostStat *s,GuestStat *g) {
    memset(g,0,sizeof(*g));
    g->dev=(uint32_t)s->st_dev; g->ino=(uint32_t)s->st_ino;
    g->nlink=(uint16_t)s->st_nlink;
    g->size=s->st_size;
#ifdef _WIN32
    /* FreeBSD mode bits: directories rwxr-xr-x, files rw-r--r--. */
    g->mode=(uint16_t)(S_ISDIR(s->st_mode) ? 0040755 : 0100644);
    g->blocks=(s->st_size+511)/512; g->blksize=4096;
    g->atime=(GuestTimespec){s->st_atime,0};
    g->mtime=(GuestTimespec){s->st_mtime,0};
    g->ctime=(GuestTimespec){s->st_ctime,0};
#else
    g->mode=(uint16_t)s->st_mode;
    g->blocks=s->st_blocks; g->blksize=(uint32_t)s->st_blksize;
    g->atime=(GuestTimespec){s->st_atim.tv_sec,s->st_atim.tv_nsec};
    g->mtime=(GuestTimespec){s->st_mtim.tv_sec,s->st_mtim.tv_nsec};
    g->ctime=(GuestTimespec){s->st_ctim.tv_sec,s->st_ctim.tv_nsec};
#endif
    g->birthtime=g->ctime;
}
static File *get(int fd) {
    if (fd<3 || fd>=MAX_FILES || !files[fd].used) return NULL;
    return &files[fd];
}
/* SPARTAN_AUDIO_TRACE=1: sound file opens and failed reads (missing game sounds). */
static int audio_trace(void) { static int v=-1; if (v<0) { const char *e=getenv("SPARTAN_AUDIO_TRACE"); v=e && e[0]=='1'; } return v; }
/* ---- Load: file trace, progress state and stall watchdog ----
 * docs/specs/2026-10-29-load-file-trace.md and docs/specs/2026-10-29-load-stall-watchdog.md.
 * SPARTAN_FILE_TRACE=1 logs begin/end of open/read/pread/close on .wad files (=all: every file);
 * SPARTAN_FILE_TRACE_LIMIT caps the operation lines (default 20000, 0: none);
 * SPARTAN_LOAD_WATCHDOG=<s> reports the load state and the threads when a load makes no file
 * progress for that long. Off: one atomic load per operation. Every path keeps errno. */
enum { LOAD_OFF, LOAD_WAD, LOAD_ALL };
enum { LOAD_INFLIGHT = 64, LOAD_REPORTS_MAX = 8 };
typedef struct { uint64_t id, started; int64_t offset; int fd, slot; const char *op; } LoadOp;
typedef struct {
    uint64_t id, started; int64_t offset; uint64_t size; int fd; unsigned long tid;
    const char *op; char thread[32]; char path[256];
} LoadFlight;
static int load_ready, load_mode, load_lines, load_reported, load_watchdog_autostart=1;
static unsigned load_watchdog_seconds, load_reports;
static uint64_t load_limit, load_written, load_sequence, load_last_us, load_last_id;
static FILE *load_out; /* NULL: stdout (user/last_run.log) */
static LoadFlight load_flights[LOAD_INFLIGHT];
static pthread_once_t load_env_once=PTHREAD_ONCE_INIT, load_watchdog_once=PTHREAD_ONCE_INIT;
static FILE *load_stream(void) { return load_out ? load_out : stdout; }
static uint64_t load_now_us(void) {
    struct timespec now;
    return clock_gettime(CLOCK_MONOTONIC,&now) ? 0 : (uint64_t)now.tv_sec*1000000+(uint64_t)now.tv_nsec/1000;
}
static void load_configure(const char *trace,const char *limit,const char *watchdog) {
    const int saved_errno=errno;
    int mode=trace && !strcmp(trace,"1") ? LOAD_WAD : trace && !strcmp(trace,"all") ? LOAD_ALL : LOAD_OFF;
    char *end=NULL;
    unsigned long long cap=limit && *limit ? strtoull(limit,&end,10) : 20000;
    if (end && *end) cap=20000;
    long seconds=watchdog && *watchdog ? strtol(watchdog,&end,10) : 0;
    if (watchdog && *watchdog && (*end || seconds<1 || seconds>3600)) seconds=0;
    pthread_mutex_lock(&lock);
    load_lines=mode!=LOAD_OFF;
    load_mode=mode!=LOAD_OFF ? mode : seconds ? LOAD_WAD : LOAD_OFF;
    load_limit=cap; load_written=0; load_reported=0;
    load_watchdog_seconds=(unsigned)seconds;
    memset(load_flights,0,sizeof(load_flights));
    pthread_mutex_unlock(&lock);
    __atomic_store_n(&load_ready,1,__ATOMIC_RELEASE);
    if (load_lines) fprintf(load_stream(),"Load: trace on (mode=%s, limit=%llu)\n",mode==LOAD_ALL ? "all" : "wad",cap);
    if (load_watchdog_seconds) fprintf(load_stream(),"Load: watchdog on (%u s without file progress, mode=%s)\n",
                                       load_watchdog_seconds,load_mode==LOAD_ALL ? "all" : "wad");
    if (load_mode) fflush(load_stream());
    errno=saved_errno;
}
static void load_configure_env(void) {
    load_configure(getenv("SPARTAN_FILE_TRACE"),getenv("SPARTAN_FILE_TRACE_LIMIT"),getenv("SPARTAN_LOAD_WATCHDOG"));
}
static int load_active(void) {
    if (!__atomic_load_n(&load_ready,__ATOMIC_ACQUIRE)) {
        const int saved_errno=errno;
        pthread_once(&load_env_once,load_configure_env);
        errno=saved_errno;
    }
    return load_mode;
}
static int load_path_traced(const char *path) {
    if (load_mode==LOAD_ALL) return 1;
    size_t n=strlen(path);
    return n>=4 && path[n-4]=='.' && (path[n-3]|32)=='w' && (path[n-2]|32)=='a' && (path[n-1]|32)=='d';
}
/* One operation line may be written (the limit line is written once, by the first refused one). */
static int load_line_allowed(void) {
    if (!load_lines) return 0;
    if (!load_limit) return 1;
    uint64_t n=__atomic_fetch_add(&load_written,1,__ATOMIC_RELAXED);
    if (n==load_limit) {
        fprintf(load_stream(),"Load: trace limit %llu reached; state and watchdog keep running\n",(unsigned long long)load_limit);
        fflush(load_stream());
    }
    return n<load_limit;
}
static void load_clean(char *path) { for (; *path; ++path) if ((unsigned char)*path<32 || *path==127) *path='?'; }
static void load_watchdog_start(void);
/* guest: the path being opened; otherwise the file is found by fd and must be traced already. */
static LoadOp load_begin(const char *op,int fd,const char *guest,int64_t offset,uint64_t size) {
    LoadOp r={0};
    if (!load_active()) return r;
    const int saved_errno=errno;
    char path[256]="";
    pthread_mutex_lock(&lock);
    if (guest) { if (load_path_traced(guest)) snprintf(path,sizeof(path),"%s",guest); }
    else {
        File *f=get(fd);
        if (f && f->traced) {
            snprintf(path,sizeof(path),"%s",f->path);
            if (!strcmp(op,"read") && f->host>=0) offset=host_lseek(f->host,0,SEEK_CUR);
        }
    }
    if (!path[0]) { pthread_mutex_unlock(&lock); errno=saved_errno; return r; }
    load_clean(path);
    r.id=++load_sequence; r.started=load_now_us(); r.offset=offset; r.fd=fd; r.op=op; r.slot=-1;
    for (int i=0;i<LOAD_INFLIGHT;++i) if (!load_flights[i].id) {
        LoadFlight *s=&load_flights[i];
        *s=(LoadFlight){.id=r.id,.started=r.started,.offset=offset,.size=size,.fd=fd,.op=op,.tid=runtime_thread_host_id()};
        snprintf(s->thread,sizeof(s->thread),"%s",runtime_thread_current_name());
        snprintf(s->path,sizeof(s->path),"%s",path);
        r.slot=i; break;
    }
    load_last_us=r.started; load_last_id=r.id; load_reported=0;
    pthread_mutex_unlock(&lock);
    if (load_line_allowed()) {
        fprintf(load_stream(),"Load: #%llu begin %s fd=%d tid=%lu thread='%s' t_us=%llu off=%lld req=%llu path=%s\n",
                (unsigned long long)r.id,op,fd,runtime_thread_host_id(),runtime_thread_current_name(),
                (unsigned long long)r.started,(long long)offset,(unsigned long long)size,path);
        fflush(load_stream());
    }
    if (load_watchdog_seconds && load_watchdog_autostart) pthread_once(&load_watchdog_once,load_watchdog_start);
    errno=saved_errno;
    return r;
}
static void load_end(const LoadOp *op,int64_t result) {
    if (!op->id) return;
    const int saved_errno=errno;
    const uint64_t now=load_now_us();
    char extra[64]="";
    pthread_mutex_lock(&lock);
    if (op->slot>=0) load_flights[op->slot].id=0;
    File *f=!strcmp(op->op,"open") ? (result>=0 ? get((int)result) : NULL) : get(op->fd);
    if (f && !strcmp(op->op,"open")) {
        HostStat s;
        f->traced=1; f->opened_us=f->last_us=now;
        f->size=f->host>=0 && !host_fstat(f->host,&s) ? (int64_t)s.st_size : -1;
        snprintf(extra,sizeof(extra)," size=%lld",(long long)f->size);
    } else if (f && f->traced && strcmp(op->op,"close")) {
        if (result>0) {
            f->bytes+=(uint64_t)result; ++f->reads;
            if (op->offset>=0 && (uint64_t)op->offset<=f->covered && (uint64_t)op->offset+(uint64_t)result>f->covered)
                f->covered=(uint64_t)op->offset+(uint64_t)result;
        }
        f->last_us=now;
        snprintf(extra,sizeof(extra)," done=%llu/%lld",(unsigned long long)f->covered,(long long)f->size);
    }
    load_last_us=now; load_last_id=op->id; load_reported=0;
    pthread_mutex_unlock(&lock);
    if (load_line_allowed()) {
        fprintf(load_stream(),"Load: #%llu end %s fd=%d tid=%lu t_us=%llu result=%lld errno=%d us=%llu%s\n",
                (unsigned long long)op->id,op->op,!strcmp(op->op,"open") ? (int)(result>=0 ? result : -1) : op->fd,
                runtime_thread_host_id(),(unsigned long long)now,(long long)result,result<0 ? (int)-result : 0,
                (unsigned long long)(now>=op->started ? now-op->started : 0),extra);
        fflush(load_stream());
    }
    errno=saved_errno;
}
static double load_age(uint64_t now,uint64_t then) { return now>then ? (double)(now-then)/1e6 : 0.0; }
static void load_state_at(FILE *out,uint64_t now) {
    pthread_mutex_lock(&lock);
    unsigned flying=0;
    for (int i=0;i<LOAD_INFLIGHT;++i) flying+=load_flights[i].id!=0;
    fprintf(out,"Load: state t_us=%llu mode=%s last progress %.1f s ago (#%llu), %u in flight\n",(unsigned long long)now,
            load_mode==LOAD_ALL ? "all" : load_mode ? "wad" : "off",load_age(now,load_last_us),(unsigned long long)load_last_id,flying);
    for (int i=0;i<LOAD_INFLIGHT;++i) {
        const LoadFlight *s=&load_flights[i];
        if (s->id) fprintf(out,"Load:   in flight #%llu %s fd=%d tid=%lu thread='%s' for %.1f s off=%lld req=%llu path=%s\n",
                           (unsigned long long)s->id,s->op,s->fd,s->tid,s->thread,load_age(now,s->started),
                           (long long)s->offset,(unsigned long long)s->size,s->path);
    }
    for (int fd=3;fd<MAX_FILES;++fd) {
        const File *f=&files[fd];
        if (f->used && f->traced) fprintf(out,"Load:   open fd=%d path=%s read=%llu/%lld bytes=%llu reads=%llu opened %.1f s ago, last %.1f s ago\n",
                                          fd,f->path,(unsigned long long)f->covered,(long long)f->size,(unsigned long long)f->bytes,
                                          (unsigned long long)f->reads,load_age(now,f->opened_us),load_age(now,f->last_us));
    }
    pthread_mutex_unlock(&lock);
    fflush(out);
}
void runtime_file_load_state(FILE *out) {
    if (!load_active()) return;
    const int saved_errno=errno;
    load_state_at(out ? out : load_stream(),load_now_us());
    errno=saved_errno;
}
/* A load is stalled: an operation is running or a traced file is open and was read, and nothing
 * began or ended for the watchdog's time. Due once per stall (progress re-arms it), 8 per session. */
static int load_stall_due(uint64_t now) {
    if (!load_watchdog_seconds) return 0;
    int due=0;
    pthread_mutex_lock(&lock);
    int loading=0;
    for (int i=0;i<LOAD_INFLIGHT && !loading;++i) loading=load_flights[i].id!=0;
    for (int fd=3;fd<MAX_FILES && !loading;++fd) loading=files[fd].used && files[fd].traced && files[fd].reads;
    if (loading && !load_reported && load_reports<LOAD_REPORTS_MAX &&
        now>=load_last_us && now-load_last_us>=(uint64_t)load_watchdog_seconds*1000000) {
        load_reported=1; ++load_reports; due=1;
    }
    pthread_mutex_unlock(&lock);
    return due;
}
static void load_stall_report(FILE *out,uint64_t now) {
    const int saved_errno=errno;
    if (!out) out=load_stream();
    fprintf(out,"Load: STALL no file progress for %llu s (watchdog %u s)\n",
            (unsigned long long)(now>load_last_us ? (now-load_last_us)/1000000 : 0),load_watchdog_seconds);
    load_state_at(out,now);
    /* Two samples a second apart: CPU time rising means a busy loop, flat means a wait. */
    runtime_thread_load_snapshot(out,1);
    fflush(out);
    struct timespec second={1,0};
    while (nanosleep(&second,&second) && errno==EINTR) {}
    runtime_thread_load_snapshot(out,2);
    fflush(out);
    errno=saved_errno;
}
static void *load_watchdog(void *arg) {
    (void)arg;
    pthread_setname_np(pthread_self(),"load-watchdog");
    for (;;) {
        struct timespec quarter={0,250000000};
        nanosleep(&quarter,NULL);
        const uint64_t now=load_now_us();
        if (load_stall_due(now)) load_stall_report(NULL,now);
    }
    return NULL;
}
static void load_watchdog_start(void) {
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread,&attr,load_watchdog,NULL)) fputs("Load: watchdog thread not started\n",load_stream());
    pthread_attr_destroy(&attr);
}
/* Game mounts (including linked mod overlays) are read-only. Saves use other mounts. */
static int game_path(const char *p) {
    if (!p || !*p) return 0;
    if (*p!='/') return 1;
    return (!strncmp(p,"/app0",5) && (!p[5] || p[5]=='/')) ||
           (!strncmp(p,"/hostapp",8) && (!p[8] || p[8]=='/'));
}
/* All operations return >=0 or -(host errno); wrappers adapt the convention. */
static int64_t open_file(const char *guest,int flags,int mode) {
    if (game_path(guest) && (flags & (3|0x8|0x200|0x400|0x800))) return -EROFS;
    char path[1024];
    int e=translate(guest,path,sizeof(path));
    if (e) return -e;
#ifdef _WIN32
    /* Windows cannot open a directory as a CRT descriptor: directories are listings only. */
    (void)mode;
    HostStat s;
    Listing *dir=NULL;
    int host=-1;
    if (!host_stat(path,&s) && S_ISDIR(s.st_mode)) {
        if (flags&3) return -EISDIR;
        if (!(dir=list_directory(path))) return -EACCES;
    } else if (flags&0x20000) {
        e=errno==ENOENT ? ENOENT : ENOTDIR;
        if (e==ENOENT) { ++missing; printf("Runtime: open(%s) -> not found\n",guest); }
        return -e;
    } else {
        host=open(path,host_flags(flags)&~O_DIRECTORY,_S_IREAD|_S_IWRITE);
        if (host<0) {
            e=errno;
            if (e==ENOENT) { ++missing; printf("Runtime: open(%s) -> not found\n",guest); }
            return -e;
        }
        if (host_fstat(host,&s)) memset(&s,0,sizeof(s));
    }
#else
    int host=open(path,host_flags(flags),mode ? mode : 0644);
    if (host<0) {
        e=errno;
        if (e==ENOENT) { ++missing; printf("Runtime: open(%s) -> not found\n",guest); }
        return -e;
    }
    struct stat s;
    Listing *dir=NULL;
    if (!fstat(host,&s) && S_ISDIR(s.st_mode)) dir=list_directory(path);
#endif
    pthread_mutex_lock(&lock);
    int fd=-1;
    for (int i=3;i<MAX_FILES;++i) if (!files[i].used) { fd=i; break; }
    if (fd<0) { pthread_mutex_unlock(&lock); if (host>=0) close(host); free_listing(dir); return -EMFILE; }
    files[fd]=(File){.used=1,.host=host,.dir=dir};
    snprintf(files[fd].path,sizeof(files[fd].path),"%s",guest);
    ++opens;
    pthread_mutex_unlock(&lock);
    if (audio_trace() && strstr(guest,"sound/")) printf("Audio trace: open(%s) -> fd %d, %lld bytes\n",guest,fd,(long long)s.st_size);
    const char *mod_trace=getenv("SPARTAN_MOD_TRACE"), *mod_root=getenv("SPARTAN_MODS_DIR");
    if (mod_trace && mod_trace[0]=='1' && mod_root) {
        char actual[PATH_MAX],root[PATH_MAX];
        static unsigned traced;
        if (realpath(path,actual) && realpath(mod_root,root)) {
            size_t n=strlen(root);
            if (!strncmp(actual,root,n) && (actual[n]=='/' || actual[n]=='\\') &&
                __atomic_fetch_add(&traced,1,__ATOMIC_RELAXED)<32)
                printf("Mods: open %s -> %s\n",guest,actual);
        }
    }
    return fd;
}
static int64_t do_open(const char *guest,int flags,int mode) {
    LoadOp op=load_begin("open",-1,guest,-1,0);
    int64_t r=open_file(guest,flags,mode);
    load_end(&op,r);
    return r;
}
static int64_t close_file(int fd) {
    if (fd>=0 && fd<3) return 0;
    pthread_mutex_lock(&lock);
    File *f=get(fd);
    if (!f) { pthread_mutex_unlock(&lock); return -EBADF; }
    if (f->host>=0) close(f->host);
    free_listing(f->dir);
    *f=(File){0};
    pthread_mutex_unlock(&lock);
    return 0;
}
static int64_t do_close(int fd) {
    LoadOp op=load_begin("close",fd,NULL,-1,0);
    int64_t r=close_file(fd);
    load_end(&op,r);
    return r;
}
static int host_fd(int fd) {
    if (fd>=0 && fd<3) return fd;
    File *f=get(fd);
    return f ? f->host : -1;
}
#ifdef _WIN32
/* The CRT reads and writes at most INT_MAX bytes per call. */
static ssize_t host_read(int h,void *buffer,uint64_t size) { return read(h,buffer,(unsigned)(size>0x7ffff000 ? 0x7ffff000 : size)); }
static ssize_t host_write(int h,const void *buffer,uint64_t size) { return write(h,buffer,(unsigned)(size>0x7ffff000 ? 0x7ffff000 : size)); }
#else
#define host_read read
#define host_write write
#endif
/* Pages of the destination may be write-protected for GPU tracking: the kernel's copy then
 * fails with EFAULT instead of faulting to our handler. A user-mode write to each page first
 * goes through the handler, which unprotects it (and records the upcoming write). */
static void touch_for_write(void *buffer,uint64_t size) {
    if (!size) return;
    uintptr_t p=(uintptr_t)buffer & ~(uintptr_t)4095, end=(uintptr_t)buffer+size;
    for (; p<end; p+=4096) {
        volatile unsigned char *b=(volatile unsigned char *)(p<(uintptr_t)buffer ? (uintptr_t)buffer : p);
        *b=*b;
    }
}
static int64_t read_file(int fd,void *buffer,uint64_t size) {
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    touch_for_write(buffer,size);
    const uint64_t wait_start=runtime_wait_begin();
    ssize_t n=host_read(h,buffer,size);
    runtime_wait_end("file read",wait_start,"bytes",(long long)size);
    if (n<0) { if (audio_trace()) printf("Audio trace: read(fd %d, %llu) failed, errno %d\n",fd,(unsigned long long)size,errno); return -errno; }
    __atomic_add_fetch(&reads,1,__ATOMIC_RELAXED); __atomic_add_fetch(&bytes_read,(uint64_t)n,__ATOMIC_RELAXED);
    return n;
}
static int64_t pread_file(int fd,void *buffer,uint64_t size,int64_t offset) {
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    touch_for_write(buffer,size);
    const uint64_t wait_start=runtime_wait_begin();
    ssize_t n=pread(h,buffer,size,offset);
    runtime_wait_end("file pread",wait_start,"bytes",(long long)size);
    if (n<0) { if (audio_trace()) printf("Audio trace: pread(fd %d, %llu @%lld) failed, errno %d\n",fd,(unsigned long long)size,(long long)offset,errno); return -errno; }
    __atomic_add_fetch(&reads,1,__ATOMIC_RELAXED); __atomic_add_fetch(&bytes_read,(uint64_t)n,__ATOMIC_RELAXED);
    return n;
}
static int64_t do_read(int fd,void *buffer,uint64_t size) {
    LoadOp op=load_begin("read",fd,NULL,-1,size);
    int64_t r=read_file(fd,buffer,size);
    load_end(&op,r);
    return r;
}
static int64_t do_pread(int fd,void *buffer,uint64_t size,int64_t offset) {
    LoadOp op=load_begin("pread",fd,NULL,offset,size);
    int64_t r=pread_file(fd,buffer,size,offset);
    load_end(&op,r);
    return r;
}
static int64_t do_write(int fd,const void *buffer,uint64_t size) {
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    ssize_t n=host_write(h,buffer,size);
    if (n<0) return -errno;
    __atomic_add_fetch(&writes,1,__ATOMIC_RELAXED);
    return n;
}
static int64_t do_pwrite(int fd,const void *buffer,uint64_t size,int64_t offset) {
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    ssize_t n=pwrite(h,buffer,size,offset);
    return n<0 ? -errno : n;
}
static int64_t do_lseek(int fd,int64_t offset,int whence) {
    File *f=get(fd);
    if (!f) return -EBADF;
    if (whence<0 || whence>2) return -EINVAL;
    if (f->dir) {
        /* Directory offsets are entry indices for getdirentries. */
        if (whence==0 && offset>=0) { f->position=(size_t)offset; return offset; }
        return -EINVAL;
    }
    int64_t r=host_lseek(f->host,offset,whence);
    return r<0 ? -errno : r;
}
static int64_t do_fstat(int fd,GuestStat *out) {
    int h=host_fd(fd);
    HostStat s;
#ifdef _WIN32
    if (h<0) { /* directory: a listing only, stat the path it was opened with */
        File *f=get(fd); char path[1024];
        if (!f || !f->dir) return -EBADF;
        if (!out) return -EFAULT;
        if (translate(f->path,path,sizeof(path)) || host_stat(path,&s)) return -EBADF;
        convert_stat(&s,out); return 0;
    }
#else
    if (h<0) return -EBADF;
#endif
    if (!out) return -EFAULT;
    if (host_fstat(h,&s)) return -errno;
    convert_stat(&s,out); return 0;
}
static int64_t do_stat(const char *guest,GuestStat *out) {
    char path[1024]; HostStat s;
    int e=translate(guest,path,sizeof(path));
    if (e) return -e;
    if (!out) return -EFAULT;
    if (host_stat(path,&s)) return -errno;
    convert_stat(&s,out); return 0;
}
static int64_t do_getdents(int fd,char *buffer,uint64_t size,int64_t *basep) {
    pthread_mutex_lock(&lock);
    File *f=get(fd);
    int64_t result=0;
    if (!f) result=-EBADF;
    else if (!f->dir) result=-EINVAL;
    else if (!buffer) result=-EFAULT;
    else if (size<512) result=-EINVAL;
    else {
        if (basep) *basep=(int64_t)f->position;
        uint64_t written=0;
        while (f->position<f->dir->count) {
            const char *name=f->dir->names+f->dir->offsets[f->position];
            size_t n=strlen(name); if (n>255) n=255;
            uint16_t reclen=(uint16_t)((8+n+1+7)&~(size_t)7);
            if (written+reclen>size) break;
            char *p=buffer+written;
            memset(p,0,reclen);
            uint32_t ino=(uint32_t)f->position+1;
            memcpy(p,&ino,4); memcpy(p+4,&reclen,2);
            p[6]=(char)f->dir->types[f->position]; p[7]=(char)n;
            memcpy(p+8,name,n);
            written+=reclen; ++f->position;
        }
        result=(int64_t)written;
    }
    pthread_mutex_unlock(&lock);
    return result;
}
static int64_t path_op(const char *guest,int op,int mode) {
    if (game_path(guest)) return -EROFS;
    char path[1024];
    int e=translate(guest,path,sizeof(path));
    if (e) return -e;
    int r= op==0 ? mkdir(path,mode ? mode : 0755) : op==1 ? rmdir(path) : unlink(path);
    return r ? -errno : 0;
}
static int64_t do_rename(const char *from,const char *to) {
    if (game_path(from) || game_path(to)) return -EROFS;
    char a[1024],b[1024];
    int e=translate(from,a,sizeof(a));
    if (!e) e=translate(to,b,sizeof(b));
    if (e) return -e;
#ifdef _WIN32
    /* POSIX rename replaces an existing target; the CRT's fails. */
    return MoveFileExA(a,b,MOVEFILE_REPLACE_EXISTING) ? 0 : -compat_errno_from_win32(GetLastError());
#else
    return rename(a,b) ? -errno : 0;
#endif
}
static int64_t do_ftruncate(int fd,int64_t length) {
    int h=host_fd(fd);
    if (h<0) return -EBADF;
#ifdef _WIN32
    return _chsize_s(h,length) ? -errno : 0;
#else
    return ftruncate(h,length) ? -errno : 0;
#endif
}
static int64_t do_truncate(const char *guest,int64_t length) {
    if (game_path(guest)) return -EROFS;
    char path[1024];
    int e=translate(guest,path,sizeof(path));
    if (e) return -e;
#ifdef _WIN32
    int h=open(path,O_RDWR|O_BINARY);
    if (h<0) return -errno;
    int r=_chsize_s(h,length) ? -errno : 0;
    close(h);
    return r;
#else
    return truncate(path,length) ? -errno : 0;
#endif
}
static int64_t do_fsync(int fd) { int h=host_fd(fd); if (h<0) return -EBADF; return fsync(h) ? -errno : 0; }
static int64_t do_access(const char *guest,int mode) {
    char path[1024];
    int e=translate(guest,path,sizeof(path));
    if (e) return -e;
#ifdef _WIN32
    return access(path,mode&6) ? -errno : 0; /* X_OK is invalid for the CRT */
#else
    return access(path,mode&7) ? -errno : 0;
#endif
}

/* Convention adapters: sceKernel* -> Orbis error codes, POSIX -> -1 + errno. */
static int64_t sce(int64_t r) { return r<0 ? ERR(runtime_guest_errno((int)-r)) : r; }
static int64_t posix(int64_t r) { if (r<0) { *runtime_errno()=runtime_guest_errno((int)-r); return -1; } return r; }
#define PAIR(name,params,args) \
    static ABI int64_t sce_##name params { return sce(do_##name args); } \
    static ABI int64_t posix_##name params { return posix(do_##name args); }
PAIR(open,(const char *p,int f,int m),(p,f,m))
PAIR(close,(int fd),(fd))
PAIR(read,(int fd,void *b,uint64_t n),(fd,b,n))
PAIR(pread,(int fd,void *b,uint64_t n,int64_t o),(fd,b,n,o))
PAIR(write,(int fd,const void *b,uint64_t n),(fd,b,n))
PAIR(pwrite,(int fd,const void *b,uint64_t n,int64_t o),(fd,b,n,o))
PAIR(lseek,(int fd,int64_t o,int w),(fd,o,w))
PAIR(fstat,(int fd,GuestStat *s),(fd,s))
PAIR(stat,(const char *p,GuestStat *s),(p,s))
PAIR(getdents,(int fd,char *b,uint64_t n,int64_t *base),(fd,b,n,base))
PAIR(rename,(const char *a,const char *b),(a,b))
PAIR(ftruncate,(int fd,int64_t l),(fd,l))
PAIR(truncate,(const char *p,int64_t l),(p,l))
PAIR(fsync,(int fd),(fd))
static ABI int64_t posix_access(const char *p,int m) { return posix(do_access(p,m)); }
static ABI int64_t sce_mkdir(const char *p,int m) { return sce(path_op(p,0,m)); }
static ABI int64_t posix_mkdir(const char *p,int m) { return posix(path_op(p,0,m)); }
static ABI int64_t sce_rmdir(const char *p) { return sce(path_op(p,1,0)); }
static ABI int64_t posix_rmdir(const char *p) { return posix(path_op(p,1,0)); }
static ABI int64_t sce_unlink(const char *p) { return sce(path_op(p,2,0)); }
static ABI int64_t posix_unlink(const char *p) { return posix(path_op(p,2,0)); }
static ABI int32_t sce_check_reachability(const char *p) {
    GuestStat s; return (int32_t)sce(do_stat(p,&s));
}

static const RuntimeExport exports[]={
    {"sceKernelOpen",sce_open}, {"open",posix_open}, {"_open",posix_open},
    {"sceKernelClose",sce_close}, {"close",posix_close}, {"_close",posix_close},
    {"sceKernelRead",sce_read}, {"read",posix_read}, {"_read",posix_read},
    {"sceKernelPread",sce_pread}, {"pread",posix_pread},
    {"sceKernelWrite",sce_write}, {"write",posix_write}, {"_write",posix_write},
    {"sceKernelPwrite",sce_pwrite}, {"pwrite",posix_pwrite},
    {"sceKernelLseek",sce_lseek}, {"lseek",posix_lseek},
    {"sceKernelFstat",sce_fstat}, {"fstat",posix_fstat},
    {"sceKernelStat",sce_stat}, {"stat",posix_stat},
    {"sceKernelGetdirentries",sce_getdents}, {"getdirentries",posix_getdents},
    {"sceKernelRename",sce_rename}, {"rename",posix_rename},
    {"sceKernelFtruncate",sce_ftruncate}, {"ftruncate",posix_ftruncate},
    {"sceKernelTruncate",sce_truncate}, {"truncate",posix_truncate},
    {"sceKernelFsync",sce_fsync}, {"fsync",posix_fsync},
    {"access",posix_access}, {"sceKernelCheckReachability",sce_check_reachability},
    {"sceKernelMkdir",sce_mkdir}, {"mkdir",posix_mkdir},
    {"sceKernelRmdir",sce_rmdir}, {"rmdir",posix_rmdir},
    {"sceKernelUnlink",sce_unlink}, {"unlink",posix_unlink},
};
uintptr_t runtime_file_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
/* Host-side helpers for other modules (e.g. SaveData, Fios). */
int64_t runtime_file_open(const char *p,int f,int m) { return do_open(p,f,m); }
int64_t runtime_file_close(int fd) { return do_close(fd); }
int64_t runtime_file_read(int fd,void *b,uint64_t n) { return do_read(fd,b,n); }
int64_t runtime_file_pread(int fd,void *b,uint64_t n,int64_t o) { return do_pread(fd,b,n,o); }
int64_t runtime_file_write(int fd,const void *b,uint64_t n) { return do_write(fd,b,n); }
int64_t runtime_file_lseek(int fd,int64_t o,int w) { return do_lseek(fd,o,w); }
int64_t runtime_file_stat(const char *p,void *s) { return do_stat(p,s); }
int64_t runtime_file_fstat(int fd,void *s) { return do_fstat(fd,s); }
int64_t runtime_file_getdents(int fd,char *b,uint64_t n,int64_t *base) { return do_getdents(fd,b,n,base); }
int runtime_file_translate(const char *guest,char *out,size_t size) { return translate(guest,out,size); }
void runtime_file_report(void) {
    printf("Runtime: files opened=%zu, reads=%zu (%llu bytes), writes=%zu, not found=%zu\n",
           opens,reads,(unsigned long long)bytes_read,writes,missing);
    /* Traced files still open at exit and the last progress (Load: trace or watchdog on). */
    runtime_file_load_state(NULL);
}

