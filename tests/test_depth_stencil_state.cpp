// SPDX-License-Identifier: GPL-2.0-or-later
// Depth/stencil attachments that cannot affect a draw (vk_depth_stencil_state.h); adapted from
// cuesta4/shadPS4 (eltutz) tests/test_depth_stencil_state.cpp. No game or Vulkan device needed.
#include <cassert>
#include <cstdio>
#include <memory>
#include <bit>

#include "video_core/renderer_vulkan/vk_depth_stencil_state.h"
#include "video_core/renderer_vulkan/ui_composition.h"

using Vulkan::GetEffectiveDepthStencilState;

static std::unique_ptr<AmdGpu::Regs> DepthRegs() {
    auto regs = std::make_unique<AmdGpu::Regs>();
    regs->depth_buffer.z_read_base = 1;
    regs->depth_buffer.z_write_base = 1;
    regs->depth_buffer.z_info.format = AmdGpu::DepthBuffer::ZFormat::Z32Float;
    regs->depth_control.depth_enable = 1;
    regs->depth_control.depth_func = AmdGpu::CompareFunc::Always;
    return regs;
}

static void AddStencil(AmdGpu::Regs& regs) {
    regs.depth_buffer.stencil_read_base = 1;
    regs.depth_buffer.stencil_write_base = 1;
    regs.depth_buffer.stencil_info.format = AmdGpu::DepthBuffer::StencilFormat::Stencil8;
    regs.depth_control.stencil_enable = 1;
    regs.depth_control.stencil_ref_func = AmdGpu::CompareFunc::Always;
}

int main() {
    // Depth test that always passes and writes nothing: no attachment (it would crop the pass).
    {
        const auto state = GetEffectiveDepthStencilState(*DepthRegs());
        assert(!state.needs_attachment && !state.depth_test_enable && !state.depth_write_enable);
    }
    // Depth that can reject, write or bound fragments is kept.
    {
        auto regs = DepthRegs();
        regs->depth_control.depth_func = AmdGpu::CompareFunc::Less;
        assert(GetEffectiveDepthStencilState(*regs).needs_attachment);
        regs = DepthRegs();
        regs->depth_control.depth_write_enable = 1;
        const auto state = GetEffectiveDepthStencilState(*regs);
        assert(state.needs_attachment && state.depth_write_enable);
        regs = DepthRegs();
        regs->depth_control.depth_bounds_enable = 1;
        assert(GetEffectiveDepthStencilState(*regs).needs_attachment);
        regs = DepthRegs();
        regs->depth_control.disable_color_writes_on_depth_pass = 1;
        assert(GetEffectiveDepthStencilState(*regs).needs_attachment);
    }
    // Color written even where depth fails: the test cannot reject anything.
    {
        auto regs = DepthRegs();
        regs->depth_control.depth_func = AmdGpu::CompareFunc::Less;
        regs->depth_control.enable_color_writes_on_depth_fail = 1;
        assert(!GetEffectiveDepthStencilState(*regs).needs_attachment);
    }
    // Stencil: kept only when it can reject or write.
    {
        auto regs = DepthRegs();
        AddStencil(*regs);
        assert(!GetEffectiveDepthStencilState(*regs).needs_attachment);
        regs->depth_control.stencil_ref_func = AmdGpu::CompareFunc::Less;
        const auto state = GetEffectiveDepthStencilState(*regs);
        assert(state.needs_attachment && state.stencil_test_enable);
        regs = DepthRegs();
        AddStencil(*regs);
        regs->stencil_control.stencil_zpass_front = AmdGpu::StencilFunc::ReplaceTest;
        regs->stencil_ref_front.stencil_write_mask = 0xff;
        assert(GetEffectiveDepthStencilState(*regs).needs_attachment);
        regs->stencil_ref_front.stencil_write_mask = 0;
        assert(!GetEffectiveDepthStencilState(*regs).needs_attachment);
    }
    // HTILE maintenance needs the attachment only when it forces valid data.
    {
        auto regs = DepthRegs();
        regs->depth_render_control.resummarize_enable = 1;
        regs->depth_render_override.force_z_dirty = 1;
        assert(!GetEffectiveDepthStencilState(*regs).needs_attachment);
        regs->depth_render_override.force_z_valid = 1;
        assert(GetEffectiveDepthStencilState(*regs).needs_attachment);
    }
    // Captured Gorgon Eye backdrop: RT0 dims the scene; RT1 exports zero and preserves its target.
    {
        using Vulkan::UiComposition::ColorMask;
        using Factor = AmdGpu::BlendControl::BlendFactor;
        using Function = AmdGpu::BlendControl::BlendFunc;
        constexpr uint64_t backdrop = 0xbcb456ff6dd871a8ull;
        const auto captured = std::bit_cast<AmdGpu::BlendControl>(0x61000504u);
        assert(ColorMask(backdrop, 3, captured) == 1);
        assert(ColorMask(0x1234, 3, captured) == 3);
        assert(ColorMask(backdrop, 7, captured) == 7);
        auto blend = captured;
        blend.enable = 0;
        assert(ColorMask(backdrop, 3, blend) == 3);
        blend = captured;
        blend.color_dst_factor = Factor::Zero;
        assert(ColorMask(backdrop, 3, blend) == 3);
        blend = captured;
        blend.alpha_dst_factor = Factor::Zero;
        assert(ColorMask(backdrop, 3, blend) == 3);
        blend = captured;
        blend.color_func = Function::Min;
        assert(ColorMask(backdrop, 3, blend) == 3);
        blend = captured;
        blend.alpha_func = Function::Max;
        assert(ColorMask(backdrop, 3, blend) == 3);
        blend = captured;
        blend.separate_alpha_blend = 0;
        assert(ColorMask(backdrop, 3, blend) == 1);
    }
    std::puts("depth-stencil-state-test: OK");
    return 0;
}
