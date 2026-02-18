section .text
bits 64

extern task_rsp_offset
extern task_cr3_offset

extern fpu_state_component_bitmap

global context_switch
context_switch:
; * RDI, RSI, RDX are arguments (they are caller saved so no need to push them)
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push rbp

    mov rbx, qword [rel task_rsp_offset]
    ; $rdi = (uint64_t)old_tcb
    mov [rdi + rbx], rsp                ; rdi->rsp = $rsp

    ; $rsi = (uint64_t)next_tcb

    ; $rdx = ds

    mov rsp, [rsi + rbx]                ; $rsp = rsi->rsp

    mov rbx, qword [rel task_cr3_offset]

    mov rcx, cr3
    mov [rdi + rbx], rcx
    mov rax, [rsi + rbx]

    cmp rax, rcx
    je .end
    mov cr3, rax

.end:
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax

    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ret