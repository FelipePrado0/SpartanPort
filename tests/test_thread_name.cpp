// SPDX-License-Identifier: GPL-2.0-or-later
// Name of the calling thread for the assertion and crash reports (gow3_thread_name.h).
// No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "gow3_thread_name.h"

// Names the calling thread as Common::SetCurrentThreadName does (UTF-8).
static void SetName(const char* name) {
#ifdef _WIN32
    wchar_t wide[64];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, 64);
    SetThreadDescription(GetCurrentThread(), wide);
#else
    pthread_setname_np(pthread_self(), name);
#endif
}

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    std::thread([] {
        SetName("gow3:DrawPrep0");
        char name[64];
        gow3_current_thread_name(name, sizeof(name));
        assert(std::strcmp(name, "gow3:DrawPrep0") == 0);
        // A small buffer keeps the start of the name, terminated.
        char small[6];
        std::memset(small, 'x', sizeof(small));
        gow3_current_thread_name(small, sizeof(small));
        assert(std::strcmp(small, "gow3:") == 0);
        char one[1] = {'x'};
        gow3_current_thread_name(one, sizeof(one));
        assert(one[0] == 0);
    }).join();
    std::thread([] {
        // UTF-8 ("gow3:Ação"); truncation never ends in half a character.
        SetName("gow3:A\xc3\xa7\xc3\xa3o");
        char name[64];
        gow3_current_thread_name(name, sizeof(name));
        assert(std::strcmp(name, "gow3:A\xc3\xa7\xc3\xa3o") == 0);
        char cut[8];
        gow3_current_thread_name(cut, sizeof(cut));
        assert(std::strcmp(cut, "gow3:A") == 0);
    }).join();
#ifdef _WIN32
    // A thread nobody named has an empty description: no name.
    std::thread([] {
        char name[64] = "stale";
        gow3_current_thread_name(name, sizeof(name));
        assert(name[0] == 0);
    }).join();
#endif

    std::puts("thread-name-test: ok");
    return 0;
}
