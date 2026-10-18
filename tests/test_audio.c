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

static SDL_AudioStream *mock_fail_wasapi(SDL_AudioDeviceID dev, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *user) {
    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver && !strcmp(driver, "wasapi")) {
        SDL_SetError("WASAPI can't initialize audio client: ");
        return NULL;
    }
    return SDL_OpenAudioDeviceStream(dev, spec, cb, user);
}

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

int main(void) {
    test_lifecycle();
    test_invalid_parameters();
#ifdef _WIN32
    test_wasapi_fallback();
    test_env_override_prevents_fallback();
#endif
    test_complete_failure_timer_sink();

    printf("\naudio tests ok\n");
    return 0;
}
