// SPDX-License-Identifier: GPL-2.0-or-later
// Run: out/gpu/cache-storage-test <dir> (a new directory that the test creates and removes).
// Exercise the production IO worker without a window, guest process or Vulkan device.
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>
#include <vector>
#include "../gpu/spartanportgpu.h"
// Include the implementation so optimized builds need not export its internal template writer.
#include "../gpu/shadps4/video_core/cache_storage.cpp"

// Mirror the production metadata shim without starting spartanportgpu_init's window/device.
namespace Core {
class Emulator {
public:
    static void FillElfInfo(const SpartanGpuConfig& config) {
        auto& info = Common::ElfInfo::Instance();
        info.initialized = true;
        info.game_serial = config.serial ? config.serial : "UNKNOWN";
        info.title = config.title ? config.title : "";
        info.sdk_ver = config.sdk_version;
        info.psf_attributes.raw = config.psf_attributes;
    }
};
} // namespace Core

int main(int argc, char** argv) {
    assert(argc == 2); // Supply a new, test-owned directory inside the workspace.
    const auto root = std::filesystem::absolute(argv[1]);
    assert(!std::filesystem::exists(root));
    std::filesystem::create_directories(root);

    // Window close and renderer startup can make the first database access concurrently.
    std::vector<Storage::DataBase*> databases(8);
    std::barrier start{8};
    std::vector<std::thread> readers;
    for (unsigned i = 0; i < databases.size(); ++i) {
        readers.emplace_back([&, i] {
            start.arrive_and_wait();
            databases[i] = &Storage::DataBase::Instance();
        });
    }
    for (auto& reader : readers) {
        reader.join();
    }
    for (auto* database : databases) {
        assert(database == databases.front());
    }

    const auto save = [&](unsigned i) {
        assert(Storage::WriteVector(Storage::BlobType::ShaderBinary, root / std::to_string(i),
                                    std::vector<u32>{i, i + 1}));
    };
    const auto verify = [&](unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            const auto path = root / (std::to_string(i) + ".spv");
            assert(std::filesystem::file_size(path) == 2 * sizeof(u32));
            std::ifstream file(path, std::ios::binary);
            u32 words[2]{};
            file.read(reinterpret_cast<char*>(words), sizeof(words));
            assert(file && words[0] == i && words[1] == i + 1);
            assert(!std::filesystem::exists(path.string() + ".tmp"));
        }
    };

    {
        std::jthread worker{Storage::ProcessIO};
        std::vector<std::thread> producers;
        for (unsigned t = 0; t < 4; ++t) {
            producers.emplace_back([&, t] {
                for (unsigned i = 0; i < 100; ++i) {
                    save(t * 100 + i);
                }
            });
        }
        for (auto& producer : producers) {
            producer.join();
        }
    } // Request stop immediately; every accepted write must finish before the join returns.
    verify(400);

    // Even a stop requested before the worker starts must drain its accepted backlog.
    for (unsigned i = 400; i < 500; ++i) {
        save(i);
    }
    std::stop_source stopped;
    stopped.request_stop();
    std::thread worker{[&] { Storage::ProcessIO(stopped.get_token()); }};
    worker.join();
    verify(500);

    // Existing cache words still load; a miss must discard the caller's previous contents.
    std::vector<u32> words{99};
    auto path = root / "1";
    Storage::LoadVector(Storage::BlobType::ShaderBinary, path, words);
    assert((words == std::vector<u32>{1, 2}));
    path = root / "absent";
    Storage::LoadVector(Storage::BlobType::ShaderBinary, path, words);
    assert(words.empty());

    // Reject both an incomplete first word and a complete word followed by stray bytes.
    for (const size_t bytes : {3, 5}) {
        path = root / "bad.spv";
        {
            std::ofstream file(path, std::ios::binary);
            file << std::string(bytes, 'x');
            assert(file);
        }
        words = {99};
        Storage::LoadVector(Storage::BlobType::ShaderBinary, path, words);
        assert(words.empty());
        std::vector<u8> raw{99};
        Storage::LoadVector(Storage::BlobType::ShaderBinary, path, raw);
        assert(raw == std::vector<u8>(bytes, 'x'));
    }
    path = root / "empty.spv";
    std::ofstream{path, std::ios::binary}.close();
    words = {99};
    Storage::LoadVector(Storage::BlobType::ShaderBinary, path, words);
    assert(words.empty());

    // A cache root replaced by a file disables caching without throwing, then permits a retry.
    Core::Emulator::FillElfInfo(SpartanGpuConfig{.serial = "TEST00000"});
    using namespace Common::FS;
    const auto original_cache = GetUserPath(PathType::CacheDir);
    const auto cache_root = root / "cache";
    std::filesystem::create_directory(cache_root);
    SetUserPath(PathType::CacheDir, cache_root);
    assert(std::filesystem::remove(cache_root));
    std::ofstream{cache_root} << "blocked";
    auto& cache = Storage::DataBase::Instance();
    cache.Open();
    assert(!cache.IsOpened());
    assert(!cache.Save(Storage::BlobType::ShaderBinary, "blocked", std::vector<u32>{1}));
    assert(std::filesystem::is_regular_file(cache_root));
    assert(std::filesystem::remove(cache_root));
    std::filesystem::create_directory(cache_root);
    cache.Open();
    assert(cache.IsOpened());
    assert(std::filesystem::is_directory(cache_root / "TEST00000"));
    // Flush waits while a write is queued or running and returns once the worker is idle.
    std::promise<void> release;
    std::atomic<bool> ran{false};
    {
        std::scoped_lock lock{m_request};
        req_queue.emplace([&ran, gate = release.get_future().share()] {
            gate.wait();
            ran = true;
        });
    }
    request_cv.notify_one();
    assert(!cache.Flush(std::chrono::milliseconds(50)));
    release.set_value();
    assert(cache.Flush(std::chrono::seconds(5)) && ran);
    // A saved blob is on disk when Flush returns; a closed cache has nothing to flush.
    assert(cache.Save(Storage::BlobType::ShaderBinary, "flushed", std::vector<u32>{7}));
    assert(cache.Flush(std::chrono::seconds(5)));
    assert(std::filesystem::file_size(cache_root / "TEST00000" / "flushed.spv") == sizeof(u32));
    cache.Close();
    assert(cache.Flush(std::chrono::seconds(5)));
    SetUserPath(PathType::CacheDir, original_cache);

    std::filesystem::remove_all(root); // Only the directory this test creates above.
    std::puts("Cache storage: concurrent writes, stop drain, flush and load hardening PASS");
}
