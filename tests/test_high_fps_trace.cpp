// Exercises the diagnostic machine code, including contended and full buffers.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>
#include <windows.h>
#include "../src/high_fps_trace.h"

struct Capture {
    unsigned char input[96];
    uint64_t flags_in, flags_out, rax, rcx, rdx;
    unsigned char output[96];
    uint64_t red_zone;
    uintptr_t caller;
    uint64_t registers_in[9], registers_out[9];
    uintptr_t saved_request;
    uintptr_t r12;
};
static_assert(offsetof(Capture, flags_in) == 96);
static_assert(offsetof(Capture, output) == 136);
static_assert(offsetof(Capture, red_zone) == 232);
static_assert(offsetof(Capture, caller) == 240);
static_assert(offsetof(Capture, registers_out) == 320 && offsetof(Capture, saved_request) == 392);
static_assert(offsetof(Capture, r12) == 400);
extern "C" __attribute__((sysv_abi)) void trace_invoke(void *code, Capture *capture, void *object,
                                                     void *request = nullptr, void *context = nullptr);
asm(".globl trace_invoke\ntrace_invoke:\n"
    "push %rbp\nmov %rsp, %rbp\npush %rbx\npush %r12\npush %r13\npush %r14\npush %r15\n"
    "sub $0x388, %rsp\n"
    "mov %rdi, %r12\nmov %rsi, %r13\nmov %rdx, %r14\nmov %rdx, %rbx\n"
    "mov %rdx, %rdi\nmov %rcx, %rsi\nmov $0x40, %r15d\n"
    "lea -0x340(%rbp), %rcx\nmov $96, %eax\n"
    "0: movq $0, (%rcx)\nadd $8, %rcx\ndec %eax\njnz 0b\n"
    "mov %rsi, -0xa0(%rbp)\nmov %r8, -0x78(%rbp)\n"
    "mov (%r13), %rax\nmov %rax, -0x30(%rbp)\n"
    "mov 8(%rbp), %rax\nmov %rax, 240(%r13)\n"
    "lea -136(%rsp), %rcx\nmov $16, %eax\nmovabs $0x5a5a5a5a5a5a5a5a, %rdx\n"
    "1: mov %rdx, (%rcx)\nadd $8, %rcx\ndec %eax\njnz 1b\n"
    "vmovups 0(%r13), %ymm0\nvmovups 32(%r13), %ymm1\nvmovups 64(%r13), %ymm2\n"
    "movabs $0x1111222233334444, %rax\nmovabs $0x2222333344445555, %rcx\n"
    "movabs $0x3333444455556666, %rdx\n"
    "pushfq\norq $0x8d5, (%rsp)\npopfq\npushfq\npopq 96(%r13)\n"
    "mov %rdi, 248(%r13)\nmov %rsi, 256(%r13)\nmov %r8, 264(%r13)\nmov %r9, 272(%r13)\n"
    "mov %r10, 280(%r13)\nmov %r11, 288(%r13)\nmov %rbx, 296(%r13)\nmov %rbp, 304(%r13)\n"
    "mov %r15, 312(%r13)\n"
    "call *%r12\n"
    "pushfq\npopq 104(%r13)\nmov %rax, 112(%r13)\nmov %rcx, 120(%r13)\nmov %rdx, 128(%r13)\n"
    "vmovups %ymm0, 136(%r13)\nvmovups %ymm1, 168(%r13)\nvmovups %ymm2, 200(%r13)\n"
    "mov %rdi, 320(%r13)\nmov %rsi, 328(%r13)\nmov %r8, 336(%r13)\nmov %r9, 344(%r13)\n"
    "mov %r10, 352(%r13)\nmov %r11, 360(%r13)\nmov %rbx, 368(%r13)\nmov %rbp, 376(%r13)\n"
    "mov %r15, 384(%r13)\nmov -0x248(%rbp), %rax\nmov %rax, 392(%r13)\n"
    "mov %r12, 400(%r13)\n"
    "movq $1, 232(%r13)\nlea -136(%rsp), %rcx\nmov $16, %edx\n"
    "movabs $0x5a5a5a5a5a5a5a5a, %rax\n"
    "2: cmp %rax, (%rcx)\nje 3f\nmovq $0, 232(%r13)\n"
    "3: add $8, %rcx\ndec %edx\njnz 2b\n"
    "vzeroupper\nadd $0x388, %rsp\npop %r15\npop %r14\npop %r13\npop %r12\npop %rbx\npop %rbp\nret\n");

static Capture capture() {
    Capture result{};
    for (unsigned i = 0; i < sizeof(result.input); ++i) result.input[i] = static_cast<unsigned char>(i + 1);
    return result;
}
static void preserved(const Capture& value, unsigned destination = 3, uintptr_t target = 0, bool flags = true) {
    if (flags) assert(value.flags_in == value.flags_out && (value.flags_out & 0x8d5) == 0x8d5);
    assert(value.rax == (destination == 0 ? target : 0x1111222233334444ull));
    assert(value.rcx == (destination == 1 ? target : 0x2222333344445555ull));
    assert(value.rdx == (destination == 2 ? target : 0x3333444455556666ull));
    assert(!std::memcmp(value.input, value.output, sizeof(value.input)));
    assert(value.red_zone == 1);
    assert(!std::memcmp(value.registers_in, value.registers_out, sizeof(value.registers_in)));
}
static void build(unsigned char *code, SpartanTraceSlot& slot, float& rate, unsigned input) {
    std::memset(code, 0xcc, 4096);
    const unsigned length = spartanport_trace_prefix(code, reinterpret_cast<uintptr_t>(&slot),
                                                    reinterpret_cast<uintptr_t>(&rate), input);
    assert(length > 0 && length < SPARTAN_TRACE_STUB_SIZE);
    code[length] = 0xc3;
    FlushInstructionCache(GetCurrentProcess(), code, 4096);
}
static uint64_t read64(const std::vector<unsigned char>& data, size_t at) {
    uint64_t value;
    assert(at + sizeof(value) <= data.size());
    std::memcpy(&value, data.data() + at, sizeof(value));
    return value;
}
static void check_reference(const char *path) {
    std::ifstream file(path, std::ios::binary);
    assert(file);
    std::vector<unsigned char> elf{std::istreambuf_iterator<char>(file), {}};
    assert(elf.size() >= 64 && !std::memcmp(elf.data(), "\x7f" "ELF", 4));
    uint16_t stride, count;
    std::memcpy(&stride, elf.data() + 0x36, 2);
    std::memcpy(&count, elf.data() + 0x38, 2);
    std::vector<unsigned char> image;
    for (unsigned i = 0; i < count; ++i) {
        const size_t at = static_cast<size_t>(read64(elf, 0x20)) + i * stride;
        assert(at + 56 <= elf.size());
        uint32_t type;
        std::memcpy(&type, elf.data() + at, 4);
        if (type != 1) continue;
        const size_t offset = read64(elf, at + 8), address = read64(elf, at + 16), size = read64(elf, at + 32);
        assert(offset <= elf.size() && size <= elf.size() - offset && address + size <= 512 * 1024 * 1024);
        if (image.size() < address + size) image.resize(address + size);
        std::memcpy(image.data() + address, elf.data() + offset, size);
    }
    for (const auto& point : spartanport_trace_points) {
        const uint64_t at = spartanport_signature_find(image.data(), image.size(), &point.site);
        assert(at != UINT64_MAX && spartanport_trace_match(&point, image.data() + at, image.size() - at));
        std::printf("High FPS trace: %s signature at 0x%llx\n", point.name,
                    static_cast<unsigned long long>(at + 0x400000));
    }
    for (unsigned phase = 0; phase < SPARTAN_CONTACT_PHASES; ++phase) {
        const uint64_t at = spartanport_signature_find(image.data(), image.size(), &spartanport_collision_sites[phase]);
        assert(at != UINT64_MAX && spartanport_collision_match(phase, image.data() + at, image.size() - at));
        std::printf("High FPS collision: phase=%u signature at image+0x%llx\n", phase,
                    static_cast<unsigned long long>(at));
    }
    for (unsigned branch = 0; branch < SPARTAN_CONTACT_BRANCHES; ++branch) {
        const auto& site = spartanport_contact_branches[branch];
        const uint64_t at = spartanport_signature_find(image.data(), image.size(), &site.site);
        assert(at != UINT64_MAX && spartanport_branch_match(branch, image.data() + at, image.size() - at));
        std::printf("High FPS contact branch: %s signature at image+0x%llx\n", site.name,
                    static_cast<unsigned long long>(at));
    }
}
static void contact_branch_collection() {
    SpartanCollisionTrace trace{};
    alignas(16) unsigned char stack[0x400]{};
    const uintptr_t frame = (uintptr_t)(stack + 0x380);
    const uintptr_t caller = 0x499d5c;
    std::memcpy((void *)(frame + 8), &caller, sizeof(caller));
    uint32_t object[0x200 / 4]{}, request[4]{0x3f800000, 0, 0x43348000, 0}; // z = 180.5
    object[0x1c0 / 4] = 0x10;
    object[0x190 / 4] = 0x40a00000;
    object[0x170 / 4 + 2] = 0x3e800000;
    *(uint32_t *)(frame - 0x230) = 0x41200000;
    *(uint32_t *)(frame - 0x80 + 4) = 0x3f000000;
    *(uint32_t *)(frame - 0x2b8) = 7;
    *(uintptr_t *)(frame - 0x60) = 0x1234;
    const unsigned branch = SPARTAN_CONTACT_PHASES;
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    spartanport_collision_record((uintptr_t)&trace, branch + 0, frame, (uintptr_t)object, 0xa, 0);   // jbe, CF=ZF=0
    spartanport_collision_record((uintptr_t)&trace, branch + 3, frame, (uintptr_t)object, 0x10, 1);  // jb, CF=1
    spartanport_collision_record((uintptr_t)&trace, 4, frame, (uintptr_t)object, 5, 0);
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 0, 0);
    const auto& group = trace.groups[0];
    const auto& failure = group.first_failure;
    assert(failure.valid && failure.path_count == 2 && failure.path[0] == 0 && failure.path[1] == (3 | 0x80));
    assert(failure.probe.ecx == 0x10 && failure.probe.blocked == 7 && failure.probe.height == 0x40a00000);
    assert(failure.probe.vectors[0][0] == 0x41200000 && failure.probe.vectors[3][1] == 0x3f000000);
    assert(failure.probe.vectors[6][2] == 0x3e800000 && failure.probe.surface == 0x1234);
    assert(failure.history_count == 1 && failure.history[0][2] == 0x43348000 && failure.history[0][3] == 0);
    assert(group.fail_low == 0x43348000 && group.fail_high == 0x43348000 && group.passes == 0);
    // A later success keeps its own path and the history of both calls, oldest first.
    request[2] = 0x43360000; // z = 182
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    spartanport_collision_record((uintptr_t)&trace, branch + 6, frame, (uintptr_t)object, 0, 0x40); // jbe, ZF=1
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 1, 0);
    const auto& success = group.last_success;
    assert(success.valid && success.path_count == 1 && success.path[0] == (6 | 0x80));
    assert(success.history_count == 2 && success.history[1][2] == 0x43360000 && success.history[1][3] == 1);
    assert(group.passes == 1 && group.pass_low == 0x43360000 && group.pass_high == 0x43360000);
    // Negative coordinates keep their float order in the integer-only helper.
    request[2] = 0xbf800000; // z = -1
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 0, 0);
    assert(group.fail_low == 0xbf800000 && group.fail_high == 0x43348000 && group.wall_failures == 2);
    // The path saturates instead of overflowing, and the history keeps only the newest calls.
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    for (unsigned i = 0; i < SPARTAN_CONTACT_PATH + 4; ++i)
        spartanport_collision_record((uintptr_t)&trace, branch + 1, frame, (uintptr_t)object, 0, 0x40);
    assert(trace.pending[0].path_count == SPARTAN_CONTACT_PATH + 4 && trace.pending[0].path[SPARTAN_CONTACT_PATH - 1] == (1 | 0x80));
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 0, 0);
    for (unsigned i = 0; i < SPARTAN_CONTACT_HISTORY; ++i) {
        spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
        spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 1, 0);
    }
    assert(group.history_count == 3 + 1 + SPARTAN_CONTACT_HISTORY);
    // Queries outside the wall traversal and out-of-range exits record no path.
    object[0x1c0 / 4] = 1;
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xa);
    spartanport_collision_record((uintptr_t)&trace, branch + 2, frame, (uintptr_t)object, 0, 0);
    spartanport_collision_record((uintptr_t)&trace, branch + SPARTAN_CONTACT_BRANCHES, frame, (uintptr_t)object, 0, 0);
    assert(trace.pending[0].path_count == 0);
}
static void collision_collection() {
    SpartanCollisionTrace trace{};
    alignas(16) uint32_t object[0x200 / 4]{}, request[4]{0x3f800000, 0, 0, 0};
    uintptr_t frame[2]{0, 0x12345678};
    object[0x1c0 / 4] = 8;
    spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)frame, (uintptr_t)object,
                                (uintptr_t)request, 0xa);
    object[0xa0 / 4] = 0x40000000;
    object[0x1c0 / 4] = 16;
    request[0] = 0x3f000000;
    spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)frame, (uintptr_t)object, 0x40, 0);
    assert(trace.entries == 1 && trace.completed == 1 && trace.count == 1);
    const auto& group = trace.groups[0];
    assert(group.first[1].result == 0x40);
    assert(group.calls == 1 && group.requested == 1 && group.moved == 1 && group.adjusted == 1);
    assert(group.first[0].caller == frame[1] && group.first[0].flags == 8 && group.first[1].flags == 16);
    assert(group.first[0].request[0] == 0x3f800000 && group.first[1].request[0] == 0x3f000000);
    assert(group.first[0].position[0] == 0 && group.first[1].position[0] == 0x40000000);
    assert(group.first[0].query_flags == 0xa && group.first[1].query_flags == 0xa && group.flags_seen == 24);
    assert(group.first[0].valid == 1 && group.first[1].valid == 3);
    assert(group.results_seen == 0x40 && group.zero_results == 0);
    // Repeated objects share a group, leaving space for an infrequent caller.
    for (unsigned i = 0; i < 100; ++i) {
        spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)frame, (uintptr_t)object,
                                    (uintptr_t)request, 0xa);
        spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)frame, (uintptr_t)object, 0, 0);
    }
    uintptr_t nested[2]{0, 0x87654321};
    spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)frame, (uintptr_t)object, (uintptr_t)request, 0);
    spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)nested, (uintptr_t)object, 0, 0);
    spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)nested, (uintptr_t)object, 0x101, 0);
    assert(trace.count == 2 && trace.groups[0].calls == 101 && trace.groups[1].calls == 1);
    assert(trace.groups[0].zero_results == 100 && trace.groups[0].results_seen == 0x40);
    assert(trace.groups[1].last[0].valid == 0 && trace.groups[1].last[1].valid == 2);
    assert(trace.groups[1].last[1].result == 0x101 && trace.groups[1].results_seen == 0x101);
    SpartanCollisionSnapshot snapshot{};
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.pending == 1 && snapshot.count == 2);
    assert(snapshot.groups[0].calls == 101 && snapshot.groups[0].first[0].flags == 8);
    assert(snapshot.groups[0].last[0].flags == 16);
    spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)frame, (uintptr_t)object, 0, 0);
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.pending == 0 && snapshot.count == 1);
    assert(snapshot.groups[0].first[0].caller == frame[1]);
    spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)frame, (uintptr_t)object, 0, 0);
    assert(trace.unpaired == 1);
    trace.lock = 1;
    spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)frame, (uintptr_t)object, 0, 0);
    assert(trace.busy == 1 && !spartanport_collision_take(&trace, &snapshot));
    trace.lock = 0;
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.busy == 1 && snapshot.unpaired == 1);
    for (unsigned i = 0; i < SPARTAN_TRACE_CAPACITY + 1; ++i) {
        frame[1] = i + 1;
        spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)frame, (uintptr_t)object, 0, 0);
        spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)frame, (uintptr_t)object, 0, 0);
    }
    assert(trace.count == SPARTAN_TRACE_CAPACITY && trace.full == 1);
    assert(trace.groups[0].first[0].caller == 1 && trace.groups[63].first[0].caller == 64);
    assert(spartanport_collision_take(&trace, &snapshot));
    uintptr_t frames[SPARTAN_TRACE_CAPACITY + 1][2]{};
    for (auto& pending : frames)
        spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)pending, (uintptr_t)object, 0, 0);
    assert(trace.full == 1);
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.pending == SPARTAN_TRACE_CAPACITY);
    for (auto& pending : frames)
        spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)pending, (uintptr_t)object, 0, 0);
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.pending == 0 && snapshot.unpaired == 1);
    auto concurrent = [&](uintptr_t caller) {
        uintptr_t local_frame[2]{0, caller};
        for (unsigned i = 0; i < 10000; ++i) {
            spartanport_collision_record((uintptr_t)&trace, 0, (uintptr_t)local_frame, (uintptr_t)object, 0, 0);
            spartanport_collision_record((uintptr_t)&trace, 1, (uintptr_t)local_frame, (uintptr_t)object, 0, 0);
        }
    };
    std::thread first(concurrent, 0x111), second(concurrent, 0x222);
    first.join(); second.join();
    assert(spartanport_collision_take(&trace, &snapshot));
    assert(snapshot.count <= 2 && snapshot.completed <= 20000 && snapshot.entries <= 20000);
    for (unsigned i = 0; i < snapshot.count; ++i) {
        const auto& result = snapshot.groups[i];
        assert(result.last[0].caller == result.last[1].caller);
        assert(result.last[0].caller == 0x111 || result.last[0].caller == 0x222);
        assert(result.last[0].object == (uintptr_t)object && result.last[1].object == (uintptr_t)object);
    }
}
static void contact_internal_collection() {
    SpartanCollisionTrace trace{};
    alignas(16) unsigned char stack[0x400]{};
    const uintptr_t frame = (uintptr_t)(stack + 0x380);
    const uintptr_t caller = 0x499d5c;
    std::memcpy((void *)(frame + 8), &caller, sizeof(caller));
    uint32_t object[0x200 / 4]{}, request[4]{0x3f800000, 0, 0, 0};
    object[0x1c0 / 4] = 0x10;
    *(uint32_t *)(frame - 0x24c) = 0x3f3504f3;
    *(uint32_t *)(frame - 0x2b0 + 4) = 0x3f800000;
    *(uint32_t *)(frame - 0x1a0) = 0x40000000;
    *(uint32_t *)(frame - 0x150) = 0x40400000;
    *(uint32_t *)(frame - 0x140 + 4) = 0x3f000000;
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    spartanport_collision_record((uintptr_t)&trace, 2, frame, (uintptr_t)object, 2, 0);
    assert(trace.pending[0].stage_mask == 1 && trace.pending[0].stages[0].mask == 2);
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 0, 0);
    const auto& detail = trace.groups[0].first_failure;
    assert(detail.valid && detail.stage_mask == 1 && trace.groups[0].wall_failures == 1);
    assert(detail.pair[0].caller == caller && detail.pair[1].result == 0);
    const auto& stage = detail.stages[0];
    assert(stage.calls == 1 && stage.threshold == 0x3f3504f3 && stage.up[1] == 0x3f800000);
    assert(stage.points[0][0] == 0x40000000 && stage.hits[0][0] == 0x40400000);
    assert(stage.normals[0][1] == 0x3f000000 && stage.normals[1][1] == 0);
    // A successful reference uses a fresh path, not stages left by a prior invocation.
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    *(uint32_t *)(frame - 0x220) = 0x40800000;
    *(uint32_t *)(frame - 0xe0 + 4) = 0x3f400000;
    spartanport_collision_record((uintptr_t)&trace, 4, frame, (uintptr_t)object, 0xa, 0);
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 1, 0);
    const auto& success = trace.groups[0].last_success;
    assert(success.valid && success.stage_mask == 4 && success.stages[0].calls == 0);
    assert(success.stages[2].calls == 1 && success.stages[2].mask == 0xa);
    assert(success.stages[2].points[0][0] == 0x40800000 && success.stages[2].normals[1][1] == 0x3f400000);
    assert(trace.groups[0].first_failure.stage_mask == 1 && trace.groups[0].wall_failures == 1);
    // A report between nested calls preserves the independent pending paths.
    const uintptr_t nested = frame - 0x20;
    std::memcpy((void *)(nested + 8), &caller, sizeof(caller));
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xb);
    spartanport_collision_record((uintptr_t)&trace, 0, nested, (uintptr_t)object, (uintptr_t)request, 0xb);
    spartanport_collision_record((uintptr_t)&trace, 3, nested, (uintptr_t)object, 8, 0);
    SpartanCollisionSnapshot snapshot{};
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.pending == 2);
    spartanport_collision_record((uintptr_t)&trace, 1, nested, (uintptr_t)object, 0, 0);
    spartanport_collision_record((uintptr_t)&trace, 2, frame, (uintptr_t)object, 0, 0);
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 0, 0);
    assert(trace.groups[0].wall_failures == 2 && trace.groups[0].first_failure.stage_mask == 2);
    assert(trace.groups[0].first_failure.stages[1].mask == 8);
    assert(spartanport_collision_take(&trace, &snapshot) && snapshot.pending == 0);
    object[0x1c0 / 4] = 1;
    spartanport_collision_record((uintptr_t)&trace, 0, frame, (uintptr_t)object, (uintptr_t)request, 0xa);
    spartanport_collision_record((uintptr_t)&trace, 2, frame, (uintptr_t)object, 0, 0);
    spartanport_collision_record((uintptr_t)&trace, 1, frame, (uintptr_t)object, 0, 0);
    assert(!trace.groups[0].first_failure.valid && trace.groups[0].wall_failures == 0);
    // Miss outputs can be inaccessible: the observer must honor the ray validity mask.
    auto *pages = (unsigned char *)VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(pages);
    DWORD previous;
    assert(VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &previous));
    SpartanContactStage missed{};
    spartanport_contact_stage(&missed, (uintptr_t)(pages + 4096 + 0x150), 0, 0);
    assert(missed.calls == 1 && missed.mask == 0 && missed.normals[0][0] == 0 && missed.hits[1][0] == 0);
    VirtualFree(pages, 0, MEM_RELEASE);
}
int main(int argc, char **argv) {
    contact_branch_collection();
    contact_internal_collection();
    collision_collection();
    auto *code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT,
                                                        PAGE_EXECUTE_READWRITE));
    assert(code);
    {
        SpartanCollisionTrace collision{};
        const unsigned length = spartanport_collision_prefix(code, (uintptr_t)&collision,
            (uintptr_t)spartanport_collision_record, 0);
        assert(length && length < SPARTAN_TRACE_STUB_SIZE);
        code[length] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        alignas(16) unsigned char collision_object[0x200]{}, context[0x180]{};
        float request[4]{1, 2, 3, 0};
        Capture value = capture();
        trace_invoke(code, &value, collision_object, request, context);
        preserved(value);
        assert(collision.entries == 1);
        const unsigned exit_length = spartanport_collision_prefix(code, (uintptr_t)&collision,
            (uintptr_t)spartanport_collision_record, 1);
        assert(exit_length && exit_length < SPARTAN_TRACE_STUB_SIZE);
        code[exit_length] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        trace_invoke(code, &value, collision_object, request, context);
        preserved(value);
        assert(collision.completed == 1);
        assert(collision.groups[0].last[0].query_flags == 0x55556666 && collision.groups[0].last[1].result == 0x40);
        assert(collision.groups[0].last[0].object == (uintptr_t)collision_object);
        assert(collision.groups[0].last[0].request[0] == 0x3f800000);
        collision = {};
        uintptr_t original_operand = 0xabcdef1234567890ull;
        const unsigned entry_stub = spartanport_collision_stub(code, (uintptr_t)&collision,
            (uintptr_t)spartanport_collision_record, 0, (uintptr_t)(code + 512), 0);
        const unsigned exit_stub = spartanport_collision_stub(code + 512, (uintptr_t)&collision,
            (uintptr_t)spartanport_collision_record, 1, (uintptr_t)(code + 1024), (uintptr_t)&original_operand);
        assert(entry_stub < SPARTAN_TRACE_STUB_SIZE && exit_stub < SPARTAN_TRACE_STUB_SIZE);
        code[1024] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        trace_invoke(code, &value, collision_object, request, context);
        preserved(value, 0, original_operand);
        assert(value.saved_request == (uintptr_t)request);
        assert(collision.completed == 1 && collision.count == 1 && collision.groups[0].last[1].result == 0x40);
        // A synthetic query corrects the target between the real entry and exit stubs.
        collision = {};
        const unsigned corrected_exit = spartanport_collision_stub(code + 768, (uintptr_t)&collision,
            (uintptr_t)spartanport_collision_record, 1, (uintptr_t)(code + 1024), (uintptr_t)&original_operand);
        assert(corrected_exit && corrected_exit < SPARTAN_TRACE_STUB_SIZE);
        const unsigned char body[] = {0xc7,0x06,0,0,0,0x3f,0xff,0x25,0,0,0,0};
        std::memcpy(code + 512, body, sizeof(body));
        const uintptr_t corrected_target = (uintptr_t)(code + 768);
        std::memcpy(code + 512 + sizeof(body), &corrected_target, sizeof(corrected_target));
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        trace_invoke(code, &value, collision_object, request, context);
        preserved(value, 0, original_operand);
        assert(collision.completed == 1 && collision.groups[0].adjusted == 1);
        assert(collision.groups[0].first[0].request[0] == 0x3f800000);
        assert(collision.groups[0].first[1].request[0] == 0x3f000000);
        assert(collision.groups[0].first[1].result == 0x40 && request[0] == 0.5f);
        assert(!spartanport_collision_stub(code + 1536, (uintptr_t)&collision,
            (uintptr_t)spartanport_collision_record, 1, (uintptr_t)(code + 1024), 0));
        for (unsigned phase = 0; phase < SPARTAN_CONTACT_PHASES; ++phase) {
            std::vector<unsigned char> image(256);
            int pattern[SPARTAN_SIGNATURE_MAX];
            const auto& site = spartanport_collision_sites[phase];
            const unsigned size = spartanport_signature_parse(site.pattern, pattern);
            assert(size);
            for (unsigned i = 0; i < size; ++i) image[32 + i] = pattern[i] < 0 ? 0 : (unsigned char)pattern[i];
            assert(spartanport_signature_find(image.data(), image.size(), &site) == 32 + site.at);
            assert(spartanport_collision_match(phase, image.data() + 32 + site.at, size - site.at));
            image[32 + site.at] ^= 1;
            assert(!spartanport_collision_match(phase, image.data() + 32 + site.at, size - site.at));
            assert(spartanport_signature_find(image.data(), image.size(), &site) == UINT64_MAX);
            image[32 + site.at] ^= 1;
            std::memcpy(image.data() + 128, image.data() + 32, size);
            assert(spartanport_signature_find(image.data(), image.size(), &site) == UINT64_MAX);
        }
        *(uint32_t *)(collision_object + 0x1c0) = 0x10;
        for (unsigned phase = 2; phase < SPARTAN_CONTACT_PHASES; ++phase) {
            collision = {};
            const unsigned entry = spartanport_collision_prefix(code + 1536, (uintptr_t)&collision,
                (uintptr_t)spartanport_collision_record, 0);
            code[1536 + entry] = 0xc3;
            const unsigned prefix = spartanport_collision_prefix(code, (uintptr_t)&collision,
                (uintptr_t)spartanport_collision_record, phase);
            code[prefix] = 0xc3;
            FlushInstructionCache(GetCurrentProcess(), code, 4096);
            trace_invoke(code + 1536, &value, collision_object, request);
            trace_invoke(code, &value, collision_object, request);
            preserved(value);
            assert(collision.pending[0].stage_mask == (1u << (phase - 2)));
            assert(collision.pending[0].stages[phase - 2].mask == 0x33334444);
            const unsigned original_length = spartanport_collision_length(phase);
            std::memcpy(code + 512, spartanport_collision_original[phase], original_length);
            code[512 + original_length] = 0xc3;
            const unsigned full = spartanport_collision_stub(code, (uintptr_t)&collision,
                (uintptr_t)spartanport_collision_record, phase, (uintptr_t)(code + 1024), 0);
            assert(full && full < SPARTAN_TRACE_STUB_SIZE);
            FlushInstructionCache(GetCurrentProcess(), code, 4096);
            Capture reference = capture();
            trace_invoke(code + 512, &reference, collision_object, request);
            trace_invoke(code, &value, collision_object, request);
            preserved(value, 0, reference.rax, false);
            assert(((value.flags_out ^ reference.flags_out) & (phase == 4 ? 0x8d5 : 0x8c5)) == 0);
            if (phase < 4) assert(value.r12 == 0x33334444 && reference.r12 == value.r12);
            assert(collision.pending[0].stages[phase - 2].calls == 2);
        }
        // The harness enters with CF=1 and ZF=1: every condition here is taken except jne.
        for (unsigned branch = 0; branch < SPARTAN_CONTACT_BRANCHES; ++branch) {
            collision = {};
            const unsigned entry = spartanport_collision_prefix(code + 1536, (uintptr_t)&collision,
                (uintptr_t)spartanport_collision_record, 0);
            code[1536 + entry] = 0xc3;
            const bool taken = spartanport_contact_branches[branch].condition != 0x85;
            const unsigned char ret[] = {0xc3}, trap[] = {0x0f, 0x0b};
            std::memcpy(code + 512, taken ? trap : ret, taken ? sizeof(trap) : sizeof(ret));
            std::memcpy(code + 1024, taken ? ret : trap, taken ? sizeof(ret) : sizeof(trap));
            const unsigned stub = spartanport_branch_stub(code, (uintptr_t)&collision,
                (uintptr_t)spartanport_collision_record, branch, (uintptr_t)(code + 1024), (uintptr_t)(code + 512));
            assert(stub && stub < SPARTAN_TRACE_STUB_SIZE);
            FlushInstructionCache(GetCurrentProcess(), code, 4096);
            trace_invoke(code + 1536, &value, collision_object, request);
            trace_invoke(code, &value, collision_object, request);
            preserved(value);
            assert(collision.pending[0].path_count == 1);
            assert(collision.pending[0].path[0] == (branch | (taken ? 0x80u : 0u)));
            assert(collision.pending[0].probe.ecx == 0x44445555);
            assert(!spartanport_branch_stub(code, (uintptr_t)&collision,
                (uintptr_t)spartanport_collision_record, branch, 0, (uintptr_t)(code + 512)));
            std::vector<unsigned char> image(256);
            int pattern[SPARTAN_SIGNATURE_MAX];
            const auto& site = spartanport_contact_branches[branch].site;
            const unsigned size = spartanport_signature_parse(site.pattern, pattern);
            assert(size);
            for (unsigned i = 0; i < size; ++i) image[32 + i] = pattern[i] < 0 ? 0 : (unsigned char)pattern[i];
            assert(spartanport_signature_find(image.data(), image.size(), &site) == 32 + site.at);
            assert(spartanport_branch_match(branch, image.data() + 32 + site.at, size - site.at));
            image[32 + site.at + 1] ^= 1;
            assert(!spartanport_branch_match(branch, image.data() + 32 + site.at, size - site.at));
            image[32 + site.at + 1] ^= 1;
            std::memcpy(image.data() + 128, image.data() + 32, size);
            assert(spartanport_signature_find(image.data(), image.size(), &site) == UINT64_MAX);
        }
    }
    float rate = 240.0f;
    unsigned char object[0x200]{};
    const float field = 9.25f;
    std::memcpy(object + 0x148, &field, sizeof(field));
    const uint16_t state = 8;
    std::memcpy(object + 0x1c0, &state, sizeof(state));
    for (unsigned input = 0; input <= SPARTAN_TRACE_FRAME_CONTEXT; ++input) {
        SpartanTraceSlot slot{};
        build(code, slot, rate, input);
        if (argc > 1 && !std::strcmp(argv[1], "--mutate-flags")) {
            std::memmove(code + 1, code, SPARTAN_TRACE_STUB_SIZE);
            code[0] = 0xf8; // A broken hook changes CF before saving it.
            FlushInstructionCache(GetCurrentProcess(), code, 4096);
        }
        Capture value = capture();
        trace_invoke(code, &value, object);
        preserved(value);
        assert(slot.count == 1 && slot.lock == 0 && slot.dropped == 0);
        const unsigned source = input == SPARTAN_TRACE_FRAME_CONTEXT ? 2 : input;
        const void *expected = input == 3 ? object + 0x148 : value.input + source * 32;
        assert(!std::memcmp(&slot.records[0].input, expected, input >= 3 ? 4 : 16));
        assert(slot.records[0].rate == rate);
        if (input == SPARTAN_TRACE_FRAME_CONTEXT) {
            uintptr_t recorded_object;
            std::memcpy(&recorded_object, slot.records[0].input + 2, sizeof(recorded_object));
            assert(recorded_object == reinterpret_cast<uintptr_t>(object));
            assert(slot.records[0].state == state && slot.records[0].caller == value.caller);
            slot.lock = 1;
            trace_invoke(code, &value, object);
            preserved(value);
            assert(slot.count == 1 && slot.dropped == 1 && slot.lock == 1);
        }
    }
    SpartanTraceSlot slot{};
    build(code, slot, rate, 0);
    for (unsigned i = 0; i < 100; ++i) {
        Capture value = capture();
        const float sample = static_cast<float>(i);
        std::memcpy(value.input, &sample, sizeof(sample));
        trace_invoke(code, &value, object);
        preserved(value);
    }
    SpartanTraceSnapshot snapshot{};
    assert(spartanport_trace_take(&slot, &snapshot));
    assert(snapshot.calls == 100 && snapshot.samples == SPARTAN_TRACE_CAPACITY && snapshot.dropped == 0);
    assert(snapshot.records[0].input[0] == 36.0f && snapshot.records[63].input[0] == 99.0f);
    assert(slot.count == 0 && slot.lock == 0);
    slot.lock = 1;
    Capture value = capture();
    trace_invoke(code, &value, object);
    preserved(value);
    assert(slot.dropped == 1 && slot.count == 0 && slot.lock == 1);
    assert(!spartanport_trace_take(&slot, &snapshot));
    slot.lock = 0;
    assert(spartanport_trace_take(&slot, &snapshot));
    assert(snapshot.dropped == 1 && snapshot.calls == 0 && snapshot.samples == 0);
    slot = {};
    auto worker = [&] {
        Capture local = capture();
        for (unsigned i = 0; i < 10000; ++i) trace_invoke(code, &local, object);
        preserved(local);
    };
    std::thread first(worker), second(worker);
    first.join(); second.join();
    assert(spartanport_trace_take(&slot, &snapshot));
    assert(snapshot.calls + snapshot.dropped == 20000 && snapshot.samples <= SPARTAN_TRACE_CAPACITY);
    SpartanTraceSnapshot contexts{};
    SpartanTraceGroup groups[SPARTAN_TRACE_CAPACITY];
    assert(spartanport_trace_group_context(&contexts, groups) == 0);
    contexts.samples = 4;
    const uintptr_t objects[] = {0x1111222233334444ull, 0x1111222233334444ull, 0x5555666677778888ull, 0x1111222233334444ull};
    const uintptr_t callers[] = {0x47d3f5, 0x47d3f5, 0x47d3f5, 0x489822};
    for (unsigned i = 0; i < contexts.samples; ++i) {
        auto& record = contexts.records[i];
        std::memcpy(record.input + 2, &objects[i], sizeof(objects[i]));
        record.caller = callers[i];
        record.state = i + 3;
        record.input[0] = static_cast<float>(i + 1);
        record.rate = 240;
    }
    assert(spartanport_trace_group_context(&contexts, groups) == 3);
    assert(groups[0].object == objects[0] && groups[0].caller == callers[0]);
    assert(groups[0].samples == 2 && groups[0].state == 4 && groups[0].input == 2 && groups[0].rate == 240);
    assert(groups[1].object == objects[2] && groups[1].caller == callers[2] && groups[1].samples == 1);
    assert(groups[2].object == objects[3] && groups[2].caller == callers[3] && groups[2].samples == 1);
    contexts.samples = SPARTAN_TRACE_CAPACITY + 1;
    for (unsigned i = 0; i < SPARTAN_TRACE_CAPACITY; ++i) {
        const uintptr_t object_id = i + 1;
        std::memcpy(contexts.records[i].input + 2, &object_id, sizeof(object_id));
    }
    assert(spartanport_trace_group_context(&contexts, groups) == SPARTAN_TRACE_CAPACITY);
    for (const auto& point : spartanport_trace_points) {
        slot = {};
        std::memset(code, 0xcc, 4096);
        const uintptr_t target = reinterpret_cast<uintptr_t>(&rate);
        const unsigned length = spartanport_trace_stub(code, &point, reinterpret_cast<uintptr_t>(&slot),
                                                      target, reinterpret_cast<uintptr_t>(code + 512));
        assert(length > 0 && length < SPARTAN_TRACE_STUB_SIZE);
        code[512] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        value = capture();
        trace_invoke(code, &value, object);
        preserved(value, point.destination, target);
        assert(slot.count == 1);
        std::vector<unsigned char> image(256);
        int pattern[SPARTAN_SIGNATURE_MAX];
        const unsigned size = spartanport_signature_parse(point.site.pattern, pattern);
        assert(size > 0);
        for (unsigned i = 0; i < size; ++i) image[32 + i] = pattern[i] < 0 ? 0 : static_cast<unsigned char>(pattern[i]);
        assert(spartanport_signature_find(image.data(), image.size(), &point.site) == 32 + point.site.at);
        assert(spartanport_trace_match(&point, image.data() + 32 + point.site.at, size - point.site.at));
        image[32 + point.site.at + 1] = 0x8b;
        assert(!spartanport_trace_match(&point, image.data() + 32 + point.site.at, size - point.site.at));
        assert(spartanport_signature_find(image.data(), image.size(), &point.site) == UINT64_MAX);
        image[32 + point.site.at + 1] = 0x8d;
        std::memcpy(image.data() + 128, image.data() + 32, size);
        assert(spartanport_signature_find(image.data(), image.size(), &point.site) == UINT64_MAX);
    }
    uint64_t last = 0;
    assert(!spartanport_trace_due(&last, 100) && last == 100);
    assert(!spartanport_trace_due(&last, 5000099));
    assert(spartanport_trace_due(&last, 5000100) && last == 5000100);
    assert(!spartanport_trace_due(&last, 5000100));
    assert(!spartanport_trace_due(&last, 4000000));
    if (argc > 1) check_reference(argv[1]);
    VirtualFree(code, 0, MEM_RELEASE);
    std::puts("High FPS trace: register/flags/red-zone preservation, bounded buffer and contention OK");
}
