// SPDX-License-Identifier: GPL-2.0-or-later
// NumAuxTessInputs (emit_spirv_quad_rect.h): how many fragment inputs the auxiliary TCS/TES of a
// RectList / QuadList draw declare. The "Compiling graphics pipeline" line reports it for a
// pipeline without a fragment shader (#35). No game, Vulkan device or SPIR-V emitter required.
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"

using Shader::HwFragmentRuntimeInfo;
using Shader::Backend::SPIRV::NumAuxTessInputs;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    HwFragmentRuntimeInfo fs{};
    assert(NumAuxTessInputs(fs) == 0);
    // Interpolated inputs are declared and passed through.
    fs.num_inputs = 3;
    assert(NumAuxTessInputs(fs) == 3);
    // A default (constant) input is synthesized by the fragment stage: not declared.
    fs.inputs[1].is_default = true;
    assert(NumAuxTessInputs(fs) == 2);
    // A flat default input is a passthrough: declared.
    fs.inputs[2].is_default = true;
    fs.inputs[2].is_flat = true;
    assert(NumAuxTessInputs(fs) == 2);
    // Entries past num_inputs are left over from earlier draws: ignored.
    fs.inputs[5].param_index = 5;
    assert(NumAuxTessInputs(fs) == 2);
    fs.num_inputs = 0;
    assert(NumAuxTessInputs(fs) == 0);
    std::puts("aux-tess-inputs-test: ok");
    return EXIT_SUCCESS;
}
