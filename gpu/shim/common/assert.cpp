// gow3: GPU-side assertion failures stop the port with exit code 23.
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "common/assert.h"
#include "common/logging/log.h"
#include "gow3_thread_name.h"

void assert_fail_impl() {
    std::fflush(stdout);
    // gow3: names the asserting thread (a gow3:DrawPrepN worker, the GPU thread...).
    char thread[64];
    gow3_current_thread_name(thread, sizeof(thread));
    if (thread[0])
        std::fprintf(stderr, "STOP: GPU library assertion failed on thread %s (see GPU log above)\n", thread);
    else
        std::fputs("STOP: GPU library assertion failed (see GPU log above)\n", stderr);
    std::_Exit(23);
}

[[noreturn]] void unreachable_impl() {
    assert_fail_impl();
    throw std::runtime_error("Unreachable code");
}

void assert_fail_debug_msg(const char* msg) {
    LOG_CRITICAL(Debug, "Assertion Failed!\n{}", msg);
    assert_fail_impl();
}
