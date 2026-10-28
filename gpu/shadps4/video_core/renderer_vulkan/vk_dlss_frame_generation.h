// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_vulkan/vk_common.h"

#include "../../../streamline_bridge/spartanport_streamline.h"
#include "common/types.h"
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>

namespace Vulkan {
class DlssFrameGeneration {
public:
    static DlssFrameGeneration& Get();
    bool Requested() const;
    bool Prepare(vk::PhysicalDevice physical);
    PFN_vkGetInstanceProcAddr Resolver() const;
    void Initialize(vk::PhysicalDevice physical, vk::Queue graphics, vk::Queue present);
    bool Ready() const { return ready.load(); }
    void* BeginFrame();
    void StartSimulation(); // Guest submit-done boundary, under the GNM submit lock.
    void* SubmissionToken();
    void SetRenderToken(void* token) { render_token = token; } // Draw recording thread only.
    void* RenderToken() const { return render_token; }
    void Marker(void* token, u32 marker);
    bool Inputs(void* token, vk::CommandBuffer cmd, const SpartanSlCamera& camera,
                const SpartanSlImage& depth, const SpartanSlImage& motion);
    bool Color(void* token, vk::CommandBuffer cmd, const SpartanSlImage& hudless,
               const SpartanSlImage& alpha);
    bool Enable(bool value, vk::Extent2D extent);
    void Presented(bool game_frame);
    void Pause();
    void Shutdown();
    void Fail(const char* reason);

private:
    const SpartanSlApi* api{};
    std::atomic<bool> ready{false};
    bool hooked = false;
    std::atomic<bool> last_enabled{false};
    std::atomic<u32> next_id{0};
    void* simulation_token{};
    void* render_token{};
    bool submission_started{};
    std::array<u8, VK_UUID_SIZE> adapter_uuid{};
    SpartanSlState state{};
    std::chrono::steady_clock::time_point report_start = std::chrono::steady_clock::now();
    u32 real_count{}, presented_count{};
};
} // namespace Vulkan
