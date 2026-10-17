// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__APPLE__)
#include <os/lock.h>
#elif defined(__unix__)
#include <pthread.h>
#endif

namespace Common {

#if defined(PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP)
class AdaptiveMutex {
public:
    AdaptiveMutex() = default;
    ~AdaptiveMutex() {
        pthread_mutex_destroy(&mutex);
    }
    AdaptiveMutex(const AdaptiveMutex&) = delete;
    AdaptiveMutex& operator=(const AdaptiveMutex&) = delete;

    void lock() {
        pthread_mutex_lock(&mutex);
    }
    void unlock() {
        pthread_mutex_unlock(&mutex);
    }
    [[nodiscard]] bool try_lock() {
        return pthread_mutex_trylock(&mutex) == 0;
    }

private:
    pthread_mutex_t mutex = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;
};
#elif defined(__APPLE__)
class AdaptiveMutex {
public:
    AdaptiveMutex() = default;
    ~AdaptiveMutex() = default;
    AdaptiveMutex(const AdaptiveMutex&) = delete;
    AdaptiveMutex& operator=(const AdaptiveMutex&) = delete;

    void lock() {
        os_unfair_lock_lock(&lock_);
    }
    void unlock() {
        os_unfair_lock_unlock(&lock_);
    }
    [[nodiscard]] bool try_lock() {
        return os_unfair_lock_trylock(&lock_);
    }

private:
    os_unfair_lock lock_ = OS_UNFAIR_LOCK_INIT;
};
#endif

} // namespace Common
