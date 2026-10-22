/* C interface of the GPU library (gpu/): shadPS4's Liverpool/Vulkan video core,
 * GnmDriver, VideoOut and kernel event queues, adapted to the native loader. */
#ifndef SPARTANGPU_H
#define SPARTANGPU_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *title;          /* window title */
    const char *serial;         /* CUSA id, names the pipeline cache */
    const char *user_dir;       /* pipeline cache/logs directory */
    uint32_t sdk_version;       /* from the eboot's procparam */
    uint32_t psf_attributes;    /* param.sfo ATTRIBUTE */
    int32_t width, height;      /* initial window size */
} SpartanGpuConfig;
/* Registers kernel event queues (needed with or without graphics). */
void spartanportgpu_register_kernel(void);
/* Creates window, Vulkan device, presenter and GPU command processor. */
int spartanportgpu_init(const SpartanGpuConfig *config);
/* Function for an imported NID ("NID#lib#mod"), or 0 when the GPU library does not provide it. */
uintptr_t spartanportgpu_resolve(const char *scoped_nid);
/* Called first by the loader's SIGSEGV handler: 1 when a GPU page-tracking fault was handled. */
int spartanportgpu_handle_fault(void *ucontext, void *address);
/* SPARTAN_WRITE_LOG=1: prints the logged GPU-side writes to guest memory near the fault. */
void spartanportgpu_dump_guest_writes(void *ucontext);
/* Keyboard text entry through the game window (IME dialog). begin returns 0 when
 * no window exists; poll returns 0 typing, 1 confirmed, 2 cancelled (UTF-8 text). */
int spartanportgpu_text_input_begin(const char *initial_utf8, const char *prompt_utf8);
int spartanportgpu_text_input_poll(char *out_utf8, uint64_t size);
/* A list for the player to choose from, drawn over the game (save list). images: a PNG path per
 * item drawn beside it (NULL array or entry: none). begin: 0 when no window exists.
 * poll: -1 while open, -2 cancelled, else the chosen index. */
int spartanportgpu_choice_begin(const char *title, const char *const *items, const char *const *images, int count, int focus);
int spartanportgpu_choice_poll(void);
/* Confirmation with a separate preview of the new save; PNG data is decoded before returning. */
int spartanportgpu_choice_confirm(const char *title, const char *const *items, int count, int focus,
                                 const unsigned char *png, uint64_t size, const char *caption, const char *unavailable);
/* 1 while the in-game settings menu is open: the game's pad input is held neutral. */
int spartanportgpu_overlay_captures_input(void);
/* Controller vibration strength set in the menu, 0 (off) to 100 (the game's own). */
int spartanportgpu_vibration_percent(void);
/* Native Display menu: query (0) or toggle (1); returns 0 off, 1 on, 2 unsupported. */
int spartanportgpu_enemy_health_bar(int toggle);
/* Native Cheats: f00+i query/f10+i toggle (8 cheats); f20+i text/f30+i decrease/f40+i increase/
 * f50+i slider position (6 multipliers); f60 resets multipliers. Text results are stable pointers. */
uint64_t spartanportgpu_native_setting(unsigned function);
/* R3 + L1 + R1 (pad) or F9: the next engine frame rate (60, 100, 120, 144, 180, 240), applied live. */
void spartanportgpu_engine_fps_cycle(void);
/* The video decoder handed the game a picture: a cutscene is on screen (the presenter draws
 * black bars outside its 16:9 picture when the render resolution is not 16:9). */
void spartanportgpu_movie_picture(void);
/* Mouse movement in px that counts as one step of a stick prompt, set in the menu. */
int spartanportgpu_mouse_prompt_distance(void);
/* The pause key (touchpad right) was pressed: it lets go of the mouse lock, or takes it back. */
void spartanportgpu_mouse_pause_key(void);
/* L2 aim with the mouse, set in the menu: 0 relative (follows movement), 1 absolute. */
int spartanportgpu_mouse_aim_style(void);
/* L2 aim sensitivity in percent (10 to 200), set in the menu. */
int spartanportgpu_mouse_aim_sensitivity(void);
/* Number of symbols registered by the vendored libraries (diagnostics). */
unsigned spartanportgpu_symbol_count(void);
/* Yields of threads waiting for guest memory copies (CPU report, #209). */
unsigned long long spartanportgpu_copy_yields(void);
/* Trophy notice at the top-right of the game window (any thread). title: the trophy's name,
 * detail: its description (null or empty: none), png: its icon (PNG bytes; null: a drawn medal). */
void spartanportgpu_toast_trophy(const char *title, const char *detail, const unsigned char *png, uint64_t png_size);
void spartanportgpu_toast_icon(const char *text, const unsigned char *png, uint64_t png_size);
void spartanportgpu_toast(const char *text);
/* The in-game trophy list (F8 / R3+R2) and the notice labels, handed over once by the runtime.
 * labels: texts of the window and of the notice (null or empty: English). items: the trophies in
 * the order of the list (their strings and png stay valid). unlocked: four 32-bit words, bit
 * id % 32 of word id / 32 set for an unlocked trophy, read live. */
typedef struct {
    const char *heading, *notice, *status_unlocked, *status_locked;
    const char *filter_all, *filter_unlocked, *filter_locked;
    const char *counter; /* "{} of {} unlocked" */
    const char *hint;    /* the keys */
} SpartanGpuTrophyLabels;
typedef struct {
    int id;
    const char *name, *detail;
    const unsigned char *png;
    uint64_t png_size;
} SpartanGpuTrophyInfo;
void spartanportgpu_trophy_setup(const SpartanGpuTrophyLabels *labels, const SpartanGpuTrophyInfo *items, int count,
                          const volatile uint32_t *unlocked);
#ifdef __cplusplus
}
#endif
#endif
