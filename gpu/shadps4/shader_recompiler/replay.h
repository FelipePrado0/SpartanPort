// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <span>
#include <vector>

#include "common/serdes.h"
#include "common/types.h"
#include "video_core/amdgpu/resource.h"

namespace Shader {

struct Info;
struct Pools;
struct Profile;
struct StageSpecialization;
enum class HwStage : u32;
enum class SwStage : u32;
namespace Gcn {
struct FetchShaderSnapshot;
}

/// spartanport: what one translation read from guest memory, saved at translation time so that the
/// permutation can be translated again without the game running (shader precompile). The Info of a
/// program is refreshed by every later draw, so it does not hold these values.
struct ReplayInputs {
    std::vector<u32> user_data;
    std::vector<u32> flat_ud;
    std::vector<AmdGpu::Buffer> fetch_buffers;
    u32 code_size{};
    u64 code_xxh3{};

    void Serialize(Serialization::Archive& ar) const;
    void Deserialize(Serialization::Archive& ar);

    bool operator==(const ReplayInputs& other) const;
};

/// Reads the inputs of a finished translation. flat_ud is the buffer left by the translation in info.
ReplayInputs CaptureReplayInputs(const Info& info, std::span<const u32> user_data,
                                 std::span<const u32> code,
                                 const Gcn::FetchShaderSnapshot& fetch);

/// Translates a recorded permutation without guest memory. info receives the result of the
/// translation. Empty when the recorded inputs do not fit the specialization. Thread safe.
std::vector<u32> ReplayTranslate(std::span<const u32> code, HwStage hw_stage, SwStage sw_stage,
                                 u64 pgm_hash, const ReplayInputs& inputs,
                                 const StageSpecialization& spec, const Profile& profile,
                                 Pools& pools, Info& info);

} // namespace Shader
