section .text
bits 64

extern task_rsp_offset
extern task_cr3_offset

extern STATE_COMPONENT_BITMAP

; void __attribute__((sysv_abi)) context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds, uint8_t* old_fpu_state, uint8_t* next_fpu_state)
global context_switch
context_switch:
; * RDI, RSI, RDX, RCX, R8 are arguments (they are caller saved so no need to push them)
    push rax
    push rbx
    push rdx
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push rbp

    mov rax, [STATE_COMPONENT_BITMAP]
    mov rdx, 0
    ; $edx:$eax = all currently supported features

; * The implicit EDX:EAX register pair specifies a 64-bit instruction mask. 
; * The specific state components saved correspond to the bits set in the requested-feature bitmap (RFBM), 
; * the logicalAND of EDX:EAX and the logical-OR of XCR0 with the IA32_XSS MSR. 

    ; $rcx = old_fpu_state
    xsaves [rcx]

    ; $r8 = next_fpu_state
    xrstors [r8]

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
    pop rdx
    pop rbx
    pop rax

    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ret