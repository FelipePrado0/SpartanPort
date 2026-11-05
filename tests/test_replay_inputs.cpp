// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdio>
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/replay.h"
#include "shader_recompiler/specialization.h"
#include "video_core/renderer_vulkan/vk_pipeline_serialization.h"

#undef NDEBUG
#include <cassert>

namespace Vulkan {
bool LoadShaderMeta(Serialization::Archive& ar, Shader::Info& info,
                    std::optional<Shader::Gcn::FetchShaderData>& fetch_shader_data,
                    Shader::StageSpecialization& spec, size_t& perm_idx);
}

static void RoundTrip() {
    Shader::ReplayInputs in;
    in.user_data = {1, 2, 3, 4};
    in.flat_ud = {1, 2, 3, 4, 5, 6};
    AmdGpu::Buffer sharp{};
    sharp.stride = 16;
    sharp.num_records = 7;
    in.fetch_buffers = {sharp, sharp};
    in.code_size = 64;
    in.code_xxh3 = 0x1122334455667788ull;

    Serialization::Archive ar;
    in.Serialize(ar);
    Serialization::Archive back{ar.TakeOff()};
    Shader::ReplayInputs out;
    out.Deserialize(back);
    assert(out == in);
    assert(back.IsEoS());

    out.fetch_buffers[1].stride = 32;
    assert(!(out == in));
}

// Empty lists are the normal case for shaders without a fetch shader.
static void RoundTripEmpty() {
    Shader::ReplayInputs in;
    Serialization::Archive ar;
    in.Serialize(ar);
    Serialization::Archive back{ar.TakeOff()};
    Shader::ReplayInputs out;
    out.Deserialize(back);
    assert(out == in);
}

static void OldMetaRefused() {
    Serialization::Archive ar;
    Serialization::Writer writer{ar};
    writer.Write(u32{7}); // the last version without ReplayInputs: nothing after it is read
    Serialization::Archive back{ar.TakeOff()};
    Shader::Info info;
    std::optional<Shader::Gcn::FetchShaderData> fetch;
    Shader::StageSpecialization spec;
    size_t perm_idx{};
    assert(!Vulkan::LoadShaderMeta(back, info, fetch, spec, perm_idx));
}

static void CaptureUsesTranslationBuffer() {
    const std::array<u32, 4> code{0xBEEB03FF, 0, 0, 0};
    const std::array<u32, 2> user_data{9, 8};
    Shader::Info info;
    info.flattened_ud_buf = {9, 8, 7};
    Shader::Gcn::FetchShaderSnapshot fetch;
    fetch.buffers.push_back(AmdGpu::Buffer{});
    const auto inputs = Shader::CaptureReplayInputs(info, user_data, code, fetch);
    assert(inputs.flat_ud == info.flattened_ud_buf);
    assert((inputs.user_data == std::vector<u32>{9, 8}));
    assert(inputs.fetch_buffers.size() == 1);
    assert(inputs.code_size == 16);
    assert(inputs.code_xxh3 != 0);
}

int main() {
    RoundTrip();
    RoundTripEmpty();
    OldMetaRefused();
    CaptureUsesTranslationBuffer();
    std::printf("PASS: replay inputs\n");
    return 0;
}
