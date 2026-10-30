// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/path_util.h"
#include "common/singleton.h"
#include "common/types.h"

// spartanport: exit hooks observe startup safely and bound cache-drain waits.
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

namespace Storage {

enum class BlobType : u32 {
    ShaderMeta,
    ShaderBinary,
    PipelineKey,
    ShaderProfile,
};

class DataBase {
public:
    static DataBase& Instance() {
        // spartanport: window close may race the first cache access during renderer startup.
        static auto* const database = Common::Singleton<DataBase>::Instance();
        return *database;
    }

    void Open();
    void Close();
    // spartanport: wait for queued and active writes; false if the timeout expires first.
    bool Flush(std::chrono::milliseconds timeout);
    [[nodiscard]] bool IsOpened() const {
        return opened;
    }
    void FinishPreload();
    /// spartanport: removes every cached blob (an incompatible cache is rebuilt, not ignored).
    void Clear();

    bool Save(BlobType type, const std::string& name, std::vector<u8>&& data);
    bool Save(BlobType type, const std::string& name, std::vector<u32>&& data);

    void Load(BlobType type, const std::string& name, std::vector<u8>& data);
    void Load(BlobType type, const std::string& name, std::vector<u32>& data);

    /// spartanport: `stop` returning true ends the walk early (background warm-up at exit).
    void ForEachBlob(BlobType type, const std::function<void(std::vector<u8>&& data)>& func,
                     const std::function<bool()>& stop = {});
    /// spartanport: blobs are plain files written beside and renamed into place, so other threads
    /// may read them while the IO thread saves (the zip archive cannot).
    [[nodiscard]] bool ConcurrentReads() const;
    /// spartanport: number of blobs ForEachBlob visits (the loading screen's total).
    size_t CountBlobs(BlobType type);
    /// spartanport: names (without extension) of the blobs of `type`; empty for the zip archive.
    std::vector<std::string> ListBlobs(BlobType type);

private:
    std::jthread io_worker{};
    std::filesystem::path cache_path{};
    // spartanport: publish initialization safely to an exit hook on another thread.
    std::atomic<bool> opened{};
};

} // namespace Storage
