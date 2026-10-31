/* Where another thread of this process is: its pc, its sp and up to THREAD_CAPTURE_FRAMES
 * addresses found on its stack inside [base, base+size). Windows only, for the Load: watchdog
 * (docs/specs/2026-10-29-load-stall-watchdog.md). The thread stays suspended only while its
 * registers and THREAD_CAPTURE_STACK bytes of stack are copied: nothing in between takes a lock,
 * so a thread suspended inside malloc or stdio cannot block the caller. */
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <stdint.h>

enum { THREAD_CAPTURE_STACK = 4096, THREAD_CAPTURE_FRAMES = 8 };
typedef struct {
    uint64_t pc, sp;
    uint64_t frames[THREAD_CAPTURE_FRAMES];
    int frame_count;
} ThreadCapture;

/* 1 when the registers were read; the stack scan may still find nothing. */
static inline int thread_capture(unsigned long tid,uint64_t base,uint64_t size,ThreadCapture *out) {
    out->pc=out->sp=0; out->frame_count=0;
    if (tid==GetCurrentThreadId()) return 0;
    HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT,FALSE,(DWORD)tid);
    if (!h) return 0;
    static uint64_t stack[THREAD_CAPTURE_STACK/8];
    SIZE_T got=0;
    int ok=0;
    if (SuspendThread(h)!=(DWORD)-1) {
        CONTEXT c={.ContextFlags=CONTEXT_CONTROL};
        if (GetThreadContext(h,&c)) {
            ok=1;
            out->pc=c.Rip; out->sp=c.Rsp;
            /* ReadProcessMemory fails instead of faulting when the stack ends inside the window. */
            for (SIZE_T want=sizeof(stack);want>=64 && !got;want/=2)
                if (!ReadProcessMemory(GetCurrentProcess(),(LPCVOID)(uintptr_t)c.Rsp,stack,want,&got)) got=0;
        }
        ResumeThread(h);
    }
    CloseHandle(h);
    for (SIZE_T i=0;i<got/8 && out->frame_count<THREAD_CAPTURE_FRAMES;++i)
        if (size && stack[i]>=base && stack[i]-base<size) out->frames[out->frame_count++]=stack[i];
    return ok;
}
#endif
