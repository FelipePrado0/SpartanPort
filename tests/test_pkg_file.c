/* Integration contract test: run with a live GOW3_PKG_PIPE for the user's PKG. */
#include "../src/runtime_file.c"
#include <assert.h>
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
int32_t runtime_guest_errno(int e) { return e; }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *name) {
    for (size_t i=0;i<n;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
int main(void) {
    assert(getenv("GOW3_PKG_PIPE"));
    runtime_file_configure("unused.pkg","out/pkg-test-user");
    int fd=(int)do_open("/app0/eboot.bin",0,0); assert(fd>=3);
    GuestStat stat; assert(!do_fstat(fd,&stat) && stat.size>1024);
    unsigned char first[32],second[32];
    assert(do_read(fd,first,32)==32);
    assert(!memcmp(first,"\x4f\x15\x3d\x1d",4));
    assert(do_pread(fd,second,32,0)==32 && !memcmp(first,second,32));
    assert(do_lseek(fd,0,1)==32); /* pread did not alter the position */
    assert(do_lseek(fd,-8,2)==stat.size-8);
    assert(do_read(fd,second,32)==8 && do_read(fd,second,32)==0);
    assert(do_lseek(fd,-1,0)==-EINVAL);
    assert(do_pread(fd,second,1,-1)==-EINVAL);
    assert(do_write(fd,second,1)<0);
    assert(!do_close(fd));
    fd=(int)do_open("/app0/EXEC/CINEMATICS/BALCONYBREAK.BSF",0,0); assert(fd>=3);
    size_t large_size=9*1024*1024;
    unsigned char *large=malloc(large_size); assert(large);
    assert(do_pread(fd,large,large_size,0)==(int64_t)large_size);
    assert(do_pread(fd,second,32,8*1024*1024)==32);
    assert(!memcmp(large+8*1024*1024,second,32)); free(large); assert(!do_close(fd));
    assert(do_open("/app0/eboot.bin",2,0)==-EROFS);
    assert(do_open("/app0/../eboot.bin",0,0)==-EACCES);
    assert(do_open("/app0/does-not-exist",0,0)==-ENOENT);
    assert(!do_stat("/hostapp/sce_sys/param.sfo",&stat) && stat.size>20);
    fd=(int)do_open("/app0",0x20000,0); assert(fd>=3);
    char entries[4096]; assert(do_getdents(fd,entries,sizeof(entries),NULL)>0);
    assert(!do_fstat(fd,&stat) && (stat.mode&0040000)); assert(!do_close(fd));
    fd=(int)do_open("/data/pkg-save-test",0x202,0644); assert(fd>=3);
    assert(do_write(fd,"save",4)==4 && !do_close(fd));
    assert(!path_op("/data/pkg-save-test",2,0));
    puts("PASS: direct PKG read/pread/seek/EOF/stat/listing, readonly assets, writable saves");
    return 0;
}
