// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>

#include "video_core/renderer_vulkan/vk_precompile_state.h"

namespace fs = std::filesystem;

namespace Vulkan {

namespace {

constexpr u32 MarkerMagic = 0x4D505053; // SPPM
constexpr u32 MarkerFormat = 1;

struct StoredMarker {
    u32 magic;
    u32 format;
    u64 manifest_id;
    u64 profile_hash;
    u64 driver_hash;
    u32 meta_version;
    u32 binary_version;
    u32 pipeline_version;
    u32 done; // bit 0 shaders, bit 1 pipelines
};
static_assert(sizeof(StoredMarker) == 48);

std::vector<u64> ReadJournal(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<u64> keys;
    u64 key{};
    while (file.read(reinterpret_cast<char*>(&key), sizeof(key))) {
        if (key) {
            keys.push_back(key);
        }
    }
    return keys;
}

} // namespace

std::vector<u8> EncodeMarker(const PrecompileMarker& marker) {
    const StoredMarker stored{
        .magic = MarkerMagic,
        .format = MarkerFormat,
        .manifest_id = marker.manifest_id,
        .profile_hash = marker.profile_hash,
        .driver_hash = marker.driver_hash,
        .meta_version = marker.meta_version,
        .binary_version = marker.binary_version,
        .pipeline_version = marker.pipeline_version,
        .done = u32(marker.shaders_done) | u32(marker.pipelines_done) << 1,
    };
    std::vector<u8> bytes(sizeof(stored));
    std::memcpy(bytes.data(), &stored, sizeof(stored));
    return bytes;
}

std::optional<PrecompileMarker> DecodeMarker(std::span<const u8> data) {
    StoredMarker stored;
    if (data.size() != sizeof(stored)) {
        return std::nullopt;
    }
    std::memcpy(&stored, data.data(), sizeof(stored));
    if (stored.magic != MarkerMagic || stored.format != MarkerFormat) {
        return std::nullopt;
    }
    return PrecompileMarker{
        .manifest_id = stored.manifest_id,
        .meta_version = stored.meta_version,
        .binary_version = stored.binary_version,
        .pipeline_version = stored.pipeline_version,
        .profile_hash = stored.profile_hash,
        .driver_hash = stored.driver_hash,
        .shaders_done = (stored.done & 1) != 0,
        .pipelines_done = (stored.done & 2) != 0,
    };
}

PrecompileWork DecideWork(const std::optional<PrecompileMarker>& stored,
                          const PrecompileMarker& now) {
    if (!stored || stored->profile_hash != now.profile_hash ||
        stored->meta_version != now.meta_version || stored->binary_version != now.binary_version) {
        return {.restart_cache = true, .shaders = true, .keys = true, .pipelines = true};
    }
    PrecompileWork work;
    if (stored->manifest_id != now.manifest_id) {
        work.shaders = work.keys = work.pipelines = work.new_only = true;
    }
    if (stored->pipeline_version != now.pipeline_version) {
        work.keys = work.pipelines = true;
    }
    if (stored->driver_hash != now.driver_hash) {
        work.pipelines = true;
    }
    if (!stored->shaders_done) {
        work.shaders = work.keys = work.pipelines = true;
    }
    if (!stored->pipelines_done) {
        work.pipelines = true;
    }
    return work;
}

CrashJournal::~CrashJournal() {
    // Not Finish: a run that stops early proved nothing, and its slots are clear anyway.
    if (file) {
        std::fclose(file);
    }
}

bool CrashJournal::Open(const fs::path& path_, u32 slots_) {
    std::scoped_lock lock{mutex};
    path = path_;
    slots = slots_;
    file = std::fopen(path.string().c_str(), "wb");
    if (!file) {
        return false;
    }
    const std::vector<u64> empty(slots);
    std::fwrite(empty.data(), sizeof(u64), slots, file);
    std::fflush(file);
    return true;
}

void CrashJournal::Mark(u32 slot, u64 key) {
    std::scoped_lock lock{mutex};
    if (!file || slot >= slots) {
        return;
    }
    std::fseek(file, long(slot * sizeof(u64)), SEEK_SET);
    std::fwrite(&key, sizeof(key), 1, file);
    // Handed to the OS, it outlives this process.
    std::fflush(file);
}

void CrashJournal::Clear(u32 slot) {
    Mark(slot, 0);
}

void CrashJournal::Finish() {
    std::scoped_lock lock{mutex};
    if (!file) {
        return;
    }
    std::fclose(file);
    file = nullptr;
    std::error_code ec;
    fs::remove(path, ec);
}

std::vector<u64> LoadSkipList(const fs::path& journal, const fs::path& skip_list,
                              const std::string& stamp) {
    const auto crashed = ReadJournal(journal);
    bool recorded = true;
    if (!crashed.empty()) {
        std::ofstream out(skip_list, std::ios::app);
        for (const u64 key : crashed) {
            out << stamp << ' ' << std::hex << key << std::dec << '\n';
        }
        out.close();
        recorded = !out.fail();
    }
    // The journal is the only record of the crash until the skip list holds it.
    std::vector<u64> keys;
    if (recorded) {
        std::error_code ec;
        fs::remove(journal, ec);
    } else {
        keys = crashed;
    }
    std::ifstream in(skip_list);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string line_stamp;
        u64 key{};
        if (fields >> line_stamp >> std::hex >> key && line_stamp == stamp) {
            keys.push_back(key);
        }
    }
    std::ranges::sort(keys);
    keys.erase(std::ranges::unique(keys).begin(), keys.end());
    return keys;
}

namespace {
constexpr const char* Shipped = "CUSA01623";
constexpr std::array OtherRegions{"CUSA01715", "CUSA01720", "CUSA01740"};
} // namespace

bool ShaderListFitsGame(const std::string& list_serial, const std::string& game_serial) {
    return list_serial == game_serial ||
           (list_serial == Shipped &&
            std::ranges::find(OtherRegions, game_serial) != OtherRegions.end());
}

std::filesystem::path ShaderListPath(const std::filesystem::path& dir, const std::string& serial) {
    const auto own = dir / (serial + ".spm");
    const auto shipped = dir / (std::string(Shipped) + ".spm");
    const bool other_region = std::ranges::find(OtherRegions, serial) != OtherRegions.end();
    // Runs at startup: a folder that cannot be read must not throw.
    std::error_code error;
    const auto exists = [&](const std::filesystem::path& path) {
        return std::filesystem::exists(path, error) && !error;
    };
    return other_region && !exists(own) && exists(shipped) ? shipped : own;
}

} // namespace Vulkan
