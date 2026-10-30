// Background pipeline cache warm-up helpers (vk_warmup_inbox.h).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include "video_core/renderer_vulkan/vk_warmup_inbox.h"

using namespace Vulkan;

struct TestPipeline {
    int id;
    bool valid = true;

    bool IsValid() const {
        return valid;
    }
};

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    assert(UseOnDemandPipelines(true, true, false));
    assert(!UseOnDemandPipelines(false, true, false));
    assert(!UseOnDemandPipelines(true, false, false));
    assert(!UseOnDemandPipelines(true, true, true));
    assert(WarmupThreadCount(1) == 1 && WarmupThreadCount(2) == 1 && WarmupThreadCount(4) == 1);
    assert(WarmupThreadCount(6) == 4 && WarmupThreadCount(8) == 6 && WarmupThreadCount(32) == 6);

    // Batches: never more than asked, oldest first, nothing lost across threads.
    WarmupInbox<int> inbox;
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&inbox, t] {
            for (int i = 0; i < 1000; ++i) {
                inbox.Push(t * 1000 + i);
            }
        });
    }
    for (auto& p : producers) {
        p.join();
    }
    std::vector<int> seen(4000, 0);
    size_t batches = 0;
    while (!inbox.Empty()) {
        const auto batch = inbox.Take(64);
        assert(!batch.empty() && batch.size() <= 64);
        for (int v : batch) {
            ++seen[v];
        }
        ++batches;
    }
    for (int count : seen) {
        assert(count == 1);
    }
    assert(batches == (4000 + 63) / 64);
    inbox.Push(1);
    inbox.Push(2);
    assert((inbox.Take(1) == std::vector<int>{1}) && (inbox.Take(8) == std::vector<int>{2}));
    assert(inbox.Take(8).empty());

    // Conflict: the first pipeline for a key wins, the later one is left to the caller.
    std::map<int, std::unique_ptr<TestPipeline>> pipelines;
    auto runtime = std::make_unique<TestPipeline>(1);
    auto warm = std::make_unique<TestPipeline>(2);
    assert(PublishFirst(pipelines, 7, std::move(runtime)));
    assert(!PublishFirst(pipelines, 7, std::move(warm)));
    assert(pipelines[7]->id == 1 && warm && warm->id == 2);
    assert(PublishFirst(pipelines, 8, std::make_unique<TestPipeline>(3)) && pipelines[8]->id == 3);

    // A refused build cannot occupy a slot or prevent a later valid pipeline from winning.
    auto refused = std::make_unique<TestPipeline>(4, false);
    assert(!PublishFirst(pipelines, 9, std::move(refused)));
    assert(!pipelines.contains(9) && refused && !refused->IsValid());
    assert(!PublishFirst(pipelines, 9, std::unique_ptr<TestPipeline>{}));
    assert(!pipelines.contains(9));
    assert(PublishFirst(pipelines, 9, std::make_unique<TestPipeline>(5)));
    assert(pipelines[9]->id == 5);
    assert(!PublishFirst(pipelines, 9, std::move(refused)));
    assert(pipelines[9]->id == 5 && refused);

    // A runtime refusal marker suppresses future warm-up publication and per-draw retries.
    pipelines.try_emplace(10);
    auto retry = std::make_unique<TestPipeline>(6);
    assert(!PublishFirst(pipelines, 10, std::move(retry)));
    assert(!pipelines[10] && retry && retry->id == 6);

    std::puts("PASS: warm-up inbox");
}
