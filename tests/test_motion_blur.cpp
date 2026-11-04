// The motion blur skip rule (gpu/shim/spartanport_motion_blur.h): no GPU or game dump needed.
#include <cassert>
#include <cstdio>
#include "spartanport_motion_blur.h"

int main() {
    using SpartanMotionBlur::Action;
    using SpartanMotionBlur::BlurPass;
    using SpartanMotionBlur::Composite;
    constexpr std::uint64_t Other = 0x1234;

    {
        // On: nothing is skipped, whatever the shaders are.
        SpartanMotionBlur::Rule rule;
        assert(rule.Next(Other, false, false) == Action::Draw);
        assert(rule.Next(BlurPass, false, false) == Action::Draw);
        assert(rule.Next(Composite, true, false) == Action::Draw);
        assert(rule.Next(Composite, false, false) == Action::Draw);
    }
    {
        // Off: the blur draw and the blended copy right after it go, everything else stays.
        SpartanMotionBlur::Rule rule;
        assert(rule.Next(Other, false, true) == Action::Draw);
        assert(rule.Next(Composite, false, true) == Action::Draw);  // the copy without blending
        assert(rule.Next(Composite, true, true) == Action::Draw);   // blended, but no blur before it
        assert(rule.Next(BlurPass, false, true) == Action::SkipBlur);
        assert(rule.Next(Composite, true, true) == Action::SkipComposite);
        assert(rule.Next(Composite, true, true) == Action::Draw);   // only the one after the blur
        assert(rule.Next(BlurPass, false, true) == Action::SkipBlur);
        assert(rule.Next(BlurPass, false, true) == Action::SkipBlur);  // a second blur draw
        assert(rule.Next(Composite, true, true) == Action::SkipComposite);
    }
    {
        // A draw between the two ends the wait: the copy after it is drawn.
        SpartanMotionBlur::Rule rule;
        assert(rule.Next(BlurPass, false, true) == Action::SkipBlur);
        assert(rule.Next(Other, true, true) == Action::Draw);
        assert(rule.Next(Composite, true, true) == Action::Draw);
        // The copy without blending directly after the blur is not the composite either.
        assert(rule.Next(BlurPass, false, true) == Action::SkipBlur);
        assert(rule.Next(Composite, false, true) == Action::Draw);
        assert(rule.Next(Composite, true, true) == Action::Draw);
    }
    {
        // Switched on between the two draws: the copy of the skipped blur is still skipped (it
        // would blend a picture nobody drew), and the next blur is drawn.
        SpartanMotionBlur::Rule rule;
        assert(rule.Next(BlurPass, false, true) == Action::SkipBlur);
        assert(rule.Next(Composite, true, false) == Action::SkipComposite);
        assert(rule.Next(BlurPass, false, false) == Action::Draw);
        assert(rule.Next(Composite, true, false) == Action::Draw);
    }
    std::puts("Motion blur rule tests passed");
}
