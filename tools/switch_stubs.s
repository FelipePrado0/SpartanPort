# Source of the switch-attack stubs in src/switch_hook.h (GNU as, x86-64).
# as --64 -o switch.o switch_stubs.s; each stub is the bytes from its label to its _end label.
# Placeholders: 0x1111111111111111 the shared state, 0x2222222222222222 the C function the stub
# calls (sysv_abi), 0x3333333333333333 where a passed input check goes on (0xeaa90).
.intel_syntax noprefix

# 0xeb290, a branch's input condition (rdi, rsi, rdx branch, rcx). A branch to a weapon's switch
# attack asks spartanport_switch_input(branch, return address) instead: 1 goes on to the rest of the conditions, 0 fails, 2 is
# the game's own check (not one of those branches after all).
.globl input_entry
input_entry:
    cmp byte ptr [rdx + 0x30], 1
    jne 9f
    cmp byte ptr [rdx + 0x31], 1
    je 7f
    cmp byte ptr [rdx + 0x31], 4
    jne 9f
7:
    cmp byte ptr [rdx + 0x2e], 0x0f
    jne 9f
    movzx eax, byte ptr [rdx + 0x2f]
    sub eax, 0x10
    cmp eax, 3
    ja 9f
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 0xc0
    mov [rsp], rdi
    mov [rsp + 8], rsi
    mov [rsp + 16], rdx
    mov [rsp + 24], rcx
    mov [rsp + 32], r8
    mov [rsp + 40], r9
    movdqu [rsp + 0x40], xmm0
    movdqu [rsp + 0x50], xmm1
    movdqu [rsp + 0x60], xmm2
    movdqu [rsp + 0x70], xmm3
    movdqu [rsp + 0x80], xmm4
    movdqu [rsp + 0x90], xmm5
    movdqu [rsp + 0xa0], xmm6
    movdqu [rsp + 0xb0], xmm7
    mov rdi, rdx
    mov rsi, [rbp + 8]
    movabs rax, 0x2222222222222222
    call rax
    mov rdi, [rsp]
    mov rsi, [rsp + 8]
    mov rdx, [rsp + 16]
    mov rcx, [rsp + 24]
    mov r8, [rsp + 32]
    mov r9, [rsp + 40]
    movdqu xmm0, [rsp + 0x40]
    movdqu xmm1, [rsp + 0x50]
    movdqu xmm2, [rsp + 0x60]
    movdqu xmm3, [rsp + 0x70]
    movdqu xmm4, [rsp + 0x80]
    movdqu xmm5, [rsp + 0x90]
    movdqu xmm6, [rsp + 0xa0]
    movdqu xmm7, [rsp + 0xb0]
    mov rsp, rbp
    pop rbp
    cmp eax, 2
    je 9f
    test eax, eax
    jz 8f
    mov rsi, rdx
    mov rdx, rcx
    jmp qword ptr [rip]
    .quad 0x3333333333333333
8:
    xor eax, eax
    ret
9:
    push rbp
    mov rbp, rsp
    push r15
input_entry_end:

# 0xe4063 in 0xe3910, the branch its move chose (rax) about to go into the input queue: a branch
# to a switch attack goes through spartanport_switch_queue, which may give another one.
.globl queue_site
queue_site:
    cmp byte ptr [rax + 0x30], 1
    jne 9f
    cmp byte ptr [rax + 0x31], 1
    je 7f
    cmp byte ptr [rax + 0x31], 4
    jne 9f
7:
    cmp byte ptr [rax + 0x2e], 0x0f
    jne 9f
    movzx edx, byte ptr [rax + 0x2f]
    sub edx, 0x10
    cmp edx, 3
    ja 9f
    lea rsp, [rsp - 128]
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 0x140
    mov [rsp], rcx
    mov [rsp + 8], rsi
    mov [rsp + 16], rdi
    mov [rsp + 24], r8
    mov [rsp + 32], r9
    mov [rsp + 40], r10
    mov [rsp + 48], r11
    movdqu [rsp + 0x40], xmm0
    movdqu [rsp + 0x50], xmm1
    movdqu [rsp + 0x60], xmm2
    movdqu [rsp + 0x70], xmm3
    movdqu [rsp + 0x80], xmm4
    movdqu [rsp + 0x90], xmm5
    movdqu [rsp + 0xa0], xmm6
    movdqu [rsp + 0xb0], xmm7
    movdqu [rsp + 0xc0], xmm8
    movdqu [rsp + 0xd0], xmm9
    movdqu [rsp + 0xe0], xmm10
    movdqu [rsp + 0xf0], xmm11
    movdqu [rsp + 0x100], xmm12
    movdqu [rsp + 0x110], xmm13
    movdqu [rsp + 0x120], xmm14
    movdqu [rsp + 0x130], xmm15
    mov rdi, rax
    movabs rax, 0x2222222222222222
    call rax
    mov rcx, [rsp]
    mov rsi, [rsp + 8]
    mov rdi, [rsp + 16]
    mov r8, [rsp + 24]
    mov r9, [rsp + 32]
    mov r10, [rsp + 40]
    mov r11, [rsp + 48]
    movdqu xmm0, [rsp + 0x40]
    movdqu xmm1, [rsp + 0x50]
    movdqu xmm2, [rsp + 0x60]
    movdqu xmm3, [rsp + 0x70]
    movdqu xmm4, [rsp + 0x80]
    movdqu xmm5, [rsp + 0x90]
    movdqu xmm6, [rsp + 0xa0]
    movdqu xmm7, [rsp + 0xb0]
    movdqu xmm8, [rsp + 0xc0]
    movdqu xmm9, [rsp + 0xd0]
    movdqu xmm10, [rsp + 0xe0]
    movdqu xmm11, [rsp + 0xf0]
    movdqu xmm12, [rsp + 0x100]
    movdqu xmm13, [rsp + 0x110]
    movdqu xmm14, [rsp + 0x120]
    movdqu xmm15, [rsp + 0x130]
    mov rsp, rbp
    pop rbp
    lea rsp, [rsp + 128]
9:
    mov edx, [rax + 8]
    test edx, 0x20000000
queue_site_end:

# 0x6b240, the d-pad weapon select (rdi his player stats): notes the stats.
.globl dpad_entry
dpad_entry:
    push r11
    movabs r11, 0x1111111111111111
    mov [r11 + 8], rdi
    pop r11
    push rbp
    mov rbp, rsp
    push rbx
    push rax
dpad_entry_end:
