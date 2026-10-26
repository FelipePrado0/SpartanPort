#include "../../src/runtime_services.c"

#ifdef _WIN32
/* The PE linker keeps unrelated service exports; none may run in this contract. */
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { abort(); }
int64_t runtime_file_open(const char *path, int flags, int mode) { abort(); }
int64_t runtime_file_read(int fd, void *buffer, uint64_t size) { abort(); }
int64_t runtime_file_close(int fd) { abort(); }
const char *runtime_file_user_dir(void) { abort(); }
int64_t runtime_savedata_mtime(int32_t user, const char *dir) { abort(); }
int runtime_savedata_param(int32_t user, const char *dir, void *param) { abort(); }
int runtime_savedata_icon(int32_t user, const char *dir, char *out, size_t size) { abort(); }
int spartanportgpu_text_input_begin(const char *text, const char *prompt) { abort(); }
int spartanportgpu_text_input_poll(char *text, uint64_t size) { abort(); }
void spartanportgpu_trophy_setup(const SpartanGpuTrophyLabels *labels, const SpartanGpuTrophyInfo *items,
                                int count, const volatile uint32_t *unlocked) { abort(); }
void spartanportgpu_toast_trophy(const char *title, const char *detail, const unsigned char *png,
                                uint64_t size) { abort(); }
int spartanportgpu_choice_begin(const char *title, const char *const *items, const char *const *images,
                                int count, int focus) { abort(); }
int spartanportgpu_choice_poll(void) { abort(); }
int spartanportgpu_choice_confirm(const char *title, const char *const *items, int count, int focus,
                                  const unsigned char *png, uint64_t size, const char *caption,
                                  const char *unavailable) { abort(); }
#endif

int spartan_validation_guest_enter(void) {
    int32_t value = -1;
    return system_param(1000, &value) == 0 ? value : -1;
}
