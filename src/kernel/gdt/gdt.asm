bits 64
section .text

global load_gdt
load_gdt:
    mov word [gdtr], di
    mov qword [gdtr + 2], rsi
    lgdt  [gdtr]
    ; jmp far 0x08:.reload_seg
    push qword 0x08
    push qword .reload_seg

    retfq

.reload_seg:    
    mov   ax, 0x10
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    mov   ss, ax
 
    ret

global load_tss
load_tss:
	mov ax, 0x28 
	ltr ax
	ret

section .data

gdtr: 
    dw 0
    dq 0