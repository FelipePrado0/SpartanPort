// SPDX-License-Identifier: MIT
#undef NDEBUG
#include "../gpu/streamline_bridge/spartanport_streamline.cpp"
#include <cassert>
#include <cstring>

namespace {
struct Token final : sl::FrameToken {
    operator uint32_t() const override { return 7; }
} token;
int tags_seen = 0, options_seen = 0;
bool moved_camera = false;
sl::Result support_result = sl::Result::eOk;
sl::Result Support(sl::Feature, const sl::AdapterInfo& adapter) {
    assert(adapter.vkPhysicalDevice == (VkPhysicalDevice)1);
    assert(adapter.deviceLUID && adapter.deviceLUIDSizeInBytes == VK_LUID_SIZE);
    assert(adapter.deviceLUID[0] == 42);
    return support_result;
}
sl::Result Sleep(const sl::FrameToken& frame) {
    assert(uint32_t(frame) == 7);
    return sl::Result::eOk;
}
sl::Result MarkerValue(sl::PCLMarker marker, const sl::FrameToken& frame) {
    assert(marker == sl::PCLMarker::ePresentStart && uint32_t(frame) == 7);
    return sl::Result::eOk;
}
sl::Result State(const sl::ViewportHandle&, sl::DLSSGState& state,
                 const sl::DLSSGOptions* options) {
    assert(options == nullptr);
    state.numFramesActuallyPresented = 3;
    state.minWidthOrHeight = 512;
    return sl::Result::eOk;
}
sl::Result Token(sl::FrameToken*& out, const uint32_t* id) {
    assert(id && *id == 7);
    out = &token;
    return sl::Result::eOk;
}
sl::Result Constants(const sl::Constants& c, const sl::FrameToken& frame,
                     const sl::ViewportHandle&) {
    assert(&frame == &token);
    assert(c.jitterOffset.x == 0.25f && c.jitterOffset.y == -0.25f);
    assert(c.mvecScale.x == 1.0f / 128 && c.mvecScale.y == 1.0f / 64);
    assert(c.cameraMotionIncluded == sl::eTrue && c.depthInverted == sl::eFalse);
    // A stationary camera must reproject every clip-space point to itself.
    const float* values = &c.clipToPrevClip[0].x;
    if (!moved_camera) {
        for (int i = 0; i < 16; ++i)
            assert(std::abs(values[i] - float(i % 5 == 0)) < 0.0001f);
    }
    // A point one meter in front of this camera moves one clip unit under a +1m prior-view X translation.
    const float point[]{0, 0, 0.9009f, 1};
    float previous_x = 0;
    for (int i = 0; i < 4; ++i) previous_x += point[i] * values[i * 4];
    assert(std::abs(previous_x - float(moved_camera)) < 0.0001f);
    return sl::Result::eOk;
}
sl::Result Tags(const sl::FrameToken& frame, const sl::ViewportHandle& viewport,
                const sl::ResourceTag* tags, uint32_t count, sl::CommandBuffer* cmd) {
    assert(&frame == &token && uint32_t(viewport) == 0 && cmd);
    assert(count == 2);
    for (uint32_t i = 0; i < count; ++i) {
        assert(tags[i].lifecycle == sl::eValidUntilPresent);
        assert(tags[i].resource->width == 128 && tags[i].resource->height == 64);
    }
    ++tags_seen;
    return sl::Result::eOk;
}
sl::Result Options(const sl::ViewportHandle&, const sl::DLSSGOptions& options) {
    assert(options.numFramesToGenerate == 1);
    assert(options.queueParallelismMode ==
           sl::DLSSGQueueParallelismMode::eBlockPresentingClientQueue);
    ++options_seen;
    return sl::Result::eOk;
}
} // namespace
int main() {
    const auto fixture =
        std::filesystem::temp_directory_path() / L"spartanport-streamline-pin-test.bin";
    {
        std::ofstream file(fixture, std::ios::binary);
        file << "abc";
    }
    constexpr auto expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    assert(Pinned(fixture, expected));
    {
        std::ofstream file(fixture, std::ios::binary);
        file << "abd";
    }
    assert(!Pinned(fixture, expected));
    std::filesystem::remove(fixture);
    const auto* api = SpartanStreamlineGetApi();
    assert(api->abi == SPARTAN_STREAMLINE_ABI && api->size == sizeof(*api));
    assert(!api->Configure(L"relative"));
    assert(!api->SetEnabled(1));
    assert(!api->Inputs(nullptr, {}, nullptr, nullptr, nullptr));
    ready = true;
    set_options = Options;
    set_tags = Tags;
    new_token = Token;
    void* ticket = reinterpret_cast<void*>(uintptr_t(8));
    reflex_sleep = Sleep;
    set_marker = MarkerValue;
    get_state = State;
    assert(api->BeginFrame(7) == ticket);
    assert(api->Marker(ticket, uint32_t(sl::PCLMarker::ePresentStart)));
    assert(!api->Marker(ticket, UINT32_MAX));
    SpartanSlState reported{};
    assert(api->GetState(&reported));
    assert(reported.presented == 3 && reported.minimum_dimension == 512);
    initialized = true;
    is_supported = Support;
    uint8_t luid[VK_LUID_SIZE]{42};
    assert(!api->Supported((VkPhysicalDevice)1, nullptr, 0));
    support_result = sl::Result::eErrorOSDisabledHWS;
    assert(!api->Supported((VkPhysicalDevice)1, luid, sizeof(luid)));
    assert(std::strstr(api->Problem(), "scheduling"));
    support_result = sl::Result::eOk;
    assert(api->Supported((VkPhysicalDevice)1, luid, sizeof(luid)));
    assert(api->SetEnabled(1));
    assert(api->SetEnabled(1));
    assert(options_seen == 1);
    assert(api->SetEnabled(0));
    assert(options_seen == 2);
    SpartanSlImage image{(VkImage)1,
                         (VkImageView)2,
                         VK_FORMAT_R8_UNORM,
                         VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                         128,
                         64};
    assert(api->Color(ticket, (VkCommandBuffer)3, &image, &image));
    assert(tags_seen == 1);
    image.width = 0;
    assert(!api->Color(ticket, (VkCommandBuffer)3, &image, &image));
    assert(tags_seen == 1);
    image.width = 128;
    set_constants = Constants;
    SpartanSlCamera camera{};
    for (int i = 0; i < 3; ++i) {
        camera.view[i * 5] = camera.inverse_view[i * 5] = camera.previous_view[i * 5] = 1;
    }
    camera.projection[0] = camera.previous_projection[0] = 1;
    camera.projection[1] = camera.previous_projection[1] = 1;
    camera.projection[2] = camera.previous_projection[2] = 1.001f;
    camera.projection[3] = camera.previous_projection[3] = -0.1001f;
    camera.jitter[0] = 0.25f;
    camera.jitter[1] = -0.25f;
    camera.near_plane = 0.1f;
    camera.far_plane = 100;
    camera.fov = 1.0f;
    assert(api->Inputs(ticket, (VkCommandBuffer)3, &camera, &image, &image));
    moved_camera = true;
    camera.previous_view[3] = 1;
    assert(api->Inputs(ticket, (VkCommandBuffer)3, &camera, &image, &image));
    camera.projection[0] = 0;
    assert(!api->Inputs(ticket, (VkCommandBuffer)3, &camera, &image, &image));
    return 0;
}
