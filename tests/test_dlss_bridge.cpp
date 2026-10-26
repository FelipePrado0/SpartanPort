// SPDX-License-Identifier: MIT
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include "../gpu/dlss_bridge/spartanport_dlss_bridge.h"
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>

static bool expect_mask;
static int calls;

static NVSDK_NGX_Result CaptureEvaluate(VkCommandBuffer, NVSDK_NGX_Handle*, NVSDK_NGX_Parameter*,
                                       NVSDK_NGX_VK_DLSS_Eval_Params* parameters) {
    ++calls;
    assert(bool(parameters->pInBiasCurrentColorMask) == expect_mask);
    if (expect_mask) {
        const auto& mask = *parameters->pInBiasCurrentColorMask;
        const auto& image = mask.Resource.ImageViewInfo;
        assert(!mask.ReadWrite && image.Format == VK_FORMAT_R8_UNORM);
        assert(image.Width == 1280 && image.Height == 720);
        assert(image.Image == reinterpret_cast<VkImage>(1));
        assert(image.ImageView == reinterpret_cast<VkImageView>(2));
        assert(image.SubresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
        assert(parameters->InBiasCurrentColorSubrectBase.X == 0);
        assert(parameters->InBiasCurrentColorSubrectBase.Y == 0);
    }
    assert(parameters->InJitterOffsetX == 0.25f && parameters->InJitterOffsetY == -0.5f);
    assert(parameters->InRenderSubrectDimensions.Width == 1280);
    assert(parameters->InRenderSubrectDimensions.Height == 720);
    return NVSDK_NGX_Result_Success;
}

// Intercept only the GPU submission, exercising the bridge's actual resource conversion.
#define NGX_VULKAN_EVALUATE_DLSS_EXT CaptureEvaluate
#include "../gpu/dlss_bridge/spartanport_dlss_bridge.cpp"
#undef NGX_VULKAN_EVALUATE_DLSS_EXT

int main() {
    static_assert(SPARTAN_DLSS_BRIDGE_ABI == 2);
    state.feature = reinterpret_cast<NVSDK_NGX_Handle*>(1);
    state.parameters = reinterpret_cast<NVSDK_NGX_Parameter*>(1);
    state.feature_desc.input_width = 1280;
    state.feature_desc.input_height = 720;
    SpartanDlssEvaluate frame{};
    frame.jitter_x = 0.25f;
    frame.jitter_y = -0.5f;
    assert(Evaluate(VK_NULL_HANDLE, &frame) == 1);
    expect_mask = true;
    frame.bias_mask = {reinterpret_cast<VkImage>(1), reinterpret_cast<VkImageView>(2),
                       {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_FORMAT_R8_UNORM, 1280, 720};
    assert(Evaluate(VK_NULL_HANDLE, &frame) == 1);
    expect_mask = false;
    frame.bias_mask = {};
    assert(Evaluate(VK_NULL_HANDLE, &frame) == 1);
    assert(calls == 3);
    assert(Evaluate(VK_NULL_HANDLE, nullptr) == 0);
    assert(calls == 3);
    std::puts("DLSS bridge: optional mask, resource conversion and jitter PASS");
}
