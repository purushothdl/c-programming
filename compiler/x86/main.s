	.file	"main.c"
	.text
	.globl	a
	.section .rdata,"dr"
	.align 4
a:
	.long	10
	.globl	massive_array
	.bss
	.align 32
massive_array:
	.space 400000
	.text
	.globl	square
	.def	square;	.scl	2;	.type	32;	.endef
square:
	pushq	%rbp
	movq	%rsp, %rbp
	movl	%ecx, 16(%rbp)
	movl	16(%rbp), %eax
	imull	%eax, %eax
	popq	%rbp
	ret
	.section .rdata,"dr"
.LC0:
	.ascii "The square of %d is %d\12\0"
.LC1:
	.ascii "The program is done\0"
	.text
	.globl	main
	.def	main;	.scl	2;	.type	32;	.endef
main:
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	call	__main
	movl	$10, %ecx
	call	square
	movl	%eax, %edx
	leaq	.LC0(%rip), %rax
	movl	%edx, %r8d
	movl	$10, %edx
	movq	%rax, %rcx
	call	printf
	leaq	.LC1(%rip), %rax
	movq	%rax, %rcx
	call	puts
	movl	$0, %eax
	leave
	ret
	.def	__main;	.scl	2;	.type	32;	.endef
	.ident	"GCC: (Rev8, Built by MSYS2 project) 15.2.0"
	.def	printf;	.scl	2;	.type	32;	.endef
	.def	puts;	.scl	2;	.type	32;	.endef
