// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__APPLE__)
#include <os/lock.h>
#else
#include <atomic>
#endif

namespace Common {

/**
 * SpinLock class
 * a lock similar to mutex that forces a thread to spin wait instead calling the
 * supervisor. Should be used on short sequences of code.
 */
class SpinLock {
public:
    SpinLock() = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    SpinLock(SpinLock&&) = delete;
    SpinLock& operator=(SpinLock&&) = delete;

#if defined(__APPLE__)
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
#else
    void lock();
    void unlock();
    [[nodiscard]] bool try_lock();

private:
    std::atomic_flag lck = ATOMIC_FLAG_INIT;
#endif
};

} // namespace Common
