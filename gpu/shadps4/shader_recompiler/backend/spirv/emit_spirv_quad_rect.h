// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <ranges>
#include <vector>
#include "common/types.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

enum class AuxShaderType : u32 {
    RectListTCS,
    QuadListTCS,
    PassthroughTES,
};

[[nodiscard]] std::vector<u32> EmitAuxilaryTessShader(AuxShaderType type,
                                                      const HwFragmentRuntimeInfo& fs_info);

/// gow3: how many fragment inputs EmitAuxilaryTessShader declares and passes through for
/// `fs_info`, as DefineInputs / DefineOutputs do: each of the first num_inputs but the default
/// ones. The compile log reports it for a pipeline without a fragment shader (#35).
[[nodiscard]] inline u32 NumAuxTessInputs(const HwFragmentRuntimeInfo& fs_info) {
    return static_cast<u32>(std::ranges::count_if(
        fs_info.inputs | std::views::take(fs_info.num_inputs),
        [](const HwFragmentRuntimeInfo::PsInput& input) { return !input.IsDefault(); }));
}

} // namespace Shader::Backend::SPIRV
