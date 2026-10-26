/* Save data: when a save was last written (the load list's focus and the menu's last autosave). */
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

#ifdef _WIN32
#include <windows.h>
static int touch_dir(const char *path, time_t when) {
    HANDLE h=CreateFileA(path,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if (h==INVALID_HANDLE_VALUE) return -1;
    uint64_t ft=((uint64_t)when+11644473600ULL)*10000000ULL;
    FILETIME filetime={(DWORD)ft,(DWORD)(ft>>32)};
    BOOL ok=SetFileTime(h,NULL,NULL,&filetime);
    CloseHandle(h);
    return ok ? 0 : -1;
}
#else
static int touch_dir(const char *path, time_t when) {
    struct utimbuf times={when,when}; return utime(path,&times);
}
#endif

int main(void) {
    const char *tmp=getenv("TMP");
    if (!tmp) tmp=getenv("TEMP");
    if (!tmp) tmp="/tmp";
    snprintf(user_dir,sizeof(user_dir),"%s/spartanport-savedata-test-%d",tmp,(int)getpid());
    runtime_savedata_configure("CUSA01623");
    assert(runtime_savedata_mtime(1,"AUTOSAVE")==0); /* no saves yet */

    assert(save_initialize(NULL)==0);
    DirName autosave={"AUTOSAVE"}, manual={"SAVE0001"};
    MountResult result;
    assert(mount(1,NULL,&autosave,MODE_CREATE2,&result)==0 && save_umount(&result.point)==0);
    assert(mount(1,NULL,&manual,MODE_CREATE2,&result)==0 && save_umount(&result.point)==0);

    /* A save's time is the newest of its files and its param.bin, whatever the folder's own time. */
    char base[600], path[800];
    root(1,NULL,base,sizeof(base));
    snprintf(path,sizeof(path),"%s/AUTOSAVE.sce_sys/param.bin",base); touch(path,1000);
    snprintf(path,sizeof(path),"%s/SAVE0001.sce_sys/param.bin",base); touch(path,1500);
    snprintf(path,sizeof(path),"%s/SAVE0001",base); assert(!touch_dir(path,1000));
    snprintf(path,sizeof(path),"%s/AUTOSAVE/data.bin",base); touch(path,2000);
    snprintf(path,sizeof(path),"%s/AUTOSAVE",base); assert(!touch_dir(path,1000));
    assert(runtime_savedata_mtime(1,"AUTOSAVE")==2000 && runtime_savedata_mtime(1,"SAVE0001")==1500);
    assert(runtime_savedata_mtime(1,"MISSING")==0 && runtime_savedata_mtime(1,"../x")==0);

    /* A directory touched with a newer timestamp must NOT override the files inside. */
    snprintf(path,sizeof(path),"%s/SAVE0001",base); assert(!touch_dir(path,5000));
    assert(runtime_savedata_mtime(1,"SAVE0001")==1500);

    /* An empty save directory without any files or param.bin returns 0 even if directory mtime > 0. */
    snprintf(path,sizeof(path),"%s/SAVEEMPTY",base);
    assert(!make_dirs(path));
    assert(!touch_dir(path,3000));
    assert(runtime_savedata_mtime(1,"SAVEEMPTY")==0);

#ifndef _WIN32
    /* Symlinks pointing outside the save directory are not followed. */
    snprintf(path,sizeof(path),"%s/external.bin",user_dir); touch(path,99999);
    char link_path[800]; snprintf(link_path,sizeof(link_path),"%s/SAVE0001/symlink.bin",base);
    assert(!symlink(path,link_path));
    assert(runtime_savedata_mtime(1,"SAVE0001")==1500);
    assert(!unlink(link_path));
#endif

    remove_tree(user_dir);
    puts("savedata test: ok");
    return 0;
}
