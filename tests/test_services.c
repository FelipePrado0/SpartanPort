/* System services contracts: dialogs (progress bars run until closed), PlayGo, trophies. */
#undef NDEBUG /* the checks are asserts; release builds define NDEBUG */
#include "../src/runtime_services.c"
#include <assert.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }
int spartanportgpu_text_input_begin(const char *text, const char *prompt) { (void)text; (void)prompt; return 0; }
int spartanportgpu_text_input_poll(char *text, uint64_t size) { (void)text; (void)size; return 0; }
void spartanportgpu_trophy_setup(const SpartanGpuTrophyLabels *labels, const SpartanGpuTrophyInfo *items,
                                int count, const volatile uint32_t *unlocked) {
    (void)labels; (void)items; (void)count; (void)unlocked;
}
static int preview_file_enabled, preview_file_reads, preview_file_closes;
static int64_t preview_file_result=8;
static const unsigned char preview_file_png[8]={137,80,78,71,13,10,26,10};
int64_t runtime_file_open(const char *path, int flags, int mode) {
    (void)mode; assert(flags==0);
    return preview_file_enabled && !strcmp(path,"/app0/EXEC/ICONS/SAVEDATA/SAVEPT18.PNG") ? 42 : -1;
}
int64_t runtime_file_read(int fd, void *buffer, uint64_t size) {
    assert(fd==42 && size>=sizeof(preview_file_png)); ++preview_file_reads;
    memcpy(buffer,preview_file_png,sizeof(preview_file_png)); return preview_file_result;
}
int64_t runtime_file_close(int fd) { assert(fd==42); ++preview_file_closes; return 0; }
static int choice_next=-1;
static int choice_count, choice_focus, choice_available=1;
static int preview_calls;
static uint64_t preview_size;
static unsigned char preview_bytes[8];
static char shown_title[3000], shown_items[33][3000];
static unsigned char save_param[RUNTIME_SAVE_PARAM_SIZE];
static int save_param_available;
int spartanportgpu_choice_begin(const char *title, const char *const *items, const char *const *images, int count, int focus) {
    (void)images;
    choice_count=count; choice_focus=focus; choice_next=-1;
    snprintf(shown_title,sizeof(shown_title),"%s",title);
    for (int i=0;i<count && i<33;++i) snprintf(shown_items[i],sizeof(shown_items[i]),"%s",items[i]);
    return choice_available && count>0;
}
int spartanportgpu_choice_poll(void) { return choice_next; }
int spartanportgpu_choice_confirm(const char *title, const char *const *items, int count, int focus,
                                 const unsigned char *png, uint64_t size, const char *caption, const char *unavailable) {
    assert(caption && *caption && unavailable && *unavailable);
    ++preview_calls; preview_size=size;
    memset(preview_bytes,0,sizeof(preview_bytes));
    if (png && size) memcpy(preview_bytes,png,size<sizeof(preview_bytes) ? (size_t)size : sizeof(preview_bytes));
    return spartanportgpu_choice_begin(title,items,NULL,count,focus);
}
int runtime_savedata_param(int32_t user, const char *dir, void *param) {
    (void)user; (void)dir;
    if (!save_param_available) return -1;
    memcpy(param,save_param,sizeof(save_param)); return 0;
}
int runtime_savedata_icon(int32_t user, const char *dir, char *out, size_t size) { (void)user; (void)dir; (void)out; (void)size; return -1; }

int main(void) {
    unsigned char param[128]={0}, result[64]={0}; /* the game zeroes the pointers it does not want */
    uint32_t value;

    /* MsgDialog: a user message finishes with OK; a progress bar runs until closed. */
    assert(msg_init()==0);
    memset(param+56,0,4); param[56]=1;
    assert(msg_open(param)==0 && msg_status()==DIALOG_FINISHED);
    assert(msg_result(result)==0);
    memcpy(&value,result+4,4); assert(value==1);
    param[56]=2;
    assert(msg_open(param)==0 && msg_status()==DIALOG_RUNNING);
    assert(msg_result(result)==DIALOG_NOT_FINISHED);
    assert(msg_progress(0,50)==0 && msg_progress(1,50)==DIALOG_PARAM_INVALID);
    assert(msg_close()==0 && msg_status()==DIALOG_FINISHED && msg_close()==DIALOG_NOT_RUNNING);
    assert(msg_progress(0,100)==DIALOG_NOT_RUNNING);
    assert(msg_result(NULL)==DIALOG_ARG_NULL);
    assert(msg_term()==0 && msg_open(NULL)==DIALOG_ARG_NULL);

    /* SaveDataDialog: mode at offset 52 (5 = progress bar); result after the mode field. */
    assert(save_init()==0 && save_ready()==1);
    param[52]=5;
    assert(save_open(param)==0 && save_status()==DIALOG_RUNNING && save_progress(0,10)==0);
    assert(save_close()==0 && save_result(result)==0);
    memcpy(&value,result+8,4); assert(value==0); /* closed by the game: no button */
    param[52]=2;
    assert(save_open(param)==0 && save_status()==DIALOG_FINISHED && save_result(result)==0);
    memcpy(&value,result+8,4); assert(value==1);
    assert(save_term()==0);

    /* Save list: the player's choice comes back as the save directory. */
    char names[2][32]={"SAVE0001","SAVE0002"}, chosen[32];
    unsigned char items[64]={0}, list[128]={0}, out[64]={0};
    const char *names_ptr=names[0]; uint32_t name_count=2;
    struct { const char *title; const unsigned char *icon; uint64_t size; } new_item={"New",NULL,0};
    const void *new_title=&new_item;
    memcpy(items+16,&names_ptr,8); memcpy(items+24,&name_count,4); memcpy(items+32,&new_title,8);
    unsigned char *items_ptr=items; char *chosen_ptr=chosen;
    list[52]=1; list[56]=2; memcpy(list+72,&items_ptr,8);
    memcpy(out+16,&chosen_ptr,8);
    assert(save_init()==0 && save_open(list)==0 && save_status()==DIALOG_RUNNING);
    choice_next=2; /* entries: New save, SAVE0001, SAVE0002 */
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0 && !strcmp(chosen,"SAVE0002"));
    memcpy(&value,out+4,4); assert(value==0);
    memcpy(&value,out+8,4); assert(value==1);
    assert(save_open(list)==0);
    choice_next=0;
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0 && chosen[0]==0); /* new save */
    assert(save_open(list)==0);
    choice_next=-2;
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0);
    memcpy(&value,out+4,4); assert(value==SAVE_RESULT_CANCELED);

    /* Overwriting must not return a slot until the second, explicit confirmation. */
    list[56]=1;
    assert(save_open(list)==0);
    choice_next=2;
    assert(save_status()==DIALOG_RUNNING);
    assert(save_result(out)==DIALOG_NOT_FINISHED);
    assert(choice_count==2 && choice_focus==1 && !strcmp(shown_items[1],"Cancel"));
    assert(preview_calls==1 && preview_size==0);
    assert(!strstr(shown_title,"SAVE0002"));
    choice_next=0;
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0 && !strcmp(chosen,"SAVE0002"));

    /* Both the Cancel option and Back return to the same slot without finishing. */
    for (int cancel=1;cancel>=-2;cancel-=3) {
        assert(save_open(list)==0);
        choice_next=2; assert(save_status()==DIALOG_RUNNING);
        choice_next=cancel; assert(save_status()==DIALOG_RUNNING);
        assert(choice_count==3 && choice_focus==2);
        choice_next=-2;
        assert(save_status()==DIALOG_FINISHED && save_result(out)==0);
        memcpy(&value,out+4,4); assert(value==SAVE_RESULT_CANCELED);
    }

    /* A new slot and a non-list progress dialog keep their existing behavior. */
    assert(save_open(list)==0);
    choice_next=0;
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0 && !chosen[0]);
    list[52]=5;
    assert(save_open(list)==0 && save_status()==DIALOG_RUNNING);
    assert(save_close()==0 && save_status()==DIALOG_FINISHED);
    list[52]=1;

    /* No New save row: entry zero is an existing slot and still needs confirmation. */
    memset(items+32,0,8);
    assert(save_open(list)==0);
    choice_next=0; assert(save_status()==DIALOG_RUNNING);
    choice_next=0;
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0 && !strcmp(chosen,"SAVE0001"));
    memcpy(items+32,&new_title,8);

    /* A failed confirmation window must never grant permission to overwrite. */
    assert(save_open(list)==0);
    choice_available=0; choice_next=1;
    assert(save_status()==DIALOG_FINISHED && save_result(out)==0);
    memcpy(&value,out+4,4); assert(value==SAVE_RESULT_CANCELED);
    choice_available=1;

    /* The new item's image is copied at open, never taken from the destination slot. */
    const unsigned char signature[8]={137,80,78,71,13,10,26,10};
    unsigned char supplied[8]; memcpy(supplied,signature,sizeof(supplied));
    new_item.icon=supplied; new_item.size=sizeof(supplied);
    assert(save_open(list)==0);
    memset(supplied,0,sizeof(supplied));
    choice_next=2; assert(save_status()==DIALOG_RUNNING);
    assert(preview_size==sizeof(signature) && !memcmp(preview_bytes,signature,sizeof(signature)));
    choice_next=1; assert(save_status()==DIALOG_RUNNING);
    choice_next=2; assert(save_status()==DIALOG_RUNNING);
    assert(preview_size==sizeof(signature) && !memcmp(preview_bytes,signature,sizeof(signature)));
    choice_next=1; assert(save_status()==DIALOG_RUNNING);
    choice_next=-2; assert(save_status()==DIALOG_FINISHED);
    new_item.size=UINT64_MAX;
    assert(save_open(list)==0);
    choice_next=2; assert(save_status()==DIALOG_RUNNING && preview_size==0);
    choice_next=1; assert(save_status()==DIALOG_RUNNING);
    choice_next=-2; assert(save_status()==DIALOG_FINISHED);
    new_item.icon=NULL; new_item.size=0;

    /* Full lists have no newItem: use the same pending SAVEPT PNG as the game. */
    memset(items+32,0,8);
    char pending_icon[128]="/app0/EXEC/ICONS/SAVEDATA/SAVEPT18.PNG";
    runtime_savedata_preview_path(pending_icon); preview_file_enabled=1;
    assert(save_open(list)==0);
    choice_next=1; assert(save_status()==DIALOG_RUNNING);
    assert(preview_size==8 && !memcmp(preview_bytes,signature,sizeof(signature)));
    assert(preview_file_reads==1 && preview_file_closes==1);
    choice_next=1; assert(save_status()==DIALOG_RUNNING);
    choice_next=-2; assert(save_status()==DIALOG_FINISHED);
    for (int64_t result=-1;result<=4194305;result+=4194306) {
        preview_file_result=result;
        assert(save_open(list)==0);
        choice_next=1; assert(save_status()==DIALOG_RUNNING && preview_size==0);
        choice_next=1; assert(save_status()==DIALOG_RUNNING);
        choice_next=-2; assert(save_status()==DIALOG_FINISHED);
    }
    int reads_before=preview_file_reads;
    strcpy(pending_icon,"/app0/EXEC/ICONS/SAVEDATA/../SAVEPT18.PNG");
    assert(save_open(list)==0 && preview_file_reads==reads_before);
    memset(pending_icon,'X',sizeof(pending_icon));
    assert(save_open(list)==0 && preview_file_reads==reads_before);
    runtime_savedata_preview_path(NULL); preview_file_enabled=0;
    memcpy(items+32,&new_title,8);

    /* The saved difficulty and elapsed time appear in list rows and confirmation. */
    save_param_available=1;
    strcpy((char *)save_param,"SpartanPort");
    strcpy((char *)save_param+128,"Reino de Hades");
    strcpy((char *)save_param+256,"Dificuldade: Espartano (F\xe1" "cil)\nTempo de jogo: 25:47:01");
    int64_t timestamp=1720000000; memcpy(save_param+1288,&timestamp,8);
    assert(save_open(list)==0);
    assert(strstr(shown_items[1],"Reino de Hades"));
    assert(strstr(shown_items[1],"F\xc3\xa1" "cil") && strstr(shown_items[1],"25:47:01"));
    choice_next=1; assert(save_status()==DIALOG_RUNNING);
    assert(strstr(shown_title,"25:47:01") && strstr(shown_title,"Reino de Hades"));
    assert(!strstr(shown_title,"SpartanPort") && !strstr(shown_title,"SAVE0001"));
    choice_next=0; assert(save_status()==DIALOG_FINISHED);

    /* Valid UTF-8 stays intact; absent and unterminated metadata are bounded. */
    strcpy((char *)save_param+256,"Difficulty: Ol\xc3\xadmpico\nPlay time: 0:02:03");
    assert(save_open(list)==0 && strstr(shown_items[1],"Ol\xc3\xadmpico"));
    memset(save_param+256,0,1024);
    assert(save_open(list)==0 && strstr(shown_items[1],"unavailable"));
    memset(save_param,'X',sizeof(save_param));
    assert(save_open(list)==0 && strlen(shown_items[1])<sizeof(shown_items[1])-1);
    save_param_available=0;
    assert(save_open(list)==0 && strstr(shown_items[1],"SAVE0001") && strstr(shown_items[1],"unavailable"));
    assert(save_term()==0);

    /* PlayGo: installed, full speed, progress never divides by zero. */
    int32_t handle=0; uint64_t progress[2]; int32_t speed; uint16_t chunk=0;
    assert(playgo_init(NULL)==0 && playgo_open(&handle,NULL)==0);
    assert(playgo_get_speed(handle,&speed)==0 && speed==2);
    assert(playgo_progress(handle,&chunk,1,progress)==0 && progress[0]==progress[1] && progress[1]);
    assert(playgo_progress(handle,&chunk,0,progress)==PLAYGO_BAD_SIZE);
    assert(playgo_todo(handle,&chunk,1)==0 && playgo_todo(handle,NULL,1)==PLAYGO_BAD_POINTER);
    assert(playgo_close(handle)==0 && playgo_close(handle)==PLAYGO_BAD_HANDLE && playgo_terminate()==0);

    /* Trophies: nothing unlocked. */
    uint32_t flags[4]={~0u,~0u,~0u,~0u}, count=7;
    assert(trophy_unlock_state(1,1,flags,&count)==0 && !flags[0] && !flags[3] && count==0);
    assert(trophy_unlock_state(1,1,NULL,&count)==TROPHY_INVALID && trophy_release(0)==TROPHY_INVALID);

    float ratio=0; assert(safe_area(&ratio)==0 && ratio==1.0f && safe_area(NULL)==SYSTEM_PARAMETER);

    /* System parameter 1: language ID from SPARTAN_LANGUAGE (default 1 = English US, 16 = Polish) */
    int32_t lang=0;
    assert(system_param(1,&lang)==0 && lang==1);
#ifdef _WIN32
    _putenv("SPARTAN_LANGUAGE=16");
#else
    setenv("SPARTAN_LANGUAGE","16",1);
#endif
    assert(system_param(1,&lang)==0 && lang==16);
#ifdef _WIN32
    _putenv("SPARTAN_LANGUAGE=");
#else
    unsetenv("SPARTAN_LANGUAGE");
#endif
    assert(system_param(1,&lang)==0 && lang==1);

    puts("services test: ok");
    return 0;
}
