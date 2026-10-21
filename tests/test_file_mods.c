/* Exercise actual guest file operations on a linked overlay and writable saves. */
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
    char root[]="/tmp/spartanport-mod-files-XXXXXX";
    assert(mkdtemp(root));
    char game[512],user[512],source[512],link[512];
    snprintf(game,sizeof(game),"%s/game",root);
    snprintf(user,sizeof(user),"%s/user",root);
    snprintf(source,sizeof(source),"%s/mod.dcx",root);
    snprintf(link,sizeof(link),"%s/game/asset.dcx",root);
    assert(!mkdir(game,0755));
    FILE *f=fopen(source,"w"); assert(f); assert(fputs("modded",f)>=0); assert(!fclose(f));
    assert(!symlink(source,link));
    runtime_file_configure(game,user);
    for (int i=0;i<3;++i) {
        const char *path=i==0 ? "/app0/asset.dcx" : i==1 ? "/hostapp/asset.dcx" : "asset.dcx";
        int fd=(int)do_open(path,0,0); assert(fd>=3);
        char content[8]={0}; assert(do_read(fd,content,6)==6 && !strcmp(content,"modded"));
        GuestStat info; assert(!do_stat(path,&info) && info.size==6);
        assert(!do_close(fd));
        assert(do_open(path,2,0)==-EROFS);
        assert(do_open(path,0x400,0)==-EROFS);
        assert(do_truncate(path,0)==-EROFS);
        assert(path_op(path,2,0)==-EROFS);
        assert(do_rename(path,"/data/moved")==-EROFS);
    }
    /* An exact-file mount replaces one asset; its siblings under /app0 still come from the game. */
    char replacement[512], other[512];
    snprintf(replacement,sizeof(replacement),"%s/replacement.dcx",root);
    snprintf(other,sizeof(other),"%s/game/other.dcx",root);
    f=fopen(replacement,"w"); assert(f); assert(fputs("exact",f)>=0); assert(!fclose(f));
    f=fopen(other,"w"); assert(f); assert(fputs("plain",f)>=0); assert(!fclose(f));
    assert(!runtime_file_mount("/app0/asset.dcx",replacement));
    int exact=(int)do_open("/app0/asset.dcx",0,0); assert(exact>=3);
    char exact_content[8]={0}; assert(do_read(exact,exact_content,5)==5 && !strcmp(exact_content,"exact"));
    GuestStat exact_info; assert(!do_stat("/app0/asset.dcx",&exact_info) && exact_info.size==5);
    assert(!do_close(exact));
    assert(do_open("/app0/asset.dcx",2,0)==-EROFS);
    int sibling=(int)do_open("/app0/other.dcx",0,0); assert(sibling>=3);
    char sibling_content[8]={0}; assert(do_read(sibling,sibling_content,5)==5 && !strcmp(sibling_content,"plain"));
    assert(!do_close(sibling));
    runtime_file_unmount("/app0/asset.dcx");
    int dir=(int)do_open("/app0",0x20000,0); assert(dir>=3);
    char entries[1024]; int64_t count=do_getdents(dir,entries,sizeof(entries),NULL); assert(count>0);
    int found=0;
    for (int64_t p=0;p<count;) {
        uint16_t length; memcpy(&length,entries+p+4,2);
        assert(length);
        if (!strcmp(entries+p+8,"asset.dcx")) { assert(entries[p+6]==8); found=1; }
        p+=length;
    }
    assert(found && !do_close(dir));
    int save=(int)do_open("/data/test-save",0x202,0644); assert(save>=3);
    assert(do_write(save,"save",4)==4 && !do_close(save));
    assert(!path_op("/data/test-save",2,0));
    assert(!unlink(link) && !unlink(other) && !unlink(source) && !unlink(replacement) && !rmdir(game));
    const char *dirs[]={"temp0","download0","data"};
    for (int i=0;i<3;++i) { char p[1024]; snprintf(p,sizeof(p),"%s/%s",user,dirs[i]); assert(!rmdir(p)); }
    assert(!rmdir(user) && !rmdir(root));
    puts("Guest mod files: reads, stat, merged listing, readonly assets and writable saves PASS");
}
