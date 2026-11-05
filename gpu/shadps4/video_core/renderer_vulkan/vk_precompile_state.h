// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdio>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "common/types.h"

namespace Vulkan {

// spartanport: what the shader preparation has done, for which build, GPU, driver and manifest.
struct PrecompileMarker {
    u64 manifest_id{};
    u32 meta_version{};
    u32 binary_version{};
    u32 pipeline_version{};
    u64 profile_hash{};
    u64 driver_hash{};
    bool shaders_done{};
    bool pipelines_done{};
};

std::vector<u8> EncodeMarker(const PrecompileMarker& marker);

/// Nothing when the data is damaged or of another format: everything is then prepared again.
std::optional<PrecompileMarker> DecodeMarker(std::span<const u8> data);

struct PrecompileWork {
    bool restart_cache{}; ///< The saved files are of no use.
    bool shaders{};
    bool keys{};          ///< Write the pipeline keys again.
    bool pipelines{};
    bool new_only{};      ///< Keep what is there and add what the manifest has new.

    [[nodiscard]] bool Any() const {
        return restart_cache || shaders || keys || pipelines;
    }
};

/// stored: the saved marker. now: this build, GPU, driver and manifest.
PrecompileWork DecideWork(const std::optional<PrecompileMarker>& stored,
                          const PrecompileMarker& now);

/// Survives a driver that kills the process: while a worker is inside the driver, its slot in this
/// file names the pipeline. A run that ends well deletes the file.
class CrashJournal {
public:
    CrashJournal() = default;
    ~CrashJournal();
    CrashJournal(const CrashJournal&) = delete;
    CrashJournal& operator=(const CrashJournal&) = delete;

    bool Open(const std::filesystem::path& path, u32 slots);
    void Mark(u32 slot, u64 key);
    void Clear(u32 slot);
    /// The run ended well: nothing to skip next time.
    void Finish();

private:
    std::mutex mutex;
    std::FILE* file{};
    std::filesystem::path path;
    u32 slots{};
};

/// Moves what a crashed run left in the journal to the skip list, under this GPU and driver, and
/// returns the keys to skip for them. Keys of another GPU or driver stay in the list unused, so a
/// new driver tries them again. stamp holds no spaces.
std::vector<u64> LoadSkipList(const std::filesystem::path& journal,
                              const std::filesystem::path& skip_list, const std::string& stamp);

} // namespace Vulkan
