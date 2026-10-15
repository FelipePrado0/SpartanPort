// SPDX-License-Identifier: GPL-2.0-or-later
// GOW3_VK_DISABLE_EXT parsing for optional device extensions (vk_extension_policy.h). No game,
// Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "video_core/renderer_vulkan/vk_extension_policy.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // Unset or empty variable: nothing is disabled.
    assert(ParseExtensionPolicy(nullptr).Empty());
    assert(!ParseExtensionPolicy(nullptr).IsDisabled("VK_KHR_maintenance8"));
    assert(ParseExtensionPolicy("").Empty());
    assert(!ParseExtensionPolicy("").IsDisabled("VK_KHR_maintenance8"));

    // Blank items are skipped, so whitespace and stray commas leave the policy empty.
    assert(ParseExtensionPolicy("   ").Empty());
    assert(ParseExtensionPolicy(",, ,").Empty());

    // One name disables exactly that extension.
    {
        const auto policy = ParseExtensionPolicy("VK_KHR_maintenance8");
        assert(!policy.Empty());
        assert(policy.IsDisabled("VK_KHR_maintenance8"));
        assert(!policy.IsDisabled("VK_NV_framebuffer_mixed_samples"));
    }

    // A comma-separated list disables every name in it.
    {
        const auto policy =
            ParseExtensionPolicy("VK_KHR_maintenance8,VK_NV_framebuffer_mixed_samples");
        assert(policy.IsDisabled("VK_KHR_maintenance8"));
        assert(policy.IsDisabled("VK_NV_framebuffer_mixed_samples"));
    }

    // Spaces and tabs around each name are ignored.
    {
        const auto policy =
            ParseExtensionPolicy(" VK_KHR_maintenance8 ,\tVK_NV_framebuffer_mixed_samples\t");
        assert(policy.IsDisabled("VK_KHR_maintenance8"));
        assert(policy.IsDisabled("VK_NV_framebuffer_mixed_samples"));
    }

    // Names are case-sensitive.
    assert(!ParseExtensionPolicy("vk_khr_maintenance8").IsDisabled("VK_KHR_maintenance8"));

    // The match is exact: a prefix does not disable longer names.
    {
        const auto policy = ParseExtensionPolicy("VK_KHR_maintenance");
        assert(!policy.IsDisabled("VK_KHR_maintenance8"));
        assert(!policy.IsDisabled("VK_KHR_maintenance5"));
    }

    // "all" (trimmed, alone or next to other names) disables every optional extension.
    for (const char* env : {"all", " all ", "all,VK_X"}) {
        const auto policy = ParseExtensionPolicy(env);
        assert(policy.all);
        assert(!policy.Empty());
        assert(policy.IsDisabled("VK_KHR_maintenance8"));
        assert(policy.IsDisabled("VK_NV_framebuffer_mixed_samples"));
    }
    // The keyword counts in any position, not only first.
    assert(ParseExtensionPolicy("VK_X,all").all);

    // The keyword is lowercase only: "ALL" and "All" are extension names that match nothing.
    for (const char* env : {"ALL", "All"}) {
        const auto policy = ParseExtensionPolicy(env);
        assert(!policy.all);
        assert(!policy.IsDisabled("VK_KHR_maintenance8"));
    }

    // Duplicates are kept once, in order of first appearance.
    assert(ParseExtensionPolicy("VK_A,VK_A").names.size() == 1);
    assert((ParseExtensionPolicy("VK_B,VK_A,VK_B").names ==
            std::vector<std::string>{"VK_B", "VK_A"}));

    // Listed names that were not applied are reported for the log, in input order; "all" has
    // nothing to report.
    assert((IgnoredNames(ParseExtensionPolicy("VK_A,bogus"), {"VK_A"}) ==
            std::vector<std::string>{"bogus"}));
    assert(IgnoredNames(ParseExtensionPolicy("all"), {"VK_A"}).empty());

    // The log joins names with ", " and prints "none" for an empty list.
    assert(JoinNames({}) == "none");
    assert((JoinNames({"a", "b"}) == "a, b"));

    // The "extensions off" log also names an extension a default kept off, with its reason (#35),
    // once: the knob's own entry already says it is off.
    assert((OffNamesForLog({"VK_A"}, "", "Intel Windows driver") ==
            std::vector<std::string>{"VK_A"}));
    assert(OffNamesForLog({}, "", "Intel Windows driver").empty());
    assert((OffNamesForLog({}, "VK_EXT_vertex_input_dynamic_state", "Intel Windows driver") ==
            std::vector<std::string>{"VK_EXT_vertex_input_dynamic_state (Intel Windows driver)"}));
    assert((OffNamesForLog({"VK_A"}, "VK_B", "GOW3_VERTEX_INPUT_DYNAMIC=0") ==
            std::vector<std::string>{"VK_A", "VK_B (GOW3_VERTEX_INPUT_DYNAMIC=0)"}));
    assert((OffNamesForLog({"VK_A", "VK_B"}, "VK_B", "Intel Windows driver") ==
            std::vector<std::string>{"VK_A", "VK_B"}));
    assert(JoinNames(OffNamesForLog({}, "VK_EXT_vertex_input_dynamic_state",
                                    "Intel Windows driver")) ==
           "VK_EXT_vertex_input_dynamic_state (Intel Windows driver)");
    assert(JoinNames(OffNamesForLog({"VK_A"}, "VK_B", "Intel Windows driver")) ==
           "VK_A, VK_B (Intel Windows driver)");

    std::puts("extension-policy-test: ok");
    return 0;
}
