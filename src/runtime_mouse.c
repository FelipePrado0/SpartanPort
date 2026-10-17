#define _GNU_SOURCE
#include "runtime_mouse.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t mouse_lock = PTHREAD_MUTEX_INITIALIZER;
static int configured;
static int enabled;
static int stick_enabled;
static int invert_y;
static float mouse_sensitivity = 1.0f;
static float stick_sensitivity = 8.0f;
static uint32_t buttons;
static uint32_t mouse_button_map[4];
static int32_t motion_x, motion_y, wheel;
static int32_t pad_motion_x, pad_motion_y;
static uint64_t last_timestamp;

static uint64_t now_us(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * UINT64_C(1000000) + (uint64_t)time.tv_nsec / 1000;
}

static float setting(const char *name, float fallback) {
    const char *text = getenv(name);
    char *end;
    float value;
    if (!text || !*text) return fallback;
    value = strtof(text, &end);
    return end == text ? fallback : value;
}

static uint32_t pad_button(const char *name) {
    if (!strcmp(name, "cross")) return UINT32_C(0x4000);
    if (!strcmp(name, "circle")) return UINT32_C(0x2000);
    if (!strcmp(name, "triangle")) return UINT32_C(0x1000);
    if (!strcmp(name, "square")) return UINT32_C(0x8000);
    if (!strcmp(name, "l1")) return UINT32_C(0x400);
    if (!strcmp(name, "r1")) return UINT32_C(0x800);
    if (!strcmp(name, "l2")) return UINT32_C(0x100);
    if (!strcmp(name, "r2")) return UINT32_C(0x200);
    if (!strcmp(name, "options")) return UINT32_C(0x8);
    return 0;
}

static void load_mouse_button_map(void) {
    char copy[256];
    char *entry;
    const char *spec = getenv("GOW3_MOUSE_MAP");
    if (!spec || !*spec) return;
    snprintf(copy, sizeof(copy), "%s", spec);
    entry = copy;
    while (entry) {
        char *next = strchr(entry, ',');
        char *equals;
        int index = -1;
        if (next) *next++ = 0;
        equals = strchr(entry, '=');
        if (equals) {
            *equals++ = 0;
            if (!strcmp(entry, "left")) index = 1;
            else if (!strcmp(entry, "middle")) index = 2;
            else if (!strcmp(entry, "right")) index = 3;
            if (index >= 0) mouse_button_map[index] = pad_button(equals);
        }
        entry = next;
    }
}

void runtime_mouse_configure(void) {
    pthread_mutex_lock(&mouse_lock);
    if (!configured) {
        enabled = getenv("GOW3_MOUSE") && getenv("GOW3_MOUSE")[0] == '1';
        stick_enabled = getenv("GOW3_STICK_MOUSE") && getenv("GOW3_STICK_MOUSE")[0] == '1';
        invert_y = getenv("GOW3_MOUSE_INVERT_Y") && getenv("GOW3_MOUSE_INVERT_Y")[0] == '1';
        mouse_sensitivity = setting("GOW3_MOUSE_SENSITIVITY", 1.0f);
        if (mouse_sensitivity < 0.0f) mouse_sensitivity = 0.0f;
        stick_sensitivity = setting("GOW3_STICK_MOUSE_SENSITIVITY", 8.0f);
        if (stick_sensitivity < 0.0f) stick_sensitivity = 0.0f;
        load_mouse_button_map();
        configured = 1;
    }
    pthread_mutex_unlock(&mouse_lock);
}

int runtime_mouse_enabled(void) {
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    int result = enabled;
    pthread_mutex_unlock(&mouse_lock);
    return result;
}

void runtime_mouse_event_motion(int32_t x, int32_t y) {
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    if (enabled) {
        motion_x += (int32_t)(x * mouse_sensitivity);
        motion_y += (int32_t)((invert_y ? -y : y) * mouse_sensitivity);
        pad_motion_x += (int32_t)(x * mouse_sensitivity);
        pad_motion_y += (int32_t)((invert_y ? -y : y) * mouse_sensitivity);
        last_timestamp = now_us();
    }
    pthread_mutex_unlock(&mouse_lock);
}

void runtime_mouse_event_button(uint8_t button, int down) {
    runtime_mouse_configure();
    if (button < 1 || button > 3) return;
    pthread_mutex_lock(&mouse_lock);
    if (enabled) {
        uint32_t bit = UINT32_C(1) << (button - 1);
        if (down) buttons |= bit;
        else buttons &= ~bit;
        last_timestamp = now_us();
    }
    pthread_mutex_unlock(&mouse_lock);
}

void runtime_mouse_event_wheel(int32_t amount) {
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    if (enabled) {
        wheel += amount;
        last_timestamp = now_us();
    }
    pthread_mutex_unlock(&mouse_lock);
}

void runtime_mouse_focus_lost(void) {
    pthread_mutex_lock(&mouse_lock);
    buttons = 0;
    motion_x = motion_y = pad_motion_x = pad_motion_y = wheel = 0;
    pthread_mutex_unlock(&mouse_lock);
}

void runtime_mouse_add_stick(uint8_t x, uint8_t y, uint64_t timestamp) {
    int32_t dx, dy;
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    if (!enabled || !stick_enabled) {
        pthread_mutex_unlock(&mouse_lock);
        return;
    }
    dx = (int32_t)x - 128;
    dy = (int32_t)y - 128;
    if (abs(dx) <= 8) dx = 0;
    if (abs(dy) <= 8) dy = 0;
    motion_x += (int32_t)(dx * stick_sensitivity / 32.0f);
    motion_y += (int32_t)((invert_y ? -dy : dy) * stick_sensitivity / 32.0f);
    last_timestamp = timestamp;
    pthread_mutex_unlock(&mouse_lock);
}

int runtime_mouse_read(RuntimeMouseData *data) {
    if (!data) return 0;
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    memset(data, 0, sizeof(*data));
    data->timestamp = last_timestamp;
    data->connected = enabled ? 1 : 0;
    data->buttons = buttons;
    data->x_axis = motion_x;
    data->y_axis = motion_y;
    data->wheel = wheel;
    motion_x = motion_y = wheel = 0;
    pthread_mutex_unlock(&mouse_lock);
    return 1;
}

uint32_t runtime_mouse_pad_buttons(void) {
    uint32_t result = 0;
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    for (int button = 1; button <= 3; ++button) {
        if (buttons & (UINT32_C(1) << (button - 1))) result |= mouse_button_map[button];
    }
    pthread_mutex_unlock(&mouse_lock);
    return result;
}

void runtime_mouse_take_pad_motion(int32_t *x, int32_t *y) {
    runtime_mouse_configure();
    pthread_mutex_lock(&mouse_lock);
    if (x) *x = pad_motion_x;
    if (y) *y = pad_motion_y;
    pad_motion_x = pad_motion_y = 0;
    pthread_mutex_unlock(&mouse_lock);
}