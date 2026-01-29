.section .text
.global _start

_start:
	xor %rax, %rax
_loop:
	movb 0x1001, %r8b
	cmpb $0, %r8b
	jz _loop
	movb 0x1000, %r8b
	movb %r8b, 0x1000
	jmp _loop
