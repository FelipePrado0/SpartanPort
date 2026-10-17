#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/runtime_mouse.h"

#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#endif

int main(void) {
    RuntimeMouseData data;

    setenv("GOW3_MOUSE", "1", 1);
    setenv("GOW3_STICK_MOUSE", "1", 1);
    setenv("GOW3_STICK_MOUSE_SENSITIVITY", "8", 1);
    runtime_mouse_configure();
    assert(runtime_mouse_enabled());

    runtime_mouse_event_motion(12, -4);
    runtime_mouse_event_button(1, 1);
    runtime_mouse_event_wheel(2);
    assert(runtime_mouse_read(&data) == 1);
    assert(data.connected && data.buttons == 1);
    assert(data.x_axis == 12 && data.y_axis == -4 && data.wheel == 2);

    runtime_mouse_add_stick(192, 64, UINT64_C(1234));
    int32_t pad_x, pad_y;
    runtime_mouse_take_pad_motion(&pad_x, &pad_y);
    assert(pad_x == 12 && pad_y == -4);
    assert(runtime_mouse_read(&data) == 1);
    assert(data.x_axis == 16 && data.y_axis == -16 && data.timestamp == 1234);

    runtime_mouse_event_button(1, 0);
    runtime_mouse_focus_lost();
    assert(runtime_mouse_read(&data) == 1);
    assert(data.buttons == 0 && data.x_axis == 0 && data.y_axis == 0 && data.wheel == 0);
    puts("PASS: mouse input");
    return 0;
}