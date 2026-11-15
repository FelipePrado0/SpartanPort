// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include "video_core/renderer_vulkan/vk_instance.h"

// Keep the dispatcher's ABI consistent with the renderer library.
#undef NDEBUG
#include <cassert>

namespace Vulkan {
bool InitializePipelineBinaryCache(vk::Device device, bool supported,
                                   vk::PipelineBinaryKeyKHR& global_key);
}

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
namespace {
static_assert(offsetof(VkPipelineBinaryKeyKHR, keySize) == 16 &&
              offsetof(VkPipelineBinaryKeyKHR, key) == 20 && VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR == 32);

// Win64 passes arguments in rcx, rdx, r8 and keeps rbx, rsi, rdi, r12 to r15 and xmm6 to xmm15 across
// calls; SysV passes them in rdi, rsi, rdx and keeps rbx and r12 to r15. Both also keep rbp and rsp.
// These two are naked (assembly only) because a C++ function restores the registers it changes.

// A fake driver for the global key query: fills key like GetKey (size 32, first byte 9; an error if
// info is not null) and returns with garbage in every callee-saved register but rsp and rbp. rbp is
// the frame pointer, which a clobber cannot protect, so changing it here would only crash the test.
[[gnu::naked]] VkResult DriverGetKeyChangingRegisters(VkDevice, const VkPipelineCreateInfoKHR*,
                                                      VkPipelineBinaryKeyKHR*) {
#ifdef _WIN32
    asm("    movl $32, 16(%r8); movb $9, 20(%r8)\n"
        "    movabsq $0xbad0bad0bad0bad0, %rax\n"
        "    movq %rax, %rbx; movq %rax, %rsi; movq %rax, %rdi\n"
        "    movq %rax, %r12; movq %rax, %r13; movq %rax, %r14; movq %rax, %r15\n"
        "    pcmpeqd %xmm6, %xmm6; pcmpeqd %xmm7, %xmm7; pcmpeqd %xmm8, %xmm8; pcmpeqd %xmm9, %xmm9\n"
        "    pcmpeqd %xmm10, %xmm10; pcmpeqd %xmm11, %xmm11; pcmpeqd %xmm12, %xmm12\n"
        "    pcmpeqd %xmm13, %xmm13; pcmpeqd %xmm14, %xmm14; pcmpeqd %xmm15, %xmm15\n"
        "    xorl %eax, %eax; testq %rdx, %rdx; jz 1f\n"
        "    movl $-13, %eax\n" // VK_ERROR_UNKNOWN
        "1:  ret\n");
#else
    asm("    movl $32, 16(%rdx); movb $9, 20(%rdx)\n"
        "    movabsq $0xbad0bad0bad0bad0, %rax\n"
        "    movq %rax, %rbx; movq %rax, %r12; movq %rax, %r13; movq %rax, %r14; movq %rax, %r15\n"
        "    xorl %eax, %eax; testq %rsi, %rsi; jz 1f\n"
        "    movl $-13, %eax\n" // VK_ERROR_UNKNOWN
        "1:  ret\n");
#endif
}

// Calls call(context) with a known value in each callee-saved register but rsp and rbp, and returns
// a mask of the ones that came back changed: 0 when all were kept. Bit 0 is the last register
// checked (xmm15 on Win64, r15 on SysV), bit 1 the one before it, and so on up to rbx.
[[gnu::naked]] unsigned CallCheckingCalleeSaved(void (*)(void*), void*) {
#ifdef _WIN32
    asm("    pushq %rbx; pushq %rsi; pushq %rdi; pushq %r12; pushq %r13; pushq %r14; pushq %r15\n"
        "    subq $192, %rsp\n" // 32 bytes of shadow space, then the caller's xmm6 to xmm15
        "    movdqu %xmm6, 32(%rsp); movdqu %xmm7, 48(%rsp); movdqu %xmm8, 64(%rsp)\n"
        "    movdqu %xmm9, 80(%rsp); movdqu %xmm10, 96(%rsp); movdqu %xmm11, 112(%rsp)\n"
        "    movdqu %xmm12, 128(%rsp); movdqu %xmm13, 144(%rsp); movdqu %xmm14, 160(%rsp)\n"
        "    movdqu %xmm15, 176(%rsp)\n"
        "    movq %rcx, %rax; movq %rdx, %rcx\n"
        "    movabsq $0x5e575e575e575e57, %r11\n"
        "    movq %r11, %rbx; movq %r11, %rsi; movq %r11, %rdi\n"
        "    movq %r11, %r12; movq %r11, %r13; movq %r11, %r14; movq %r11, %r15\n"
        "    movq %r11, %xmm6; movq %r11, %xmm7; movq %r11, %xmm8; movq %r11, %xmm9\n"
        "    movq %r11, %xmm10; movq %r11, %xmm11; movq %r11, %xmm12; movq %r11, %xmm13\n"
        "    movq %r11, %xmm14; movq %r11, %xmm15\n"
        "    callq *%rax\n"
        // For each register: CF = (register != r11), shifted into the mask.
        "    movabsq $0x5e575e575e575e57, %r11\n"
        "    xorl %eax, %eax\n"
        "    xorq %r11, %rbx; negq %rbx; adcl %eax, %eax\n"
        "    xorq %r11, %rsi; negq %rsi; adcl %eax, %eax\n"
        "    xorq %r11, %rdi; negq %rdi; adcl %eax, %eax\n"
        "    xorq %r11, %r12; negq %r12; adcl %eax, %eax\n"
        "    xorq %r11, %r13; negq %r13; adcl %eax, %eax\n"
        "    xorq %r11, %r14; negq %r14; adcl %eax, %eax\n"
        "    xorq %r11, %r15; negq %r15; adcl %eax, %eax\n"
        "    movq %xmm6, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm7, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm8, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm9, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm10, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm11, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm12, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm13, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm14, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movq %xmm15, %rdx; xorq %r11, %rdx; negq %rdx; adcl %eax, %eax\n"
        "    movdqu 32(%rsp), %xmm6; movdqu 48(%rsp), %xmm7; movdqu 64(%rsp), %xmm8\n"
        "    movdqu 80(%rsp), %xmm9; movdqu 96(%rsp), %xmm10; movdqu 112(%rsp), %xmm11\n"
        "    movdqu 128(%rsp), %xmm12; movdqu 144(%rsp), %xmm13; movdqu 160(%rsp), %xmm14\n"
        "    movdqu 176(%rsp), %xmm15\n"
        "    addq $192, %rsp\n"
        "    popq %r15; popq %r14; popq %r13; popq %r12; popq %rdi; popq %rsi; popq %rbx\n"
        "    ret\n");
#else
    asm("    pushq %rbx; pushq %r12; pushq %r13; pushq %r14; pushq %r15\n" // rsp 16-byte aligned
        "    movq %rdi, %rax; movq %rsi, %rdi\n"
        "    movabsq $0x5e575e575e575e57, %r11\n"
        "    movq %r11, %rbx; movq %r11, %r12; movq %r11, %r13; movq %r11, %r14; movq %r11, %r15\n"
        "    callq *%rax\n"
        // For each register: CF = (register != r11), shifted into the mask.
        "    movabsq $0x5e575e575e575e57, %r11\n"
        "    xorl %eax, %eax\n"
        "    xorq %r11, %rbx; negq %rbx; adcl %eax, %eax\n"
        "    xorq %r11, %r12; negq %r12; adcl %eax, %eax\n"
        "    xorq %r11, %r13; negq %r13; adcl %eax, %eax\n"
        "    xorq %r11, %r14; negq %r14; adcl %eax, %eax\n"
        "    xorq %r11, %r15; negq %r15; adcl %eax, %eax\n"
        "    popq %r15; popq %r14; popq %r13; popq %r12; popq %rbx\n"
        "    ret\n");
#endif
}
}
#endif

namespace {
unsigned key_queries{};
VkResult key_result = VK_SUCCESS;
uint32_t key_size = 2;

void SetCache(const char* value) {
#ifdef _WIN32
    _putenv_s("SPARTAN_PIPELINE_BINARIES", value);
#else
    if (*value) setenv("SPARTAN_PIPELINE_BINARIES", value, 1);
    else unsetenv("SPARTAN_PIPELINE_BINARIES");
#endif
}

VKAPI_ATTR VkResult VKAPI_CALL GetKey(VkDevice, const VkPipelineCreateInfoKHR* info,
                                     VkPipelineBinaryKeyKHR* key) {
    assert(!info);
    ++key_queries;
    key->keySize = key_size;
    key->key[0] = 7;
    return key_result;
}

void TestFallbacks() {
    auto& dispatch = VULKAN_HPP_DEFAULT_DISPATCHER;
    const auto original = dispatch;
    dispatch.vkGetPipelineKeyKHR = GetKey;
    dispatch.vkCreatePipelineBinariesKHR = [](VkDevice, const VkPipelineBinaryCreateInfoKHR*,
        const VkAllocationCallbacks*, VkPipelineBinaryHandlesInfoKHR*) -> VkResult {
        std::abort();
    };
    dispatch.vkGetPipelineBinaryDataKHR = [](VkDevice, const VkPipelineBinaryDataInfoKHR*,
        VkPipelineBinaryKeyKHR*, size_t*, void*) -> VkResult { std::abort(); };
    dispatch.vkDestroyPipelineBinaryKHR = [](VkDevice, VkPipelineBinaryKHR,
        const VkAllocationCallbacks*) { std::abort(); };
    dispatch.vkReleaseCapturedPipelineDataKHR = [](VkDevice,
        const VkReleaseCapturedPipelineDataInfoKHR*, const VkAllocationCallbacks*) -> VkResult {
        std::abort();
    };
    const vk::Device device{reinterpret_cast<VkDevice>(uintptr_t{1})};
    vk::PipelineBinaryKeyKHR key{.keySize = 2};
    SetCache("0");
    assert(!Vulkan::InitializePipelineBinaryCache(device, true, key));
    assert(key_queries == 0 && key.keySize == 0);

    SetCache("1");
    assert(!Vulkan::InitializePipelineBinaryCache(device, false, key));
    assert(key_queries == 0 && key.keySize == 0);

    const auto get_data = dispatch.vkGetPipelineBinaryDataKHR;
    dispatch.vkGetPipelineBinaryDataKHR = nullptr;
    assert(!Vulkan::InitializePipelineBinaryCache(device, true, key));
    assert(key_queries == 0 && key.keySize == 0);
    dispatch.vkGetPipelineBinaryDataKHR = get_data;

    key_result = VK_ERROR_UNKNOWN;
    assert(!Vulkan::InitializePipelineBinaryCache(device, true, key));
    assert(key_queries == 1 && key.keySize == 0);

    key_result = VK_SUCCESS;
    key_size = 0;
    assert(!Vulkan::InitializePipelineBinaryCache(device, true, key));
    assert(key_queries == 2 && key.keySize == 0);

    key_size = VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR + 1;
    assert(!Vulkan::InitializePipelineBinaryCache(device, true, key));
    assert(key_queries == 3 && key.keySize == 0);

    SetCache("");
    key_size = VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR;
    assert(Vulkan::InitializePipelineBinaryCache(device, true, key));
    assert(key_queries == 4 && key.keySize == key_size && key.key[0] == 7);

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    // NVIDIA drivers return from the global key query with callee-saved registers changed (#1352).
    dispatch.vkGetPipelineKeyKHR = DriverGetKeyChangingRegisters;
    struct Call {
        vk::Device device;
        vk::PipelineBinaryKeyKHR& key;
        bool enabled;
    } call{device, key, false};
    const unsigned changed = CallCheckingCalleeSaved(
        [](void* context) {
            auto& call = *static_cast<Call*>(context);
            call.enabled = Vulkan::InitializePipelineBinaryCache(call.device, true, call.key);
        },
        &call);
    if (changed) {
        std::fprintf(stderr, "FAIL: callee-saved registers changed (mask %#x)\n", changed);
        std::abort();
    }
    assert(call.enabled && key.keySize == 32 && key.key[0] == 9);
#endif
    dispatch = original;
}
}

int main(int argc, char**) {
    const bool enabled = argc > 1;
    SetCache(enabled ? "1" : "0");
    Vulkan::Instance instance{s32{-1}, false};
    if (!enabled && instance.GetPipelineBinaryGlobalKey().keySize != 0) {
        std::fputs("FAIL: disabled binary cache initialized a global key\n", stderr);
        return 1;
    }
    std::printf("Real device: binary cache %s, global key size %u\n",
                enabled ? "requested" : "disabled", instance.GetPipelineBinaryGlobalKey().keySize);
    TestFallbacks();
    std::puts("pipeline-binary-initialization-test: passed");
}
