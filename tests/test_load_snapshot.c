/* thread_capture.h: the Windows Load: watchdog reads another thread's pc and stack, then lets it run. */
#include "thread_capture.h"
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static volatile LONG64 counter;
static volatile LONG64 sink;

__attribute__((noinline)) static void inner(void) {
    for (;;) { InterlockedIncrement64(&counter); sink+=counter; }
}
__attribute__((noinline)) static void outer(void) {
    inner();
    sink=0; /* keeps the call above a real call with a return address on the stack */
}
static DWORD WINAPI spinner(LPVOID arg) { (void)arg; outer(); return 0; }

int main(void) {
    MODULEINFO mod;
    CHECK(GetModuleInformation(GetCurrentProcess(), GetModuleHandleW(NULL), &mod, sizeof(mod)));
    uint64_t base=(uintptr_t)mod.lpBaseOfDll, size=mod.SizeOfImage;

    DWORD tid;
    HANDLE t=CreateThread(NULL, 0, spinner, NULL, 0, &tid);
    CHECK(t);
    while (counter<1000) Sleep(1);

    ThreadCapture cap;
    CHECK(!thread_capture(GetCurrentThreadId(), base, size, &cap)); /* never suspends itself */
    CHECK(thread_capture(tid, base, size, &cap));
    CHECK(cap.pc>=base && cap.pc-base<size);
    CHECK(cap.sp!=0);
    CHECK(cap.frame_count>=1);
    for (int i=0;i<cap.frame_count;++i) CHECK(cap.frames[i]>=base && cap.frames[i]-base<size);

    LONG64 before=counter;
    Sleep(20);
    CHECK(counter>before); /* resumed */

    CHECK(!thread_capture(0xfffffff0u, base, size, &cap)); /* no such thread */
    printf("PASS: thread capture reads pc and stack inside the image, skips itself and resumes the thread\n");
    return 0;
}
