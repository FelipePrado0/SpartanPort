// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the production IO worker without a window, guest process or Vulkan device.
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
// Include the implementation so optimized builds need not export its internal template writer.
#include "../gpu/shadps4/video_core/cache_storage.cpp"

int main(int argc, char** argv) {
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

    assert(argc == 2); // Supply a new, test-owned directory inside the workspace.
    const auto root = std::filesystem::absolute(argv[1]);
    assert(!std::filesystem::exists(root));
    std::filesystem::create_directories(root);
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

    std::filesystem::remove_all(root); // Only the directory this test creates above.
    std::puts("Cache storage: concurrent writes and stop drain PASS");
}
