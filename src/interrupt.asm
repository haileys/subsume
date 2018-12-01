use32
global interrupt_init

%include "consts.asm"

%define IDT_SIZE 0x1000

interrupt_init:
    ; init PIC
    call pic_init

    ; build IDT:
    %define IDT_PRESENT 0x80
    %define IDT_INT32   0x0e
    ; ENTRY(offset, segment, type)
    %macro ENTRY 4
        mov eax, %2 ; offset lo
        mov [idt + ((%1) * 8) + 0], ax
        mov [idt + ((%1) * 8) + 2], word %3 ; segment
        mov [idt + ((%1) * 8) + 4], word (%4) << 8 ; type
        shr eax, 16 ; offset hi
        mov [idt + ((%1) * 8) + 6], ax
    %endmacro

    ENTRY 0x00, divide_by_zero,           SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x01, debug,                    SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x02, nmi,                      SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x03, breakpoint,               SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x04, overflow,                 SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x05, bound_range_exceeded,     SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x06, invalid_opcode,           SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x07, device_not_available,     SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x08, double_fault,             SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x0a, invalid_tss,              SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x0b, segment_not_present,      SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x0c, stack_segment_fault,      SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x0d, general_protection_fault, SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x0e, page_fault,               SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x10, x87_exception,            SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x11, alignment_check,          SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x12, machine_check,            SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x13, simd_exception,           SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x14, virtualization_exception, SEG_KCODE, IDT_PRESENT | IDT_INT32
    ENTRY 0x1e, security_exception,       SEG_KCODE, IDT_PRESENT | IDT_INT32
    %assign irq_num 0
    %rep 16
        ENTRY 0x20 + irq_num, irq%[irq_num], SEG_KCODE, IDT_PRESENT | IDT_INT32
        %assign irq_num irq_num + 1
    %endrep

    ; load IDT
    lidt [idtr]
    ret

%define PIC1 0x20
%define PIC2 0xa0
%define COMMAND 0
%define DATA    1

pic_init:
    ; save pic masks, PIC1 in BL and PIC2 in BH
    in al, PIC2 + DATA
    mov ah, al
    in al, PIC1 + DATA
    ; reinitialise PICs
    mov al, 0x11
    out PIC1 + COMMAND, al
    out PIC2 + COMMAND, al
    ; set interrupt vector offsets for PICs
    mov al, 0x20
    out PIC1 + DATA, al
    mov al, 0x28
    out PIC2 + DATA, al
    ;
    mov al, 0x04
    out PIC1 + DATA, al
    mov al, 0x02
    out PIC2 + DATA, al

    mov al, 0x01
    out PIC1 + DATA, al
    out PIC2 + DATA, al
    ; restore saved masks
    mov ax, bx
    out PIC1 + DATA, al
    mov al, ah
    out PIC2 + DATA, al
    ret

divide_by_zero:
    iret

debug:
    xchg bx, bx
    iret

nmi:
    xchg bx, bx
    iret

breakpoint:
    xchg bx, bx
    iret

overflow:
    xchg bx, bx
    iret

bound_range_exceeded:
    xchg bx, bx
    iret

invalid_opcode:
    xchg bx, bx
    iret

device_not_available:
    xchg bx, bx
    iret

double_fault:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

invalid_tss:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

segment_not_present:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

stack_segment_fault:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

general_protection_fault:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

page_fault:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

x87_exception:
    xchg bx, bx
    iret

alignment_check:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

machine_check:
    xchg bx, bx
    iret

simd_exception:
    xchg bx, bx
    iret

virtualization_exception:
    xchg bx, bx
    iret

security_exception:
    xchg bx, bx
    add esp, 4 ; pop error code from stack
    iret

; receives IRQ number on stack
irq:
    xchg bx, bx
    add esp, 4
    iret

; generate IRQ handlers
%assign irq_num 0
%rep 16
    irq%[irq_num]:
        push irq_num
        jmp irq
    %assign irq_num irq_num + 1
%endrep

align 4
idtr:
    dw IDT_SIZE - 1
    dd idt

section .bss
idt resb IDT_SIZE
