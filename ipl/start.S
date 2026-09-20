.section .text
.global _start

_start:
_setup_cpu:
    mov    %cr0, %rax
    and    $0xFFFB, %ax        # clear CR0.EM   (bit 2)  - disable FPU emulation
    or     $0x2, %ax           # set   CR0.MP   (bit 1)  - monitor coprocessor
    mov    %rax, %cr0

    mov    %cr4, %rax
    or     $(3 << 9), %ax      # set CR4.OSFXSR     (bit 9)  - enable FXSAVE/FXRSTOR + SSE
                               # set CR4.OSXMMEXCPT (bit 10) - handle SIMD FP exceptions
    mov    %rax, %cr4
_jmp_to_c:
	mov $_stack, %rsp
	sub $8, %rsp               # SysV ABI: rsp % 16 == 8 at function entry
	jmp _c_start
