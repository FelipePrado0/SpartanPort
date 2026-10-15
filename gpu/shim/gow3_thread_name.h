// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: name of the calling thread (gow3:DrawPrep0, shadPS4:GpuCommandProcessor, gow3:main...)
// for the GPU assertion STOP line and the Windows fault report. Plain C: src/probe.c and the
// GPU library both use it. Without _GNU_SOURCE glibc has no pthread_getname_np: C files include
// it from Windows code only (C++ compilers define _GNU_SOURCE).
#pragma once

#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// UTF-8 name of the calling thread, or "" (unnamed, or the name cannot be read). Always
// terminated; a name longer than size - 1 bytes is cut before a whole UTF-8 character.
static inline void gow3_current_thread_name(char* out, size_t size) {
    if (!out || !size) return;
    out[0] = 0;
    char name[256];
#ifdef _WIN32
    PWSTR wide = NULL;
    if (FAILED(GetThreadDescription(GetCurrentThread(), &wide)) || !wide) return;
    int converted = WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, (int)sizeof(name), NULL, NULL);
    LocalFree(wide);
    if (converted <= 0) return;
#else
    if (pthread_getname_np(pthread_self(), name, sizeof(name)) != 0) return;
#endif
    size_t length = strlen(name);
    if (length >= size) {
        length = size - 1;
        while (length && ((unsigned char)name[length] & 0xc0) == 0x80) --length;
    }
    memcpy(out, name, length);
    out[length] = 0;
}
