// The depth of field switch (gpu/shim/spartanport_depth_of_field.h): no GPU or game dump needed.
#include <array>
#include <cassert>
#include <cstdio>
#include "spartanport_depth_of_field.h"

int main() {
    namespace D = SpartanDepthOfField;

    // Which buffer: the composite's first stage buffer, 80 bytes.
    assert(D::IsComposite(D::Composite, 0, 80));
    assert(!D::IsComposite(D::Composite, 1, 80));
    assert(!D::IsComposite(D::Composite, 0, 64));
    assert(!D::IsComposite(0x1234, 0, 80));

    // The constants of a frame capture of the game (book camera scene).
    const std::array<float, D::ConstantWords> captured = {
        0.00769231f, -0.711538f, 0.5f,         0.0f,         -3.0303f,    -3.0303f,    -2.94118f,
        2.94118f,    1.0f,       2.0f,         2.94118f,     -1.94118f,   0.000260417f, -0.00138889f,
        -0.00078125f, -0.000462963f, -0.000260417f, 0.00138889f, 0.00078125f, 0.000462963f};
    static_assert(sizeof(captured) == D::ConstantBytes);

    auto changed = captured;
    assert(D::Disable(changed.data()));
    // The distance's scale and bias and the four ramp slopes are zero, nothing else moved.
    for (size_t i = 0; i < changed.size(); ++i) {
        const bool zeroed = i < 2 || (i >= 4 && i < 8);
        assert(changed[i] == (zeroed ? 0.0f : captured[i]));
    }
    // Already changed: no longer the game's shape, left alone.
    auto again = changed;
    assert(!D::Disable(again.data()));
    assert(again == changed);

    // Anything of another shape is not touched.
    std::array<float, D::ConstantWords> other{};
    assert(!D::Disable(other.data()));
    other = captured;
    other[4] = 3.0f;  // a ramp that rises
    auto before = other;
    assert(!D::Disable(other.data()));
    assert(other == before);
    other = captured;
    other[8] = 0.0f;  // the first ramp's offset
    before = other;
    assert(!D::Disable(other.data()));
    assert(other == before);

    std::puts("Depth of field tests passed");
}
