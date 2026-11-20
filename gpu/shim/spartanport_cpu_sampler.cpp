// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_cpu_sampler.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_map>

#include <windows.h>
#include <x86intrin.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

namespace SpartanCpuSampler {

#ifdef _WIN32
namespace {

struct Module {
    std::uintptr_t begin;
    std::uintptr_t end;
    std::string name;
};

std::string Utf8(const wchar_t* text) {
    if (!text || !*text) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string out(size > 0 ? size - 1 : 0, '\0');
    if (size > 1) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
    }
    return out;
}

std::vector<Module> LoadedModules() {
    std::vector<HMODULE> handles(1024);
    DWORD needed = 0;
    const HANDLE process = GetCurrentProcess();
    if (!K32EnumProcessModules(process, handles.data(), DWORD(handles.size() * sizeof(HMODULE)),
                               &needed)) {
        return {};
    }
    handles.resize(std::min<std::size_t>(handles.size(), needed / sizeof(HMODULE)));
    std::vector<Module> modules;
    for (const HMODULE handle : handles) {
        MODULEINFO info{};
        wchar_t path[MAX_PATH]{};
        if (!K32GetModuleInformation(process, handle, &info, sizeof(info)) ||
            !GetModuleFileNameW(handle, path, MAX_PATH)) {
            continue;
        }
        std::string name = Utf8(path);
        if (const auto slash = name.find_last_of("\\/"); slash != std::string::npos) {
            name.erase(0, slash + 1);
        }
        const auto base = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
        modules.push_back({base, base + info.SizeOfImage, std::move(name)});
    }
    std::sort(modules.begin(), modules.end(),
              [](const Module& a, const Module& b) { return a.begin < b.begin; });
    return modules;
}

struct Sampled {
    HANDLE handle;
    std::uint32_t name;
    bool named;
    ULONG64 cycles;          ///< the thread's CPU cycles at the last look
    ULONG64 used = 0;        ///< CPU cycles it used since sampling started
    std::uintptr_t stack_top; ///< its stack's highest address (TEB), 0 when unknown
};

/// Samples outside the game's executable also count, once, the nearest return address into it
/// (who called the system or driver code), marked with this bit.
constexpr std::uint64_t CallerBit = 1ull << 63;
/// And the game function one level above that one, marked with this bit.
constexpr std::uint64_t Caller2Bit = 1ull << 62;

/// The top of a thread's stack, from its TEB (read while it runs: no lock is involved).
std::uintptr_t StackTop(HANDLE thread) {
    using Query = LONG(WINAPI*)(HANDLE, int, void*, ULONG, ULONG*);
    static const auto query = reinterpret_cast<Query>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    struct BasicInformation {
        LONG exit_status;
        void* teb;
        void* client_id[2];
        ULONG_PTR affinity;
        LONG priority;
        LONG base_priority;
    } info{};
    if (!query || query(thread, 0, &info, sizeof(info), nullptr) < 0 || !info.teb) {
        return 0;
    }
    return reinterpret_cast<std::uintptr_t>(static_cast<NT_TIB*>(info.teb)->StackBase);
}

class Sampler {
public:
    explicit Sampler(std::string path_) : path{std::move(path_)} {
        const HMODULE exe = GetModuleHandleW(nullptr);
        MODULEINFO info{};
        wchar_t file[MAX_PATH]{};
        if (K32GetModuleInformation(GetCurrentProcess(), exe, &info, sizeof(info))) {
            exe_begin = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
            exe_end = exe_begin + info.SizeOfImage;
        }
        GetModuleFileNameW(exe, file, MAX_PATH);
        exe_name = Utf8(file);
        if (const auto slash = exe_name.find_last_of("\\/"); slash != std::string::npos) {
            exe_name.erase(0, slash + 1);
        }
    }

    void Run() {
        SetThreadDescription(GetCurrentThread(), L"spartanport:CpuSampler");
        const char* hz_env = std::getenv("SPARTAN_CPU_SAMPLER_HZ");
        const int hz = std::clamp(hz_env ? std::atoi(hz_env) : 250, 10, 4000);
        const auto period = std::chrono::nanoseconds(1'000'000'000 / hz);
        // A high-resolution timer: Sleep(1) waits a whole 15.6 ms tick by default.
        const HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                                    CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                    TIMER_ALL_ACCESS);
        // SPARTAN_CPU_SAMPLER_DELAY_S: start after loading and walking to the measured spot.
        const char* delay_env = std::getenv("SPARTAN_CPU_SAMPLER_DELAY_S");
        const int delay_s = delay_env ? std::max(0, std::atoi(delay_env)) : 0;
        std::printf("CPU sampler: %d Hz after %d s, writing %s every 10 s\n", hz, delay_s,
                    path.c_str());
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::seconds(delay_s));
        start_tsc = __rdtsc();
        start_time = std::chrono::steady_clock::now();
        auto next_refresh = std::chrono::steady_clock::now();
        auto next_dump = next_refresh + std::chrono::seconds(10);
        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= next_refresh) {
                RefreshThreads();
                next_refresh = now + std::chrono::milliseconds(500);
            }
            SampleOnce();
            if (now >= next_dump) {
                Dump();
                next_dump = now + std::chrono::seconds(10);
            }
            if (timer) {
                LARGE_INTEGER due{};
                due.QuadPart = -static_cast<LONGLONG>(period.count() / 100);
                SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(timer, INFINITE);
            } else {
                Sleep(1);
            }
        }
    }

private:
    std::uint32_t NameIndex(const std::string& name) {
        const auto [it, added] = name_index.try_emplace(name, std::uint32_t(names.size()));
        if (added) {
            names.push_back(name);
        }
        return it->second;
    }

    std::string ThreadName(HANDLE handle) {
        PWSTR description = nullptr;
        std::string name;
        if (SUCCEEDED(GetThreadDescription(handle, &description))) {
            name = Utf8(description);
            LocalFree(description);
        }
        return name;
    }

    void RefreshThreads() {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return;
        }
        const DWORD process = GetCurrentProcessId();
        const DWORD self = GetCurrentThreadId();
        std::unordered_map<DWORD, bool> alive;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != process || entry.th32ThreadID == self) {
                continue;
            }
            alive[entry.th32ThreadID] = true;
            auto it = threads.find(entry.th32ThreadID);
            if (it == threads.end()) {
                const HANDLE handle =
                    OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                                   THREAD_QUERY_LIMITED_INFORMATION,
                               FALSE, entry.th32ThreadID);
                if (!handle) {
                    continue;
                }
                it = threads.emplace(entry.th32ThreadID,
                                     Sampled{handle, 0, false, 0, StackTop(handle)})
                         .first;
            }
            // Threads name themselves after they start: look again until they have a name.
            if (!it->second.named) {
                std::string name = ThreadName(it->second.handle);
                it->second.named = !name.empty();
                it->second.name = NameIndex(name.empty() ? "unnamed" : name);
            }
        }
        CloseHandle(snapshot);
        for (auto it = threads.begin(); it != threads.end();) {
            if (alive.contains(it->first)) {
                ++it;
            } else {
                retired_used[it->second.name] += it->second.used; // a thread that ended
                CloseHandle(it->second.handle);
                it = threads.erase(it);
            }
        }
    }

    void SampleOnce() {
        for (auto& [tid, thread] : threads) {
            // Only threads that ran since the last look: a waiting thread is not CPU time (and
            // is not paused for nothing).
            ULONG64 cycles = 0;
            if (!QueryThreadCycleTime(thread.handle, &cycles) || cycles == thread.cycles) {
                continue;
            }
            if (thread.cycles) {
                thread.used += cycles - thread.cycles;
            }
            thread.cycles = cycles;
            // Nothing between the pause and the resume may allocate or take a lock: the paused
            // thread may hold it.
            if (SuspendThread(thread.handle) == DWORD(-1)) {
                continue;
            }
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            const bool got = GetThreadContext(thread.handle, &context);
            std::uint64_t caller2 = 0;
            const std::uint64_t caller = got ? Caller(context, thread.stack_top, caller2) : 0;
            ResumeThread(thread.handle);
            if (got) {
                table.Add(thread.name, context.Rip);
                if (caller) {
                    table.Add(thread.name, caller | CallerBit);
                }
                if (caller2) {
                    table.Add(thread.name, caller2 | Caller2Bit);
                }
                ++samples;
            }
        }
    }

    [[nodiscard]] bool InExe(std::uint64_t address) const {
        return address >= exe_begin && address < exe_end;
    }

    /// While the thread is paused (plain reads of its own stack, no lock): for a sample outside
    /// the executable, the return address that leads back into it. [RSP] first (leaf code such
    /// as memcpy or a system call stub), then the frame pointer chain (the executable keeps
    /// frame pointers).
    /// `caller2`: the next return address into the executable above the first one (the frame
    /// pointer chain), 0 when there is none.
    [[nodiscard]] std::uint64_t Caller(const CONTEXT& context, std::uintptr_t top,
                                       std::uint64_t& caller2) const {
        caller2 = 0;
        if (InExe(context.Rip) || !top || context.Rsp >= top) {
            return 0;
        }
        const auto read = [&](std::uint64_t at) -> std::uint64_t {
            return at >= context.Rsp && at + 8 <= top ? *reinterpret_cast<const std::uint64_t*>(at)
                                                     : 0;
        };
        std::uint64_t first = 0;
        if (const std::uint64_t ret = read(context.Rsp); InExe(ret)) {
            first = ret;
        }
        std::uint64_t frame = context.Rbp;
        for (int depth = 0; depth < 16; ++depth) {
            const std::uint64_t ret = read(frame + 8);
            if (!ret) {
                break;
            }
            if (InExe(ret) && ret != first) {
                if (!first) {
                    first = ret;
                } else {
                    caller2 = ret;
                    break;
                }
            }
            const std::uint64_t next = read(frame);
            if (next <= frame) {
                break;
            }
            frame = next;
        }
        return first;
    }

    void Dump() {
        const auto modules = LoadedModules();
        const std::string temp = path + ".tmp";
        std::FILE* file = std::fopen(temp.c_str(), "w");
        if (!file) {
            return;
        }
        table.ForEach([&](std::uint32_t name, std::uint64_t address, std::uint32_t count) {
            if (address & (CallerBit | Caller2Bit)) {
                std::fprintf(file, "%s\t%s:%s\t%llx\t%u\n", names[name].c_str(),
                             address & CallerBit ? "via" : "via2", exe_name.c_str(),
                             static_cast<unsigned long long>(
                                 (address & ~(CallerBit | Caller2Bit)) - exe_begin),
                             count);
                return;
            }
            const auto it = std::upper_bound(
                modules.begin(), modules.end(), address,
                [](std::uint64_t a, const Module& m) { return a < m.begin; });
            const Module* module =
                it != modules.begin() && address < std::prev(it)->end ? &*std::prev(it) : nullptr;
            std::fprintf(file, "%s\t%s\t%llx\t%u\n", names[name].c_str(),
                         module ? module->name.c_str() : "?",
                         static_cast<unsigned long long>(module ? address - module->begin : address),
                         count);
        });
        // CPU time per thread name (cycles used while sampled), and the time sampled: what each
        // thread costs per frame (perf/auto/cpu_profile.py --frames).
        std::unordered_map<std::uint32_t, ULONG64> used;
        for (const auto& [tid, thread] : threads) {
            used[thread.name] += thread.used;
        }
        for (const auto& [name, cycles] : used) {
            std::fprintf(file, "%s\t#cycles\t0\t%llu\n", names[name].c_str(),
                         static_cast<unsigned long long>(cycles + retired_used[name]));
        }
        const double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
        std::fprintf(file, "#\t#elapsed\t%llu\t%llu\n",
                     static_cast<unsigned long long>(__rdtsc() - start_tsc),
                     static_cast<unsigned long long>(seconds * 1000.0));
        std::fclose(file);
        std::remove(path.c_str());
        std::rename(temp.c_str(), path.c_str());
        std::printf("CPU sampler: %llu samples of %zu threads (%llu dropped), in %s\n",
                    static_cast<unsigned long long>(samples), threads.size(),
                    static_cast<unsigned long long>(table.Dropped()), path.c_str());
        std::fflush(stdout);
    }

    std::string path;
    std::uintptr_t exe_begin = 0, exe_end = 0;
    std::string exe_name;
    Table table{20};
    std::unordered_map<DWORD, Sampled> threads;
    std::vector<std::string> names;
    std::unordered_map<std::string, std::uint32_t> name_index;
    std::uint64_t samples = 0;
    std::uint64_t start_tsc = 0;
    std::chrono::steady_clock::time_point start_time{};
    std::unordered_map<std::uint32_t, ULONG64> retired_used;
};

} // namespace

void StartFromEnv() {
    static std::once_flag once;
    std::call_once(once, [] {
        const char* env = std::getenv("SPARTAN_CPU_SAMPLER");
        if (!env || !*env || (env[0] == '0' && !env[1])) {
            return;
        }
        std::string path = (env[0] == '1' && !env[1]) ? "cpu_samples.tsv" : env;
        std::thread([path = std::move(path)] { Sampler{path}.Run(); }).detach();
    });
}
#else
// shortcut: no sampler outside Windows yet; perf works there without it.
void StartFromEnv() {}
#endif

} // namespace SpartanCpuSampler
