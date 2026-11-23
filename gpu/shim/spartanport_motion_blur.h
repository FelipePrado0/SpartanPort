// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the game's motion blur, found in a frame capture (F11) of God of War III Remastered
// (CUSA01623 01.02). The blur is a short chain of passes: a half resolution pass draws a velocity
// image (previous and current transforms of the geometry), one draw (BlurPass) blurs the half
// resolution scene colour along it, and the next draw (Composite) blends that result back over the
// full resolution scene colour. Skipping the last two leaves the scene colour untouched; the
// velocity pass then only draws an image nobody reads.
//
// Composite's pixel shader is a plain copy that the game also uses without blending, so it is
// only skipped as the blended draw that directly follows a skipped BlurPass.

#pragma once

#include <cstdint>

namespace SpartanMotionBlur {

/// Pixel shader hashes (as FrameCapture prints them).
inline constexpr std::uint64_t BlurPass = 0x107d87f9d56574cfull;
inline constexpr std::uint64_t Composite = 0xabd418f2b4587d5dull;

enum class Action { Draw, SkipBlur, SkipComposite };

class Rule {
public:
    /// What to do with the next draw: its pixel shader hash and whether colour target 0 blends.
    /// `skip`: the motion blur is switched off.
    Action Next(std::uint64_t ps_hash, bool blend_enabled, bool skip) {
        const bool pending = composite_pending_;
        composite_pending_ = false;
        if (pending && blend_enabled && ps_hash == Composite) {
            return Action::SkipComposite;
        }
        if (skip && ps_hash == BlurPass) {
            composite_pending_ = true;
            return Action::SkipBlur;
        }
        return Action::Draw;
    }

private:
    bool composite_pending_ = false;
};

} // namespace SpartanMotionBlur
