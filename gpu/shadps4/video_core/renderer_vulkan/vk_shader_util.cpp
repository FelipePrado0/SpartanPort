// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <vector>

#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {

vk::ShaderModule CompileSPV(std::span<const u32> code, vk::Device device) {
    const vk::ShaderModuleCreateInfo shader_info = {
        .codeSize = code.size() * sizeof(u32),
        .pCode = code.data(),
    };

    auto [module_result, module] = device.createShaderModule(shader_info);
    ASSERT_MSG(module_result == vk::Result::eSuccess, "Failed to compile SPIR-V shader: {}",
               vk::to_string(module_result));
    return module;
}

bool SetSpecConstantDefault(std::span<u32> code, u32 spec_id, float value) {
    // SPIR-V: a 5-word header, then instructions whose first word is (word count << 16) | opcode.
    constexpr u32 OpTypeFloat = 22, OpSpecConstant = 50, OpDecorate = 71, DecorationSpecId = 1;
    constexpr size_t Header = 5;
    u32 target = 0;
    for (size_t i = Header; i < code.size() && target == 0;) {
        const u32 count = code[i] >> 16;
        if (count == 0 || i + count > code.size()) {
            return false;
        }
        if ((code[i] & 0xFFFF) == OpDecorate && count == 4 && code[i + 2] == DecorationSpecId &&
            code[i + 3] == spec_id) {
            target = code[i + 1];
        }
        i += count;
    }
    if (target == 0) {
        return false;
    }
    // Types come before the constants that use them, so the float type is seen first.
    std::vector<u32> float32_types;
    for (size_t i = Header; i < code.size();) {
        const u32 count = code[i] >> 16, opcode = code[i] & 0xFFFF;
        if (count == 0 || i + count > code.size()) {
            return false;
        }
        if (opcode == OpTypeFloat && count >= 3 && code[i + 2] == 32) {
            float32_types.push_back(code[i + 1]);
        }
        if (opcode == OpSpecConstant && count == 4 && code[i + 2] == target) {
            if (std::find(float32_types.begin(), float32_types.end(), code[i + 1]) ==
                float32_types.end()) {
                return false;
            }
            std::memcpy(&code[i + 3], &value, sizeof(value));
            return true;
        }
        i += count;
    }
    return false;
}

} // namespace Vulkan
