// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

/**
 * @brief Creates a vulkan shader module from SPIR-V bytecode.
 * @param code The SPIR-V bytecode data.
 * @param device The vulkan device handle
 */
vk::ShaderModule CompileSPV(std::span<const u32> code, vk::Device device);

/**
 * @brief spartanport: sets the default value of a 32-bit float specialization constant
 * (layout(constant_id = spec_id) const float) in SPIR-V code, for a pipeline that is built without
 * VkSpecializationInfo and so uses the default.
 * @return false, with the code unchanged, when the code has no such constant.
 */
bool SetSpecConstantDefault(std::span<u32> code, u32 spec_id, float value);

} // namespace Vulkan
