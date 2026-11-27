// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>
#include <vector>
#include "common/types.h"

namespace Shader {
struct HwFragmentRuntimeInfo;
struct Info;
} // namespace Shader

namespace Shader::Backend::SPIRV {

enum class AuxShaderType : u32 {
    RectListTCS,
    QuadListTCS,
    PassthroughTES,
};

/// spartanport: components the vertex shader writes at each output location (0 = nothing). The
/// rect/quad TCS reads the vertex shader outputs, so it declares only these locations with the
/// same component count (VUID-RuntimeSpirv-OpEntryPoint-08743). Intel's shader compiler crashes
/// on the mismatched interface that AMD and NVIDIA tolerate (#1408).
using VsOutputs = std::array<u8, 32>;

/// Every location written as a full vec4: the interface the TCS had before it mirrored the VS.
[[nodiscard]] constexpr VsOutputs AllVec4Outputs() {
    VsOutputs outputs{};
    outputs.fill(4);
    return outputs;
}

[[nodiscard]] std::vector<u32> EmitAuxilaryTessShader(
    AuxShaderType type, const HwFragmentRuntimeInfo& fs_info,
    const VsOutputs& vs_outputs = AllVec4Outputs());

/// spartanport: the fragment inputs an auxiliary TCS was emitted for, read back from its SPIR-V
/// (the locations of its outputs). A pipeline loaded from the cache or the shipped shader list has
/// the TCS but not the fragment runtime info, so this lets it be emitted again. False when `tcs`
/// is not an auxiliary TCS.
bool AuxTessInputsFromSpirv(std::span<const u32> tcs, HwFragmentRuntimeInfo& fs_info);

/// spartanport: sets the parameter (and emulated clip distance) stores of a vertex shader Info
/// from its SPIR-V. Info::stores is not saved with the shader meta, so a vertex shader loaded from
/// the cache would otherwise look like it writes nothing. Does nothing for other stages.
void RestoreVertexOutputStores(Info& info, std::span<const u32> spv);

} // namespace Shader::Backend::SPIRV
