/* SPDX-License-Identifier: GPL-2.0-or-later */
/* AudioOut initialization, stream open, playback and backend fallback tests. */
#define _GNU_SOURCE
#define TEST_AUDIO_HOOK
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/runtime_audio.c"

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

#ifdef _WIN32
static SDL_AudioStream *mock_fail_wasapi(SDL_AudioDeviceID dev, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *user) {
    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver && !strcmp(driver, "wasapi")) {
        SDL_SetError("WASAPI can't initialize audio client: ");
        return NULL;
    }
    return SDL_OpenAudioDeviceStream(dev, spec, cb, user);
}
#endif

static SDL_AudioStream *mock_fail_all(SDL_AudioDeviceID dev, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *user) {
    (void)dev; (void)spec; (void)cb; (void)user;
    SDL_SetError("Mock audio device open failure");
    return NULL;
}

static void test_lifecycle(void) {
    printf("Testing audio lifecycle...\n");
    assert(audio_init() == 0);
    assert(audio_init() == ERR_ALREADY_INIT);

    /* Open valid port (type 0, 8ch float, 256 frames) */
    int32_t handle = audio_open(0, 0, 0, 256, 48000, 5);
    assert(handle > 0);
    int id = handle & 0xff;
    assert(ports[id].used == 1);
    assert(ports[id].channels == 8);
    assert(ports[id].is_float == 1);
    assert(ports[id].frames == 256);

    /* Test volume setting */
    int32_t vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    assert(audio_volume(handle, 0, vol) == 0);

    /* Test audio output */
    float buffer[256 * 8] = {0};
    int32_t samples = audio_output(handle, buffer);
    assert(samples == 256 * 8);

    /* Close port */
    assert(audio_close(handle) == 0);
    assert(ports[id].used == 0);
    assert(ports[id].stream == NULL);

    /* Output to closed port returns error */
    assert(audio_output(handle, buffer) == ERR_NOT_OPENED);
    printf("Audio lifecycle passed!\n");
}

static void test_invalid_parameters(void) {
    printf("Testing invalid parameters...\n");
    assert(audio_open(0, 0, 0, 0, 48000, 5) == ERR_INVALID_SIZE);
    assert(audio_open(0, 0, 0, 3000, 48000, 5) == ERR_INVALID_SIZE);
    assert(audio_open(0, 0, 0, 250, 48000, 5) == ERR_INVALID_SIZE);
    assert(audio_open(0, 0, 0, 256, 44100, 5) == ERR_INVALID_FREQ);
    assert(audio_open(0, 0, 0, 256, 48000, 9) == ERR_INVALID_FORMAT);
    assert(audio_open(0, 99, 0, 256, 48000, 5) == ERR_INVALID_TYPE);
    printf("Invalid parameters passed!\n");
}

#ifdef _WIN32
static void test_wasapi_fallback(void) {
    printf("Testing WASAPI fallback to DirectSound on failure...\n");
    /* Reset audio subsystem to default (wasapi) */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "wasapi", SDL_HINT_OVERRIDE);
    sdl_ready = -1;

    test_open_stream = mock_fail_wasapi;

    int32_t handle = audio_open(0, 0, 0, 256, 48000, 5);
    assert(handle > 0);
    int id = handle & 0xff;

    /* Verify fallback switched backend to directsound and stream is open */
    assert(!strcmp(SDL_GetCurrentAudioDriver(), "directsound"));
    assert(ports[id].stream != NULL);

    /* Verify output works on directsound stream */
    float buffer[256 * 8] = {0};
    int32_t samples = audio_output(handle, buffer);
    assert(samples == 256 * 8);

    assert(audio_close(handle) == 0);
    test_open_stream = NULL;
    printf("WASAPI fallback passed!\n");
}

static void test_env_override_prevents_fallback(void) {
    printf("Testing SDL_AUDIO_DRIVER environment override...\n");
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "wasapi", SDL_HINT_OVERRIDE);
    sdl_ready = -1;

    putenv("SDL_AUDIO_DRIVER=wasapi");
    test_open_stream = mock_fail_wasapi;

    int32_t handle = audio_open(0, 0, 0, 256, 48000, 5);
    assert(handle > 0);
    int id = handle & 0xff;

    /* Fallback must NOT be attempted when the user forced a driver */
    assert(!strcmp(SDL_GetCurrentAudioDriver(), "wasapi"));
    assert(ports[id].stream == NULL);

    assert(audio_close(handle) == 0);
    test_open_stream = NULL;
    putenv("SDL_AUDIO_DRIVER=");
    printf("SDL_AUDIO_DRIVER override test passed!\n");
}

static SDL_AudioStream *mock_fail_port8(SDL_AudioDeviceID dev, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *user) {
    if (ports[8].used && !ports[8].stream) {
        SDL_SetError("WASAPI secondary port failure");
        return NULL;
    }
    return SDL_OpenAudioDeviceStream(dev, spec, cb, user);
}

static void test_secondary_port_failure_leaves_active_stream_intact(void) {
    printf("Testing secondary port failure leaves active primary stream intact...\n");
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "wasapi", SDL_HINT_OVERRIDE);
    sdl_ready = -1;

    int32_t h0 = audio_open(0, 0, 0, 256, 48000, 5);
    assert(h0 > 0);
    int id0 = h0 & 0xff;
    assert(!strcmp(SDL_GetCurrentAudioDriver(), "wasapi"));
    assert(ports[id0].stream != NULL);

    test_open_stream = mock_fail_port8;

    int32_t h8 = audio_open(0, 1, 0, 256, 48000, 5);
    assert(h8 > 0);
    int id8 = h8 & 0xff;

    /* Backend must NOT be switched because port 0 has an active stream */
    assert(!strcmp(SDL_GetCurrentAudioDriver(), "wasapi"));
    assert(ports[id0].stream != NULL);
    assert(ports[id8].stream == NULL);

    float buffer[256 * 8] = {0};
    assert(audio_output(h0, buffer) == 256 * 8);
    assert(audio_output(h8, buffer) == 256 * 8);

    assert(audio_close(h8) == 0);
    assert(audio_close(h0) == 0);
    test_open_stream = NULL;
    printf("Secondary port failure test passed!\n");
}

static SDL_AudioStream *mock_fail_both_wasapi_and_ds(SDL_AudioDeviceID dev, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *user) {
    (void)dev; (void)spec; (void)cb; (void)user;
    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver && !strcmp(driver, "wasapi")) {
        SDL_SetError("WASAPI can't initialize audio client: ");
    } else {
        SDL_SetError("DirectSound device creation failed");
    }
    return NULL;
}

static void test_wasapi_and_directsound_both_fail_timer_sink(void) {
    printf("Testing complete failure sequence (WASAPI fails, DirectSound fails -> timer sink)...\n");
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "wasapi", SDL_HINT_OVERRIDE);
    sdl_ready = -1;

    test_open_stream = mock_fail_both_wasapi_and_ds;

    int32_t handle = audio_open(0, 0, 0, 256, 48000, 5);
    assert(handle > 0);
    int id = handle & 0xff;

    assert(!strcmp(SDL_GetCurrentAudioDriver(), "directsound"));
    assert(ports[id].stream == NULL);

    float buffer[256 * 8] = {0};
    assert(audio_output(handle, buffer) == 256 * 8);

    assert(audio_close(handle) == 0);
    test_open_stream = NULL;
    printf("Complete failure sequence test passed!\n");
}
#endif

static void test_complete_failure_timer_sink(void) {
    printf("Testing complete backend failure graceful timer sink...\n");
    test_open_stream = mock_fail_all;

    int32_t handle = audio_open(0, 0, 0, 256, 48000, 5);
    assert(handle > 0);
    int id = handle & 0xff;

    assert(ports[id].stream == NULL);

    /* Paced timer sink outputs samples without error */
    float buffer[256 * 8] = {0};
    int32_t samples = audio_output(handle, buffer);
    assert(samples == 256 * 8);

    assert(audio_close(handle) == 0);
    test_open_stream = NULL;
    printf("Timer sink fallback passed!\n");
}

static void test_multiple_ports(void) {
    printf("Testing multiple active audio ports...\n");
    int32_t h0 = audio_open(0, 0, 0, 256, 48000, 5);  /* type 0: main */
    int32_t h8 = audio_open(0, 1, 0, 256, 48000, 5);  /* type 1: bgm */
    int32_t h17 = audio_open(0, 4, 0, 256, 48000, 5); /* type 4: pad speaker */
    assert(h0 > 0 && h8 > 0 && h17 > 0);

    int id0 = h0 & 0xff, id8 = h8 & 0xff, id17 = h17 & 0xff;
    assert(ports[id0].used && ports[id8].used && ports[id17].used);

    /* Output to all 3 ports concurrently via audio_outputs */
    float buffer[256 * 8] = {0};
    OutputParam params[3] = {
        {h0, buffer},
        {h8, buffer},
        {h17, buffer}
    };
    int32_t samples = audio_outputs(params, 3);
    assert(samples == 256 * 8);

#ifdef _WIN32
    if (ports[id0].stream) {
        /* When an active stream exists on a port, try_audio_fallback must refuse to switch backends */
        const char *driver_before = SDL_GetCurrentAudioDriver();
        try_audio_fallback(&ports[id8], id8);
        assert(!strcmp(SDL_GetCurrentAudioDriver(), driver_before));
    }
#endif

    assert(audio_close(h0) == 0);
    assert(audio_close(h8) == 0);
    assert(audio_close(h17) == 0);
    printf("Multiple active audio ports passed!\n");
}

int main(void) {
    test_lifecycle();
    test_invalid_parameters();
    test_multiple_ports();
#ifdef _WIN32
    test_wasapi_fallback();
    test_env_override_prevents_fallback();
    test_secondary_port_failure_leaves_active_stream_intact();
    test_wasapi_and_directsound_both_fail_timer_sink();
#endif
    test_complete_failure_timer_sink();

    printf("\naudio tests ok\n");
    return 0;
}
