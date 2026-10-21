/* Guest file system: PS4 mount points mapped onto host directories, or onto a packed ZArchive
 * (.zar) game image.
 *   /app0, /hostapp  -> game package root (read-only by convention)
 *   /temp0, /download0, /data, and mounts added by SaveData -> user directory
 * Guest descriptors are small integers in our own table; stdio 0-2 pass through.
 * Paths containing ".." components are rejected rather than normalized.
 * With a .zar game image the same guest paths resolve inside the archive: reads decompress the
 * 64 KiB blocks they touch, and the archive is as read-only as the folder it replaces. A host
 * mount over the archive (the glyphs' and the pause menu's copy of one WAD) still wins for that
 * path. */
#define _GNU_SOURCE
#include "runtime.h"
#include "runtime_zarchive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
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
#define MAX_MOUNTS 16

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
/* An open guest file: a host descriptor, or a node of the mounted archive (host is -1), plus a
 * directory listing when the path was a directory. `position` is the guest file offset. */
typedef struct {
    int used, host;
    Listing *dir;
    size_t position;
    char path[512];
    ZArchive *archive;
    int64_t node;
    uint64_t size;
} File;
typedef struct { char guest[64]; char host[512]; ZArchive *archive; } Mount;
static File files[MAX_FILES];
static Mount mounts[MAX_MOUNTS];
static size_t mount_count, opens, reads, writes, missing;
static uint64_t bytes_read;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

/* guest=host, or guest=archive (host empty and archive set). */
static int mount_point(const char *guest,const char *host,ZArchive *archive) {
    pthread_mutex_lock(&lock);
    for (size_t i=0;i<mount_count;++i) if (!strcmp(mounts[i].guest,guest)) {
        snprintf(mounts[i].host,sizeof(mounts[i].host),"%s",host);
        mounts[i].archive=archive;
        pthread_mutex_unlock(&lock); return 0;
    }
    if (mount_count==MAX_MOUNTS || strlen(guest)>=64 || strlen(host)>=512) { pthread_mutex_unlock(&lock); return -1; }
    snprintf(mounts[mount_count].guest,64,"%s",guest);
    snprintf(mounts[mount_count].host,512,"%s",host);
    mounts[mount_count].archive=archive;
    ++mount_count;
    pthread_mutex_unlock(&lock);
    return 0;
}
int runtime_file_mount(const char *guest,const char *host) { return mount_point(guest,host,NULL); }
static int mount_archive(const char *guest,ZArchive *archive) { return mount_point(guest,"",archive); }
void runtime_file_unmount(const char *guest) {
    pthread_mutex_lock(&lock);
    for (size_t i=0;i<mount_count;++i) if (!strcmp(mounts[i].guest,guest)) {
        mounts[i]=mounts[--mount_count]; break;
    }
    pthread_mutex_unlock(&lock);
}
static char user_root[512]="user";
const char *runtime_file_user_dir(void) { return user_root; }
/* A .zar is chosen by its name: opening a directory as an archive is a Linux-only accident. */
static int archive_name(const char *path) {
    if (!path) return 0;
    size_t n = strlen(path);
    return n > 4 && path[n-4]=='.' &&
           tolower((unsigned char)path[n-3])=='z' &&
           tolower((unsigned char)path[n-2])=='a' &&
           tolower((unsigned char)path[n-1])=='r';
}
void runtime_file_configure(const char *app0,const char *user) {
    char path[600];
    snprintf(user_root,sizeof(user_root),"%s",user);
    ZArchive *archive = archive_name(app0) ? zarchive_open(app0) : NULL;
    if (archive) {
        mount_archive("/app0",archive);
        mount_archive("/hostapp",archive);
        printf("Runtime: /app0 is the ZArchive %s (%u entries)\n",app0,zarchive_entries(archive));
    } else {
        if (archive_name(app0)) printf("Runtime: %s is not usable as a ZArchive (%s); using it as a folder\n",app0,zarchive_error());
        runtime_file_mount("/app0",app0);
        runtime_file_mount("/hostapp",app0);
    }
    const char *writable[]={"temp0","download0","data"};
    mkdir(user,0755);
    for (int i=0;i<3;++i) {
        snprintf(path,sizeof(path),"%s/%s",user,writable[i]);
        mkdir(path,0755);
        char guest[32]; snprintf(guest,sizeof(guest),"/%s",writable[i]);
        runtime_file_mount(guest,path);
    }
}
/* What a guest path resolves to: a host path, or an archive node. */
typedef struct { ZArchive *archive; int64_t node; char host[1024]; } Target;
/* Resolve a guest path; returns 0 or a host errno. The longest matching mount wins, so an exact
 * file mount over the game (glyphs, pause menu) takes precedence over the archive's copy. */
static int resolve(const char *guest,Target *target) {
    target->archive=NULL; target->node=ZARCHIVE_NONE; target->host[0]=0;
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
    else if (m->archive) {
        const char *inside=buffer+best;
        while (*inside=='/') ++inside;
        target->archive=m->archive;
        target->node=zarchive_find(m->archive,inside);
        if (target->node==ZARCHIVE_NONE) result=ENOENT;
    }
    else if ((size_t)snprintf(target->host,sizeof(target->host),"%s%s",m->host,buffer+best)>=sizeof(target->host)) result=ENAMETOOLONG;
    pthread_mutex_unlock(&lock);
    if (result==ENOENT && !m) fprintf(stderr,"Runtime: no mount for guest path %s\n",guest);
    return result;
}
/* Resolve a guest path to a host path (no archive). */
int runtime_file_translate(const char *guest,char *out,size_t size) {
    Target target;
    int e=resolve(guest,&target);
    if (e) return e;
    if (target.archive) return ENOENT;
    if ((size_t)snprintf(out,size,"%s",target.host)>=size) return ENAMETOOLONG;
    return 0;
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
static Listing *new_listing(void) {
    Listing *l=calloc(1,sizeof(*l));
    if (!l) { fputs("Out of memory listing directory\n",stderr); exit(1); }
    return l;
}
static void listing_add(Listing *l,size_t *capacity,size_t *bytes,size_t *cap_names,
                        const char *name,unsigned char type) {
    size_t n=strlen(name)+1;
    if (l->count==*capacity) {
        *capacity=*capacity ? *capacity*2 : 64;
        l->offsets=realloc(l->offsets,*capacity*sizeof(size_t));
        l->types=realloc(l->types,*capacity);
    }
    if (*bytes+n>*cap_names) { *cap_names=(*bytes+n)*2; l->names=realloc(l->names,*cap_names); }
    if (!l->offsets || !l->types || !l->names) { fputs("Out of memory listing directory\n",stderr); exit(1); }
    memcpy(l->names+*bytes,name,n);
    l->offsets[l->count]=*bytes;
    l->types[l->count]=type;
    ++l->count; *bytes+=n;
}
static Listing *list_directory(const char *path) {
    DIR *d=opendir(path);
    if (!d) return NULL;
    Listing *l=new_listing();
    size_t capacity=0,bytes=0,cap_names=0;
    struct dirent *e;
    while (l && (e=readdir(d))) {
        unsigned char type;
#ifdef _WIN32
        /* MinGW's dirent has no d_type. */
        char child[1100]; HostStat entry;
        snprintf(child,sizeof(child),"%s/%s",path,e->d_name);
        type=host_stat(child,&entry) ? 0 : S_ISDIR(entry.st_mode) ? 4 : S_ISREG(entry.st_mode) ? 8 : 0;
#else
        type=e->d_type;
        if (type==DT_LNK || type==DT_UNKNOWN) {
            struct stat entry;
            if (!fstatat(dirfd(d),e->d_name,&entry,0))
                type=S_ISDIR(entry.st_mode) ? DT_DIR : S_ISREG(entry.st_mode) ? DT_REG : type;
        }
        type=type==DT_DIR ? 4 : type==DT_REG ? 8 : type==DT_LNK ? 10 : 0;
#endif
        listing_add(l,&capacity,&bytes,&cap_names,e->d_name,type);
    }
    closedir(d);
    return l;
}
/* A directory of the mounted archive, in the same listing form as a host directory. */
static Listing *list_archive(ZArchive *archive,int64_t node) {
    Listing *l=new_listing();
    size_t capacity=0,bytes=0,cap_names=0;
    char name[256];
    int is_file=0;
    for (uint32_t i=0;zarchive_child(archive,node,i,name,sizeof(name),&is_file);++i)
        listing_add(l,&capacity,&bytes,&cap_names,name,is_file ? 8 : 4);
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
/* The archive has no timestamps: the guest reads a full-sized file or directory, mode from its
 * kind, and a zero time. */
static void archive_stat(ZArchive *archive,int64_t node,GuestStat *g) {
    int is_file=zarchive_is_file(archive,node);
    memset(g,0,sizeof(*g));
    g->dev=1; g->ino=(uint32_t)(node+1); g->nlink=1;
    g->size=(int64_t)(is_file ? zarchive_entry_size(archive,node) : 0);
    g->mode=(uint16_t)(is_file ? 0100644 : 0040755);
    g->blocks=(g->size+511)/512; g->blksize=4096;
}
static File *get(int fd) {
    if (fd<3 || fd>=MAX_FILES || !files[fd].used) return NULL;
    return &files[fd];
}
/* SPARTAN_AUDIO_TRACE=1: sound file opens and failed reads (missing game sounds). */
static int audio_trace(void) { static int v=-1; if (v<0) { const char *e=getenv("SPARTAN_AUDIO_TRACE"); v=e && e[0]=='1'; } return v; }
/* Game mounts (including linked mod overlays) are read-only. Saves use other mounts. */
static int game_path(const char *p) {
    if (!p || !*p) return 0;
    if (*p!='/') return 1;
    return (!strncmp(p,"/app0",5) && (!p[5] || p[5]=='/')) ||
           (!strncmp(p,"/hostapp",8) && (!p[8] || p[8]=='/'));
}
/* All operations return >=0 or -(host errno); wrappers adapt the convention. */
static int64_t do_open(const char *guest,int flags,int mode) {
    if (game_path(guest) && (flags & (3|0x8|0x200|0x400|0x800))) return -EROFS;
    Target target;
    int e=resolve(guest,&target);
    if (e) return -e;
    if (target.archive) {
        int is_file=zarchive_is_file(target.archive,target.node);
        if (is_file && (flags&0x20000)) return -ENOTDIR;
        if (!is_file && (flags&3)) return -EISDIR;
        Listing *dir=is_file ? NULL : list_archive(target.archive,target.node);
        if (!is_file && !dir) return -EACCES;
        pthread_mutex_lock(&lock);
        int fd=-1;
        for (int i=3;i<MAX_FILES;++i) if (!files[i].used) { fd=i; break; }
        if (fd<0) { pthread_mutex_unlock(&lock); free_listing(dir); return -EMFILE; }
        files[fd]=(File){.used=1,.host=-1,.dir=dir,.archive=target.archive,.node=target.node,
                         .size=is_file ? zarchive_entry_size(target.archive,target.node) : 0};
        snprintf(files[fd].path,sizeof(files[fd].path),"%s",guest);
        ++opens;
        pthread_mutex_unlock(&lock);
        if (audio_trace() && strstr(guest,"sound/"))
            printf("Audio trace: open(%s) -> fd %d, %llu bytes (archive)\n",guest,fd,(unsigned long long)files[fd].size);
        return fd;
    }
#ifdef _WIN32
    /* Windows cannot open a directory as a CRT descriptor: directories are listings only. */
    (void)mode;
    HostStat s;
    Listing *dir=NULL;
    int host=-1;
    if (!host_stat(target.host,&s) && S_ISDIR(s.st_mode)) {
        if (flags&3) return -EISDIR;
        if (!(dir=list_directory(target.host))) return -EACCES;
    } else if (flags&0x20000) {
        e=errno==ENOENT ? ENOENT : ENOTDIR;
        if (e==ENOENT) { ++missing; printf("Runtime: open(%s) -> not found\n",guest); }
        return -e;
    } else {
        host=open(target.host,host_flags(flags)&~O_DIRECTORY,_S_IREAD|_S_IWRITE);
        if (host<0) {
            e=errno;
            if (e==ENOENT) { ++missing; printf("Runtime: open(%s) -> not found\n",guest); }
            return -e;
        }
        if (host_fstat(host,&s)) memset(&s,0,sizeof(s));
    }
#else
    int host=open(target.host,host_flags(flags),mode ? mode : 0644);
    if (host<0) {
        e=errno;
        if (e==ENOENT) { ++missing; printf("Runtime: open(%s) -> not found\n",guest); }
        return -e;
    }
    struct stat s;
    Listing *dir=NULL;
    if (!fstat(host,&s) && S_ISDIR(s.st_mode)) dir=list_directory(target.host);
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
        if (realpath(target.host,actual) && realpath(mod_root,root)) {
            size_t n=strlen(root);
            if (!strncmp(actual,root,n) && (actual[n]=='/' || actual[n]=='\\') &&
                __atomic_fetch_add(&traced,1,__ATOMIC_RELAXED)<32)
                printf("Mods: open %s -> %s\n",guest,actual);
        }
    }
    return fd;
}
static int64_t do_close(int fd) {
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
/* Reads of a guest file at an explicit offset (archive files ignore the descriptor's position). */
static int64_t archive_read(File *f,void *buffer,uint64_t size,int64_t offset) {
    if (f->dir) return -EISDIR;
    if (offset<0) return -EINVAL;
    if ((uint64_t)offset>=(uint64_t)f->size) return 0;
    uint64_t available=(uint64_t)f->size-(uint64_t)offset;
    if (size>available) size=available;
    touch_for_write(buffer,size);
    uint64_t n=zarchive_read(f->archive,f->node,(uint64_t)offset,buffer,size);
    if (!n && size) return -EIO;
    __atomic_add_fetch(&reads,1,__ATOMIC_RELAXED); __atomic_add_fetch(&bytes_read,n,__ATOMIC_RELAXED);
    return (int64_t)n;
}
static int64_t do_read(int fd,void *buffer,uint64_t size) {
    File *f=get(fd);
    if (f && f->archive) {
        int64_t n=archive_read(f,buffer,size,(int64_t)f->position);
        if (n>0) f->position+=(size_t)n;
        return n;
    }
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    touch_for_write(buffer,size);
    ssize_t n=host_read(h,buffer,size);
    if (n<0) { if (audio_trace()) printf("Audio trace: read(fd %d, %llu) failed, errno %d\n",fd,(unsigned long long)size,errno); return -errno; }
    __atomic_add_fetch(&reads,1,__ATOMIC_RELAXED); __atomic_add_fetch(&bytes_read,(uint64_t)n,__ATOMIC_RELAXED);
    return n;
}
static int64_t do_pread(int fd,void *buffer,uint64_t size,int64_t offset) {
    File *f=get(fd);
    if (f && f->archive) return archive_read(f,buffer,size,offset);
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    touch_for_write(buffer,size);
    ssize_t n=pread(h,buffer,size,offset);
    if (n<0) { if (audio_trace()) printf("Audio trace: pread(fd %d, %llu @%lld) failed, errno %d\n",fd,(unsigned long long)size,(long long)offset,errno); return -errno; }
    __atomic_add_fetch(&reads,1,__ATOMIC_RELAXED); __atomic_add_fetch(&bytes_read,(uint64_t)n,__ATOMIC_RELAXED);
    return n;
}
static int64_t do_write(int fd,const void *buffer,uint64_t size) {
    File *f=get(fd);
    if (f && f->archive) return -EROFS;
    int h=host_fd(fd);
    if (h<0) return -EBADF;
    ssize_t n=host_write(h,buffer,size);
    if (n<0) return -errno;
    __atomic_add_fetch(&writes,1,__ATOMIC_RELAXED);
    return n;
}
static int64_t do_pwrite(int fd,const void *buffer,uint64_t size,int64_t offset) {
    File *f=get(fd);
    if (f && f->archive) return -EROFS;
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
    if (f->archive) {
        int64_t base=whence==0 ? 0 : whence==1 ? (int64_t)f->position : (int64_t)f->size;
        if (whence==0 && offset<0) return -EINVAL;
        int64_t next=base+offset;
        if (next<0) return -EINVAL;
        f->position=(size_t)next;
        return next;
    }
    int64_t r=host_lseek(f->host,offset,whence);
    return r<0 ? -errno : r;
}
static int64_t do_fstat(int fd,GuestStat *out) {
    File *f=get(fd);
    if (!f) return -EBADF;
    if (!out) return -EFAULT;
    if (f->archive) { archive_stat(f->archive,f->node,out); return 0; }
    int h=host_fd(fd);
    HostStat s;
#ifdef _WIN32
    if (h<0) { /* directory: a listing only, stat the path it was opened with */
        if (!f->dir) return -EBADF;
        Target target;
        if (resolve(f->path,&target) || target.archive || host_stat(target.host,&s)) return -EBADF;
        convert_stat(&s,out); return 0;
    }
#else
    if (h<0) return -EBADF;
#endif
    if (host_fstat(h,&s)) return -errno;
    convert_stat(&s,out); return 0;
}
static int64_t do_stat(const char *guest,GuestStat *out) {
    Target target; HostStat s;
    int e=resolve(guest,&target);
    if (e) return -e;
    if (!out) return -EFAULT;
    if (target.archive) { archive_stat(target.archive,target.node,out); return 0; }
    if (host_stat(target.host,&s)) return -errno;
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
    Target target;
    int e=resolve(guest,&target);
    if (e) return -e;
    if (target.archive) return -EROFS;
    int r= op==0 ? mkdir(target.host,mode ? mode : 0755) : op==1 ? rmdir(target.host) : unlink(target.host);
    return r ? -errno : 0;
}
static int64_t do_rename(const char *from,const char *to) {
    if (game_path(from) || game_path(to)) return -EROFS;
    Target a_target,b_target;
    int e=resolve(from,&a_target);
    if (!e) e=resolve(to,&b_target);
    if (e) return -e;
    if (a_target.archive || b_target.archive) return -EROFS;
    char a[1024],b[1024];
    snprintf(a,sizeof(a),"%s",a_target.host);
    snprintf(b,sizeof(b),"%s",b_target.host);
#ifdef _WIN32
    /* POSIX rename replaces an existing target; the CRT's fails. */
    return MoveFileExA(a,b,MOVEFILE_REPLACE_EXISTING) ? 0 : -compat_errno_from_win32(GetLastError());
#else
    return rename(a,b) ? -errno : 0;
#endif
}
static int64_t do_ftruncate(int fd,int64_t length) {
    File *f=get(fd);
    if (f && f->archive) return -EROFS;
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
    Target target;
    int e=resolve(guest,&target);
    if (e) return -e;
    if (target.archive) return -EROFS;
#ifdef _WIN32
    int h=open(target.host,O_RDWR|O_BINARY);
    if (h<0) return -errno;
    int r=_chsize_s(h,length) ? -errno : 0;
    close(h);
    return r;
#else
    return truncate(target.host,length) ? -errno : 0;
#endif
}
static int64_t do_fsync(int fd) {
    File *f=get(fd);
    if (f && f->archive) return 0;
    int h=host_fd(fd); if (h<0) return -EBADF; return fsync(h) ? -errno : 0;
}
static int64_t do_access(const char *guest,int mode) {
    Target target;
    int e=resolve(guest,&target);
    if (e) return -e;
#ifdef _WIN32
    if (target.archive) return (mode&2) ? -EROFS : 0;
    return access(target.host,mode&6) ? -errno : 0; /* X_OK is invalid for the CRT */
#else
    if (target.archive) return (mode&2) ? -EROFS : 0;
    return access(target.host,mode&7) ? -errno : 0;
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
/* A guest file read whole (the loader's param.sfo); at most `size` bytes, returns the length,
 * or -1 when the file is missing or larger than the buffer. */
int64_t runtime_file_read_all(const char *guest,void *buffer,uint64_t size) {
    Target target;
    int e=resolve(guest,&target);
    if (e) return -1;
    if (target.archive) {
        if (target.node==ZARCHIVE_NONE || !zarchive_is_file(target.archive,target.node)) return -1;
        uint64_t length=zarchive_entry_size(target.archive,target.node);
        if (length>size) return -1;
        if (length && zarchive_read(target.archive,target.node,0,buffer,length)!=length) return -1;
        return (int64_t)length;
    }
    FILE *f=fopen(target.host,"rb");
    if (!f) return -1;
    size_t n=fread(buffer,1,(size_t)size,f);
    int extra=fgetc(f)!=EOF;
    fclose(f);
    return extra ? -1 : (int64_t)n;
}
void runtime_file_report(void) {
    printf("Runtime: files opened=%zu, reads=%zu (%llu bytes), writes=%zu, not found=%zu\n",
           opens,reads,(unsigned long long)bytes_read,writes,missing);
}
