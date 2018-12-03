use16
org 0x100

%include "consts.asm"

    ; save necessary registers
    mov ax, cs
    mov [task + TASK_CS], ax
    mov ax, retn
    mov [task + TASK_IP], ax
    mov ax, ds
    mov [task + TASK_DS], ax
    mov ax, ss
    mov [task + TASK_SS], ax
    mov [task + TASK_SP], sp

    ; fetch memory map from BIOS
    mov di, memmap
    xor ebx, ebx
    mov edx, 0x534d4150
memloop:
    mov ecx, 24
    mov eax, 0xe820
    int 0x15
    jc .done
    add di, 24
    test ebx, ebx
    jnz memloop
.done:

    ; set up gdt offset in gdtr
    mov eax, ds
    shl eax, 4
    add eax, gdt
    mov [gdtr.offset], eax

    ; load gdt to prepare to enter protected mode
    cli
    lgdt [gdtr]

    ; enable protected mode
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; we can only far jump to a 16 bit IP while in 16 bit code, so we need to
    ; place a trampoline below the 64 KiB mark that will near jump to our
    ; protected mode code.

    ; load gs first so we can reference phys 0x0
    mov ax, SEG_KDATA
    mov gs, ax

    ; save value of 0x0 in eax
    mov eax, [gs:0]

    ; write trampoline to 0x0
    mov ebx, [trampoline]
    mov [gs:0], ebx

    ; calculate address of pmode and store in ebp
    mov ebp, cs
    shl ebp, 4
    add ebp, pmode

    ; jump to trampoline which will near jump to ebp
    jmp SEG_KCODE:0

; here is where the protected mode kernel will return back to when we're in
; VM8086 mode
retn:
    cli

    xor ax, ax
    mov es, ax
    mov word [es:8*4+0], pit
    mov ax, cs
    mov word [es:8*4+2], ax

    mov ax, 0xb800
    mov es, ax

    sti
    jmp $

pit:
    inc byte [es:0]
    mov al, 0x20
    out 0x20, al
    iret

gdtr:
    dw gdt.end - gdt - 1
.offset:
    dd 0

gdt:
    ; entry 0x00 : null
    dq 0
    ; entry 0x08 : code
    dw 0xffff ; limit 0xfffff, 0:15
    dw 0x0000 ; base 0, 0:15
    db 0x00   ; base 0, 16:23
    db 0x9a   ; present | code | rx
    db 0xcf   ; 32 bit, 4 KiB granularity, limit 0xfffff 16:19
    db 0x00   ; base 0, 24:31
    ; entry 0x10 : data
    dw 0xffff ; limit 0xfffff, 0:15
    dw 0x0000 ; base 0, 0:15
    db 0x00   ; base 0, 16:23
    db 0x92   ; present | data | rw
    db 0xcf   ; 32 bit, 4 KiB granularity, limit 0xfffff 16:19
    db 0x00   ; base 0, 24:31
.end:

; we are now in 32 bit mode and must be position independent
use32

%define ADDR(addr) (ebp + (addr) - pmode)

trampoline:
    jmp ebp
.end:

pmode:
    ; reload segment selectors
    mov bx, SEG_KDATA
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov ss, bx
    ; gs was already loaded earlier

    ; restore previous value of 0x0
    mov [0], eax

    ; find start of high
    lea esi, [ADDR(kernelcode)]
    mov edi, KERNEL_PHYS_BASE
    mov ecx, kernelcode.end - kernelcode
    rep movsb

    ; ebp still contains address of pmode
    ; add task offset to it so that kernel receives task data pointer in ebp
    add ebp, task - pmode

    mov eax, KERNEL_PHYS_BASE
    jmp eax

kernelcode:
    incbin "subsumek.bin"
.end:

align 4
task: times TASK_SIZE db 0
align 4
memmap:
