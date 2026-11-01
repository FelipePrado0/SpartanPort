// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include "gpu/shadps4/video_core/renderer_vulkan/ui_composition.h"

int main() {
    using namespace Vulkan::UiComposition;
    // Title/loading menus have no depth/camera. UI must still go to the native target.
    assert(Choose(true, true, false, true) == Background::Copy);
    assert(Choose(true, true, false, false) == Background::Copy);
    // In-game HUD: first reconstruct the scene, then draw the native UI over it.
    assert(Choose(true, true, true, true) == Background::Temporal);
    // Runtime FSR disable/failure must not change UI resolution.
    assert(Choose(true, true, true, false) == Background::Copy);
    assert(Choose(true, false, true, true) == Background::None);
    assert(Choose(false, true, true, true) == Background::None);
    assert(NativeViewport(1920, -1080));
    assert(!NativeViewport(960, -540));
    assert(MovieShader(0x34e8a281) && MovieShader(0x24042a9b));
    assert(!MovieShader(0x0b0acf50)); // fullscreen post/tonemap also has a native viewport
    assert(!MovieShader(0x6c62a79f)); // shadow geometry
    // Only the verified opaque display copy may land in the discard target.
    constexpr uint64_t copy_shader = 0x6d2dc8a47f6d9c23ull;
    assert(PlanDisplayDraw(copy_shader, true, true, false, false).discard);
    assert(!PlanDisplayDraw(0x5678, false, false, false, true).discard); // dark backdrop
    assert(!PlanDisplayDraw(copy_shader, true, false, false, true).discard); // alpha overlay
    assert(!PlanDisplayDraw(0x5678, true, true, false, true).discard); // screen filter/water
    assert(!PlanDisplayDraw(copy_shader, false, true, false, true).discard); // other texture
    assert(!PlanDisplayDraw(copy_shader, true, true, true, true).discard); // depth-tested draw
    assert(!PlanDisplayDraw(0, true, true, false, false).discard);
    // The recorded display pass clears first, then copies the finished UI image.
    const auto clear = PlanDisplayDraw(0x2bde139bde4ef56e, false, true, false, false);
    assert(!clear.seed && !clear.discard);
    const auto menu_copy = PlanDisplayDraw(0x6d2dc8a47f6d9c23, true, true, false, false);
    assert(menu_copy.seed && menu_copy.discard);
    const auto repeated_copy = PlanDisplayDraw(0x6d2dc8a47f6d9c23, true, true, false, true);
    assert(!repeated_copy.seed && repeated_copy.discard);
    const auto backdrop = PlanDisplayDraw(0x2bde139bde4ef56e, false, false, false, true);
    assert(!backdrop.seed && !backdrop.discard);
    // Different corner colors expose the zoom/crop regression; a later backdrop must survive.
    const std::array<float, 4> corners{0.2f, 0.4f, 0.6f, 0.8f};
    std::array<float, 4> displayed{};
    bool seeded = false;
    const auto copy_frame = [&] {
        const auto plan = PlanDisplayDraw(copy_shader, true, true, false, seeded);
        if (plan.seed) { displayed = corners; seeded = true; }
        if (!plan.discard) displayed.fill(corners[0]); // render-size coordinates crop the host image
    };
    copy_frame();
    assert(displayed == corners);
    for (float& value : displayed) value *= 0.5f; // alpha black backdrop
    const auto dimmed = displayed;
    copy_frame();
    assert(displayed == dimmed); // a repeated copy must not re-seed over the backdrop
    const auto display = Scale(960, 540, 1920, 1080, false);
    assert(display[0] == 2 && display[1] == 2);
    const auto ui = Scale(960, 540, 1920, 1080, true);
    assert(ui[0] == 1 && ui[1] == 1); // don't double an already native UI viewport
    const auto larger = Scale(960, 540, 3840, 2160, true);
    assert(larger[0] == 2 && larger[1] == 2);
    std::puts("UI composition: PASS (menu without camera, HUD, FSR off, viewport scaling)");
}
