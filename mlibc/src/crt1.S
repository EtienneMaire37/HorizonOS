.section .text
.global _start
_start:
	mov %rsp, %rdi
	mov $main, %rsi
	andq $0xfffffffffffffff0, %rsp
	mov %rsp, %rbp
	call __mlibc_entry
.section .note.GNU-stack,"",%progbits