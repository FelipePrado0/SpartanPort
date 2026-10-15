// SPDX-License-Identifier: GPL-2.0-or-later
// GPU log lines (Common::Log::Write, stderr) and runtime lines (printf on stdout, unbuffered as
// probe.c sets it) reach last_run.log through one pipe (#35): threads writing both at once must
// never cut each other's lines. No game, Vulkan device or window required.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "common/logging/log.h"

#ifdef _WIN32
static int Dup(int fd) { return _dup(fd); }
static int Dup2(int from, int to) { return _dup2(from, to); }
static int Close(int fd) { return _close(fd); }
static int FileNo(FILE* file) { return _fileno(file); }
#else
static int Dup(int fd) { return dup(fd); }
static int Dup2(int from, int to) { return dup2(from, to); }
static int Close(int fd) { return close(fd); }
static int FileNo(FILE* file) { return fileno(file); }
#endif

constexpr int kThreads = 6;  // of each kind
constexpr int kLines = 400;  // per thread
constexpr const char* kPad = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static std::string RuntimeLine(int thread, int n) {
    return fmt::format("runtime t{} n{} {}", thread, n, kPad);
}
static std::string GpuMessage(int thread, int n) {
    return fmt::format("gpu t{} n{} {}", thread, n, kPad);
}
static std::string GpuLine(int thread, int n) {
    return "GPU [Test] <Error> test_log_interleave.cpp:42 Worker: " + GpuMessage(thread, n);
}

int main() {
    // As probe.c: the runtime's printf lines must not wait in a buffer.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gow3_log_interleave_test.log";
    FILE* file = std::fopen(path.string().c_str(), "wb");
    assert(file);

    // One file behind both streams, as the launcher's pipe behind stdout and stderr.
    std::fflush(stdout);
    std::fflush(stderr);
    const int saved_out = Dup(FileNo(stdout));
    const int saved_err = Dup(FileNo(stderr));
    assert(saved_out >= 0 && saved_err >= 0);
    assert(Dup2(FileNo(file), FileNo(stdout)) == 0);
    assert(Dup2(FileNo(file), FileNo(stderr)) == 0);

    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&go, t] {
            while (!go) std::this_thread::yield();
            for (int n = 0; n < kLines; ++n) std::printf("runtime t%d n%d %s\n", t, n, kPad);
        });
        threads.emplace_back([&go, t] {
            while (!go) std::this_thread::yield();
            for (int n = 0; n < kLines; ++n)
                Common::Log::Write("Test", Common::Log::Level::err, "gpu/tests/test_log_interleave.cpp",
                                   42, "Worker", GpuMessage(t, n));
        });
    }
    go = true;
    for (auto& thread : threads) thread.join();

    std::fflush(stdout);
    std::fflush(stderr);
    assert(Dup2(saved_out, FileNo(stdout)) == 0);
    assert(Dup2(saved_err, FileNo(stderr)) == 0);
    Close(saved_out);
    Close(saved_err);
    std::fclose(file);

    std::set<std::string> expected;
    for (int t = 0; t < kThreads; ++t)
        for (int n = 0; n < kLines; ++n) {
            expected.insert(RuntimeLine(t, n));
            expected.insert(GpuLine(t, n));
        }
    std::vector<std::string> lines;
    {
        std::ifstream in(path, std::ios::binary);
        for (std::string line; std::getline(in, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }
    std::filesystem::remove(path);

    // Every line is one whole runtime or GPU line, each written exactly once.
    std::set<std::string> seen;
    size_t garbled = 0;
    std::string example;
    for (const auto& line : lines) {
        if (expected.count(line) && seen.insert(line).second) continue;
        if (!garbled++) example = line;
    }
    if (garbled)
        std::printf("log-interleave-test: %zu of %zu lines garbled, e.g. \"%s\"\n", garbled,
                    lines.size(), example.substr(0, 120).c_str());
    assert(garbled == 0);
    assert(lines.size() == expected.size());
    assert(seen == expected);

    std::puts("log-interleave-test: ok");
    return 0;
}
