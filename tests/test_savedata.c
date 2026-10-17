/* Save data: the newest save (for the quit button and the load list) and whether a save is being written. */
#undef NDEBUG /* the checks are asserts; release builds define NDEBUG */
#include "../src/runtime_savedata.c"
#include <assert.h>
#include <utime.h>

static char user_dir[256];
const char *runtime_file_user_dir(void) { return user_dir; }
int runtime_file_mount(const char *guest, const char *host) { (void)guest; (void)host; return 0; }
void runtime_file_unmount(const char *point) { (void)point; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }

static void touch(const char *path, time_t when) {
    FILE *f=fopen(path,"wb"); assert(f); fputs("x",f); fclose(f);
    struct utimbuf times={when,when}; assert(!utime(path,&times));
}

int main(void) {
    snprintf(user_dir,sizeof(user_dir),"/tmp/gow3-savedata-test-%d",(int)getpid());
    runtime_savedata_configure("CUSA01623");
    char dir[32]; int64_t when=0;
    assert(runtime_savedata_latest(1,dir,sizeof(dir),&when)==-1 && when==0); /* no saves yet */

    assert(save_initialize(NULL)==0);
    DirName autosave={"AUTOSAVE"}, manual={"SAVE0001"};
    MountResult result;
    assert(mount(1,NULL,&autosave,MODE_CREATE2,&result)==0);
    assert(runtime_savedata_writing()); /* mounted for writing */
    assert(save_umount(&result.point)==0 && !runtime_savedata_writing());
    assert(mount(1,NULL,&manual,MODE_CREATE2,&result)==0 && save_umount(&result.point)==0);
    assert(mount(1,NULL,&manual,MODE_RDONLY,&result)==0);
    assert(!runtime_savedata_writing()); /* loading is not writing */
    assert(save_umount(&result.point)==0);

    /* The newest save is the one whose files changed last, whatever its param says. */
    char base[600], path[800];
    root(1,NULL,base,sizeof(base));
    snprintf(path,sizeof(path),"%s/AUTOSAVE.sce_sys/param.bin",base); touch(path,1000);
    snprintf(path,sizeof(path),"%s/SAVE0001.sce_sys/param.bin",base); touch(path,1500);
    snprintf(path,sizeof(path),"%s/AUTOSAVE",base); struct utimbuf old={1000,1000}; assert(!utime(path,&old));
    snprintf(path,sizeof(path),"%s/SAVE0001",base); assert(!utime(path,&old));
    snprintf(path,sizeof(path),"%s/AUTOSAVE/data.bin",base); touch(path,2000);
    snprintf(path,sizeof(path),"%s/AUTOSAVE",base); assert(!utime(path,&old));
    assert(runtime_savedata_mtime(1,"AUTOSAVE")==2000 && runtime_savedata_mtime(1,"SAVE0001")==1500);
    assert(runtime_savedata_mtime(1,"MISSING")==0 && runtime_savedata_mtime(1,"../x")==0);
    assert(runtime_savedata_latest(1,dir,sizeof(dir),&when)==0 && !strcmp(dir,"AUTOSAVE") && when==2000);

    snprintf(path,sizeof(path),"%s/SAVE0001/data.bin",base); touch(path,3000);
    snprintf(path,sizeof(path),"%s/SAVE0001",base); assert(!utime(path,&old));
    assert(runtime_savedata_latest(1,dir,sizeof(dir),&when)==0 && !strcmp(dir,"SAVE0001") && when==3000);

    remove_tree(user_dir);
    puts("savedata test: ok");
    return 0;
}
