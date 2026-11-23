// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include "common/types.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

[[nodiscard]] std::vector<u32> EmitDiscardFragmentShader(std::array<Shader::OutputMap, 3> outputs);

/// spartanport: a fragment shader that does nothing, for pipelines the game runs without one
/// (SPARTAN_EMPTY_FS=1, vk_graphics_pipeline.cpp).
[[nodiscard]] std::vector<u32> EmitEmptyFragmentShader();

} // namespace Shader::Backend::SPIRV
