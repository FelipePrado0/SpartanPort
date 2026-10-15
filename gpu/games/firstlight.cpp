// SPDX-License-Identifier: GPL-2.0-or-later
// inFAMOUS First Light CUSA00575.
//
// Stage 1 deliberately has no guessed GPU hooks, guest address ranges or shader hashes.
// Add game-specific data only after a reproducible CUSA00575 trace proves it is necessary.
#include "game_profile.h"
#include "gow3_toggles.h"

namespace Game {
namespace {
constexpr const char* Serials[] = {"CUSA00575"};

constexpr std::uint64_t FirstLightBringupDisabled =
    Gow3Toggle::ThreadedRecording |
    Gow3Toggle::PreparedResources |
    Gow3Toggle::DrawPreparation |
    Gow3Toggle::DeferredStreamCopies |
    Gow3Toggle::ParallelCopies |
    Gow3Toggle::AsyncFences |
    Gow3Toggle::RecordPrefetch |
    Gow3Toggle::TextureBindHelper |
    Gow3Toggle::EarlyDrawInputs |
    Gow3Toggle::ConstantRing |
    Gow3Toggle::DrawPipeline |
    Gow3Toggle::PipelinedTasks |
    Gow3Toggle::PendingFenceWaits |
    Gow3Toggle::PipelinedDispatch |
    Gow3Toggle::RecorderFences |
    Gow3Toggle::PipelinedMemoryWrites |
    Gow3Toggle::MultiCopyShader |
    Gow3Toggle::RenderStateMemo |
    Gow3Toggle::TextureSetMemo |
    Gow3Toggle::PipelinedIndirectDraws |
    Gow3Toggle::SceneAttachmentsOnly |
    Gow3Toggle::SampleSceneProxies |
    Gow3Toggle::SceneHalfRes |
    Gow3Toggle::SceneMipBias;
}

extern const Profile FirstLightProfile = {
    .name = "inFAMOUS First Light",
    .serials = Serials,
    .install_hooks = nullptr,
    .dynamic_writers = {},
    .buffer_copy_shader = 0,
    .disabled_optimizations = FirstLightBringupDisabled,
};

} // namespace Game
