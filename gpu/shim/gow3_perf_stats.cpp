// SPDX-License-Identifier: GPL-2.0-or-later
#include "gow3_perf_stats.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include "video_core/renderer_vulkan/vk_instance.h"

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#endif

namespace Gow3PerfStats {

namespace {

std::mutex mutex;
Sample latest;
std::once_flag started;

#ifdef _WIN32
uint64_t Ticks(const FILETIME& time) {
    return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

// This process's use of the GPU's 3D engines, as Task Manager shows it ("GPU Engine" counters,
// any vendor). Negative until the second sample or without the counters.
class GpuUsage {
public:
    GpuUsage() {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) {
            query = nullptr;
            return;
        }
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter) !=
            ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            return;
        }
        PdhCollectQueryData(query);
        pid = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
    }

    float Percent() {
        if (!query || PdhCollectQueryData(query) != ERROR_SUCCESS) {
            return -1.0f;
        }
        DWORD size = 0, count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count,
                                         nullptr) != PDH_MORE_DATA) {
            return -1.0f;
        }
        buffer.resize(size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count,
                                         items) != ERROR_SUCCESS) {
            return -1.0f;
        }
        double total = 0.0;
        for (DWORD i = 0; i < count; ++i) {
            const wchar_t* name = items[i].szName;
            if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA && wcsstr(name, pid.c_str()) &&
                wcsstr(name, L"engtype_3D")) {
                total += items[i].FmtValue.doubleValue;
            }
        }
        return std::clamp(float(total), 0.0f, 100.0f);
    }

private:
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    std::wstring pid;
    std::vector<unsigned char> buffer;
};

void Run(const Vulkan::Instance* instance) {
    GpuUsage gpu;
    FILETIME created, ended, kernel, user, now;
    uint64_t last_process = 0, last_wall = 0;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
    while (true) {
        Sample s;
        s.gpu_name = std::string(instance->GetModelName());
        GetSystemTimeAsFileTime(&now);
        if (GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user)) {
            const uint64_t process = Ticks(kernel) + Ticks(user), wall = Ticks(now);
            if (last_wall) {
                s.cpu_percent = CpuPercent(process - last_process, wall - last_wall, cores);
            }
            last_process = process;
            last_wall = wall;
        }
        PROCESS_MEMORY_COUNTERS memory{};
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) {
            s.ram_used_gb = double(memory.WorkingSetSize) / GiB;
        }
        MEMORYSTATUSEX system{.dwLength = sizeof(MEMORYSTATUSEX)};
        if (GlobalMemoryStatusEx(&system)) {
            s.ram_total_gb = double(system.ullTotalPhys) / GiB;
        }
        if (instance->CanReportMemoryUsage()) {
            s.vram_used_gb = double(instance->GetDeviceMemoryUsage()) / GiB;
            s.vram_total_gb = double(instance->GetDeviceMemoryBudgetNow()) / GiB;
        }
        s.gpu_percent = gpu.Percent();
        {
            std::scoped_lock lock{mutex};
            latest = std::move(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>

void Run(const Vulkan::Instance* instance) {
    uint64_t last_process_us = 0, last_wall_us = 0;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
    while (true) {
        Sample s;
        s.gpu_name = std::string(instance->GetModelName());

        // GPU percentage via IOKit IOAccelerator
        CFMutableDictionaryRef matching = IOServiceMatching("IOAccelerator");
        io_iterator_t iterator;
        if (matching && IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) == KERN_SUCCESS) {
            io_registry_entry_t entry;
            while ((entry = IOIteratorNext(iterator)) != 0) {
                CFMutableDictionaryRef props = nullptr;
                if (IORegistryEntryCreateCFProperties(entry, &props, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS && props) {
                    CFDictionaryRef stats = (CFDictionaryRef)CFDictionaryGetValue(props, CFSTR("PerformanceStatistics"));
                    if (stats) {
                        CFNumberRef util = (CFNumberRef)CFDictionaryGetValue(stats, CFSTR("Device Utilization %"));
                        if (util) {
                            int64_t val = 0;
                            if (CFNumberGetValue(util, kCFNumberSInt64Type, &val)) {
                                s.gpu_percent = std::clamp(float(val), 0.0f, 100.0f);
                            }
                        }
                    }
                    CFRelease(props);
                }
                IOObjectRelease(entry);
                if (s.gpu_percent >= 0.0f) {
                    break;
                }
            }
            IOObjectRelease(iterator);
        }

        // CPU percentage
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            const uint64_t process_us = uint64_t(usage.ru_utime.tv_sec) * 1000000ULL + usage.ru_utime.tv_usec +
                                        uint64_t(usage.ru_stime.tv_sec) * 1000000ULL + usage.ru_stime.tv_usec;
            const auto now = std::chrono::steady_clock::now();
            const uint64_t wall_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
            if (last_wall_us > 0 && wall_us > last_wall_us) {
                s.cpu_percent = CpuPercent(process_us - last_process_us, wall_us - last_wall_us, cores);
            }
            last_process_us = process_us;
            last_wall_us = wall_us;
        }

        // Process RAM & Total RAM
        mach_task_basic_info_data_t task_info_data;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&task_info_data, &count) == KERN_SUCCESS) {
            s.ram_used_gb = double(task_info_data.resident_size) / GiB;
        }
        int64_t mem_size = 0;
        size_t len = sizeof(mem_size);
        if (sysctlbyname("hw.memsize", &mem_size, &len, nullptr, 0) == 0) {
            s.ram_total_gb = double(mem_size) / GiB;
        }

        // VRAM (VK_EXT_memory_budget or unified memory fallback)
        if (instance->CanReportMemoryUsage()) {
            s.vram_used_gb = double(instance->GetDeviceMemoryUsage()) / GiB;
            s.vram_total_gb = double(instance->GetDeviceMemoryBudgetNow()) / GiB;
        }
        if (s.vram_used_gb < 0.0 && s.ram_used_gb >= 0.0) {
            s.vram_used_gb = s.ram_used_gb;
            s.vram_total_gb = s.ram_total_gb;
        }

        {
            std::scoped_lock lock{mutex};
            latest = std::move(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
#else
void Run(const Vulkan::Instance* instance) {
    while (true) {
        Sample s;
        s.gpu_name = std::string(instance->GetModelName());
        {
            std::scoped_lock lock{mutex};
            latest = std::move(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
#endif

} // namespace

void Start(const Vulkan::Instance& instance) {
    // ponytail: never joined; a thread that ended while the game ran crashed in ntdll
    // (vk_pipeline_serialization.cpp, StopWarmUp). It sleeps between samples.
    std::call_once(started, [&] { std::thread(Run, &instance).detach(); });
}

Sample Latest() {
    std::scoped_lock lock{mutex};
    return latest;
}

} // namespace Gow3PerfStats
