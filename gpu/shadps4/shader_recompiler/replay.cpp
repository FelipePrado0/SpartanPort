// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>

#include <xxhash.h>

#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/replay.h"

namespace Shader {

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

} // namespace Shader
