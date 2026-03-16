.section .text
.global _start
_start:
	mov %rsp, %rdi
	mov $main, %rsi
	andq $0xfffffffffffffff0, %rsp
	// pad with zeroes to tell the stack trace to end
	push $0
	push $0
	mov %rsp, %rbp
	call __mlibc_entry
.section .note.GNU-stack,"",%progbits
