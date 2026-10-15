// SPDX-License-Identifier: GPL-2.0-or-later
// Shader::Profile identity of the vertex input mode (#35): the disk pipeline cache compares the
// profile blob, so the dynamic and static vertex input paths must never compare equal. No game,
// Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "shader_recompiler/profile.h"

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    const Shader::Profile dynamic{.vertex_input = 1};
    const Shader::Profile fixed{.vertex_input = 2};
    assert(!(dynamic == fixed));
    // A 2.1.8 cache blob has no vertex input field, so it never matches 1 or 2.
    assert(Shader::Profile{}.vertex_input == 0);
    assert(!(Shader::Profile{} == dynamic) && !(Shader::Profile{} == fixed));
    std::puts("shader-profile-test: ok");
    return EXIT_SUCCESS;
}
