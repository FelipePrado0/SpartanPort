/* Guest file operations on a .zar game image: /app0 reads, listings, stats and the read-only
 * rules of the folder it replaces, plus a host mount over one archive file (the glyphs' and pause
 * menu's copy of a WAD). Uses tests/data/zarchive.zar, packed by tools/make_zar.py; no game dump.
 *
 *   out/archive-fs-test        (from the repository root; skipped without libzstd) */
#include "../src/runtime_file.c"
#include <assert.h>
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
int32_t runtime_guest_errno(int e) { return e; }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *name) {
    for (size_t i=0;i<n;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
static char *read_whole(int fd,char *buffer,size_t size) {
    int64_t n=do_read(fd,buffer,size-1);
    assert(n>=0);
    buffer[n]=0;
    return buffer;
}
int main(void) {
    if (!zarchive_library_available()) {
        printf("archive-fs-test: skipped, libzstd is not installed\n");
        return 0;
    }
    char root[]="/tmp/spartanport-archive-XXXXXX";
    assert(mkdtemp(root));
    char user[512];
    snprintf(user,sizeof(user),"%s/user",root);
    runtime_file_configure("tests/data/zarchive.zar",user);

    /* Files of the archive, case-insensitively, with positions and sizes. */
    int fd=(int)do_open("/app0/hello.txt",0,0); assert(fd>=3);
    char text[64];
    assert(!strcmp(read_whole(fd,text,sizeof(text)),"SpartanPort .zar\n"));
    GuestStat info; assert(!do_fstat(fd,&info) && info.size==17 && (info.mode&0170000)==0100000);
    assert(do_lseek(fd,0,2)==17);
    assert(do_read(fd,text,4)==0);
    assert(do_pread(fd,text,4,0)==4 && !memcmp(text,"Spar",4));
    assert(!do_close(fd));
    assert(!strcmp(read_whole((int)do_open("/app0/HELLO.TXT",0,0),text,sizeof(text)),"SpartanPort .zar\n"));

    /* A read across a 64 KiB block boundary, and the file's size from the tree. */
    fd=(int)do_open("/app0/EXEC/WAD/PS4_LE/R_UI.WAD",0,0); assert(fd>=3);
    GuestStat wad; assert(!do_stat("/app0/EXEC/WAD/PS4_LE/R_UI.WAD",&wad) && wad.size==140000);
    unsigned char crossing[3];
    assert(do_pread(fd,crossing,3,65535)==3);
    assert(do_lseek(fd,65535,0)==65535);
    assert(do_read(fd,crossing,3)==3);
    assert(!do_close(fd));

    /* Directories list from the archive: the root, and a nested one. */
    int dir=(int)do_open("/app0",0x20000,0); assert(dir>=3);
    char entries[4096];
    int64_t count=do_getdents(dir,entries,sizeof(entries),NULL); assert(count>0);
    int found=0;
    for (int64_t p=0;p<count;) {
        uint16_t length; memcpy(&length,entries+p+4,2);
        assert(length);
        if (!strcmp(entries+p+8,"hello.txt")) { assert(entries[p+6]==8); found=1; }
        p+=length;
    }
    assert(found && !do_close(dir));
    int sub=(int)do_open("/app0/EXEC/WAD/PS4_LE",0,0); assert(sub>=3);
    count=do_getdents(sub,entries,sizeof(entries),NULL); assert(count>0);
    assert(!strcmp(entries+8,"R_UI.WAD") && entries[6]==8);
    assert(!do_close(sub));
    GuestStat directory; assert(!do_stat("/app0/sce_sys",&directory) && (directory.mode&0170000)==0040000);

    /* Read-only, like the game folder: opens for writing and every path operation fail. */
    assert(do_open("/app0/hello.txt",2,0)==-EROFS);
    assert(do_open("/app0/new.bin",0x200|1,0)==-EROFS);
    assert(do_truncate("/app0/hello.txt",0)==-EROFS);
    assert(path_op("/app0/hello.txt",2,0)==-EROFS);
    assert(do_rename("/app0/hello.txt","/data/moved")==-EROFS);
    assert(do_access("/app0/hello.txt",4)==0);
    assert(do_access("/app0/missing.bin",4)==-ENOENT);
    assert(do_open("/app0/missing.bin",0,0)==-ENOENT);

    /* param.sfo for the loader, through the same guest path. */
    unsigned char sfo[256];
    assert(runtime_file_read_all("/app0/sce_sys/param.sfo",sfo,sizeof(sfo))==64);
    assert(!memcmp(sfo,"\0PSF",4));

    /* A host file mounted over one archive path wins for it; its siblings stay in the archive. */
    char replacement[512], guest[512];
    snprintf(replacement,sizeof(replacement),"%s/param.sfo",root);
    FILE *f=fopen(replacement,"wb"); assert(f); assert(fwrite("\0PSFcopy",1,8,f)==8); assert(!fclose(f));
    assert(!runtime_file_mount("/app0/sce_sys/param.sfo",replacement));
    fd=(int)do_open("/app0/sce_sys/param.sfo",0,0); assert(fd>=3);
    assert(!strcmp(read_whole(fd,text,sizeof(text)),"\0PSFcopy")); assert(!do_close(fd));
    assert(!strcmp(read_whole((int)do_open("/app0/hello.txt",0,0),text,sizeof(text)),"SpartanPort .zar\n"));
    assert(runtime_file_read_all("/app0/sce_sys/param.sfo",sfo,sizeof(sfo))==8);
    runtime_file_unmount("/app0/sce_sys/param.sfo");

    /* Unpacked folders keep working on the writable mounts. */
    snprintf(guest,sizeof(guest),"/data/note.txt");
    fd=(int)do_open(guest,0x200|1,0644); assert(fd>=3);
    assert(do_write(fd,"save",4)==4); assert(!do_close(fd));
    assert(!strcmp(read_whole((int)do_open(guest,0,0),text,sizeof(text)),"save"));

    printf("archive-fs-test: ok\n");
    return 0;
}
