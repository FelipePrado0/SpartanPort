// gow3: see log.h.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common/logging/log.h"

namespace Common::Log {
static Level threshold() {
    static const Level level = [] {
        const char* v = std::getenv("GOW3_GPU_LOG");
        if (!v) return Level::warn;
        static const char* names[] = {"trace", "debug", "info", "warning", "error", "critical"};
        for (int i = 0; i < 6; ++i)
            if (!std::strcmp(v, names[i])) return Level(i);
        return Level::warn;
    }();
    return level;
}
bool ShouldLog(Level level) { return level >= threshold(); }
void Write(std::string_view log_class, Level level, const char* file, int line, const char* function,
           std::string&& message) {
    static const char* names[] = {"Trace", "Debug", "Info", "Warning", "Error", "Critical"};
    const char* base = std::strrchr(file, '/');
    const std::string text = fmt::format("GPU [{}] <{}> {}:{} {}: {}\n", log_class, names[int(level)],
                                         base ? base + 1 : file, line, function, message.c_str());
    // gow3: the launcher sends stdout and stderr down one pipe into last_run.log, and the
    // unbuffered streams write a character at a time, so a GPU line and a runtime printf line
    // from another thread came out cut into each other (#35). The runtime's printf holds the
    // stdout lock for the whole line; holding it here too puts the two streams in turn. It also
    // keeps GPU lines from cutting each other. No deadlock: the CRT/glibc FILE locks are
    // recursive for their owning thread, and nothing below calls back into printf or the log.
    // The fault and STOP paths (probe.c, assert.cpp) stay lock-free on purpose.
#ifdef _WIN32
    _lock_file(stdout);
#else
    flockfile(stdout);
#endif
    std::fwrite(text.data(), 1, text.size(), stderr);
#ifdef _WIN32
    _unlock_file(stdout);
#else
    funlockfile(stdout);
#endif
}
} // namespace Common::Log
