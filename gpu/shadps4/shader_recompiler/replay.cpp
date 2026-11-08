// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>

#include <xxhash.h>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/replay.h"
#include "shader_recompiler/specialization.h"
#include "video_core/amdgpu/spartanport_msaa.h"

namespace Shader {

namespace {

/// Serves the recorded user data to info for one translation, on this thread only.
class ReplayScope {
public:
    ReplayScope(const Info& info, const ReplayInputs& inputs, std::span<const u32> code) {
        Info::ud_snapshots[0] = {&info,
                                 inputs.user_data.data(),
                                 u32(inputs.user_data.size()),
                                 inputs.flat_ud.data(),
                                 u32(inputs.flat_ud.size()),
                                 VAddr(code.data())};
        Info::num_ud_snapshots = 1;
        Info::replaying = true;
    }
    ~ReplayScope() {
        Info::replaying = false;
        Info::num_ud_snapshots = 0;
    }
    ReplayScope(const ReplayScope&) = delete;
    ReplayScope& operator=(const ReplayScope&) = delete;
};

} // namespace

void ReplayInputs::Serialize(Serialization::Archive& ar) const {
    Serialization::Writer writer{ar};
    writer.Write(user_data);
    writer.Write(flat_ud);
    writer.Write(fetch_buffers);
    writer.Write(code_size);
    writer.Write(code_xxh3);
}

void ReplayInputs::Deserialize(Serialization::Archive& ar) {
    Serialization::Reader reader{ar};
    reader.Read(user_data);
    reader.Read(flat_ud);
    reader.Read(fetch_buffers);
    reader.Read(code_size);
    reader.Read(code_xxh3);
}

bool ReplayInputs::operator==(const ReplayInputs& other) const {
    return user_data == other.user_data && flat_ud == other.flat_ud &&
           code_size == other.code_size && code_xxh3 == other.code_xxh3 &&
           fetch_buffers.size() == other.fetch_buffers.size() &&
           (fetch_buffers.empty() ||
            std::memcmp(fetch_buffers.data(), other.fetch_buffers.data(),
                        fetch_buffers.size() * sizeof(AmdGpu::Buffer)) == 0);
}

ReplayInputs CaptureReplayInputs(const Info& info, std::span<const u32> user_data,
                                 std::span<const u32> code,
                                 const Gcn::FetchShaderSnapshot& fetch) {
    ReplayInputs inputs;
    inputs.user_data.assign(user_data.begin(), user_data.end());
    inputs.flat_ud = info.flattened_ud_buf;
    inputs.fetch_buffers = fetch.buffers;
    inputs.code_size = u32(code.size_bytes());
    inputs.code_xxh3 = XXH3_64bits(code.data(), code.size_bytes());
    return inputs;
}

bool LoadReplayProfile(std::span<const u8> blob, Profile& profile) {
    if (blob.size() != sizeof(profile) && blob.size() != sizeof(profile) + 1) {
        return false;
    }
    std::memcpy(&profile, blob.data(), sizeof(profile));
    AmdGpu::no_msaa = blob.size() > sizeof(profile) && blob[sizeof(profile)] != 0;
    return true;
}

std::vector<u32> ReplayTranslate(std::span<const u32> code, HwStage hw_stage, SwStage sw_stage,
                                 u64 pgm_hash, const ReplayInputs& inputs,
                                 const StageSpecialization& spec, const Profile& profile,
                                 Pools& pools, Info& info) {
    const size_t attributes = spec.fetch_shader_data ? spec.fetch_shader_data->attributes.size() : 0;
    if (attributes != inputs.fetch_buffers.size() ||
        inputs.user_data.size() != ShaderParams::NumShaderUserData) {
        return {};
    }
    info = Info(hw_stage, sw_stage,
                ShaderParams{.user_data = std::span<const u32, ShaderParams::NumShaderUserData>{
                                 inputs.user_data.data(), ShaderParams::NumShaderUserData},
                             .code = code,
                             .hash = pgm_hash});
    ReplayScope scope{info, inputs, code};
    Gcn::FetchShaderSnapshot fetch;
    fetch.info = &info;
    fetch.fetch = spec.fetch_shader_data;
    fetch.buffers = inputs.fetch_buffers;
    Gcn::ScopedFetchShaderSnapshot scoped_fetch{fetch};

    auto runtime = spec.runtime_info;
    auto binding = spec.start;
    const auto program = TranslateProgram(code, pools, info, runtime, profile);
    return Backend::SPIRV::EmitSPIRV(profile, runtime, program, binding);
}

} // namespace Shader
