#ifndef GOW3_RUNTIME_MOUSE_H
#define GOW3_RUNTIME_MOUSE_H

#include <stdint.h>

/* Guest-visible libSceMouse data. Keep this layout in sync with the PS4 ABI. */
typedef struct {
    uint64_t timestamp;
    uint8_t connected;
    uint8_t padding[3];
    uint32_t buttons;
    int32_t x_axis;
    int32_t y_axis;
    int32_t wheel;
    int32_t tilt;
    uint8_t reserve[8];
} RuntimeMouseData;
_Static_assert(sizeof(RuntimeMouseData) == 40, "OrbisMouseData layout");

void runtime_mouse_configure(void);
int runtime_mouse_enabled(void);
void runtime_mouse_event_motion(int32_t x, int32_t y);
void runtime_mouse_event_button(uint8_t button, int down);
void runtime_mouse_event_wheel(int32_t amount);
void runtime_mouse_focus_lost(void);
int runtime_mouse_read(RuntimeMouseData *data);
void runtime_mouse_take_pad_motion(int32_t *x, int32_t *y);
uint32_t runtime_mouse_pad_buttons(void);
void runtime_mouse_add_stick(uint8_t x, uint8_t y, uint64_t timestamp);

#endif