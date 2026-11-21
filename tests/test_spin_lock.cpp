// SPDX-License-Identifier: GPL-2.0-or-later
// The resolvers' recursive spin lock: mutual exclusion between threads, re-entry on one thread.
#include <cassert>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "spartanport_spin_lock.h"

int main() {
    SpartanSpin::RecursiveLock lock;
    long counter = 0;
    constexpr int Threads = 4, Rounds = 200000;
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < Rounds; ++i) {
                std::unique_lock outer{lock};
                std::unique_lock inner{lock}; // re-entry, as ObtainBuffer inside a bind
                const long seen = counter;
                counter = seen + 1;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    assert(counter == long{Threads} * Rounds);
    assert(lock.try_lock());
    std::thread other([&] { assert(!lock.try_lock()); });
    other.join();
    lock.unlock();
    std::thread again([&] {
        assert(lock.try_lock());
        lock.unlock();
    });
    again.join();
    std::printf("PASS: recursive spin lock\n");
}
