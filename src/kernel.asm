use32
extern end
extern interrupt_init
extern page_map
extern phys_alloc
extern phys_free_list
extern phys_next_free
extern temp_map
extern temp_unmap
extern textend
extern virt_alloc
extern virt_free

%include "consts.asm"

; kernel is loaded at kernelbase = phys 0x00110000
; we need to:
;   * map kernelbase at KERNEL_BASE
;   * identity map first 1 Mib + 64 KiB
global init
init:
    ; zero page directory
    mov edi, physpd
    mov ecx, 1024
    xor eax, eax
    rep stosd
    ; edi is now physpd + PAGESIZE

    ; identity map current page of init
    mov dword [initpdent], (physpd + PAGE_SIZE) + 0x03 ; present | rw
    mov ecx, 1024
    xor eax, eax
    rep stosd ; clear PT
    mov dword [initptent], physbase + 0x03 ; present | rw

    ; recursively map page directory
    mov dword [physpd + 1023 * 4], physpd + 0x03

    ; load esi with higher kernel base
    mov esi, KERNEL_BASE
.mapkernel:
    mov edx, esi ; esi contains virt page to be mapped
    shr edx, 22 ; find PD index
    lea ebx, [physpd + edx * 4] ; find PD entry address
    mov edx, [ebx] ; load PD entry
    ; test PD entry existence
    test edx, edx
    jnz .ptexist
    ; build new page table:
    mov edx, edi ; load PT phys addr in ebx
    mov ecx, 1024
    xor eax, eax
    rep stosd ; clear PT, advance edi
    or edx, 0x03 ; present | rw
    mov [ebx], edx
.ptexist:
    and edx, ~0xfff ; remove page flags, extract PT phys addr
    mov eax, esi ; load virt page to be mapped in edx again
    shr eax, 12
    and eax, 0x3ff
    lea ebx, [edx + eax * 4] ; find PT entry address
    ; build PT entry
    lea eax, [esi + physbase - KERNEL_BASE] ; convert virt to phys for kernel map
    or eax, 0x03 ; present | rw
    mov [ebx], eax ; set PT entry
    ; advance esi (virt addr) to next page
    add esi, PAGE_SIZE
    ; loop around if there are more pages to be mapped (esi < end)
    cmp esi, end
    jb .mapkernel
.pgenable:
    ; set cr3 to PD phys addr
    mov eax, physpd
    mov cr3, eax
    ; enable paging
    mov eax, cr0
    or eax, 0x80000000 ; PG flag
    mov cr0, eax
    ; jump to mapped kernel
    mov eax, kernel
    jmp eax

%define PD 0xfffff000
%define PDE(addr) (0xfffff000 + ((addr) >> 22) * 4)
%define PT 0xffc00000
%define PTE(addr) (0xffc00000 + ((addr) >> 12) * 4)

kernel:
    ; nerf temp mapping of first init page via recursively mapped PD
    mov dword [PTE(physbase)], 0

    ; edi contains next physical page available for use
    ; save it in ebx
    mov ebx, edi

    ; move GDT to higher half
    lgdt [gdtr]

    ; zero out bss
    mov edi, textend
    mov ecx, end
    sub ecx, edi
    shr ecx, 2
    xor eax, eax
    rep stosd

    ; save next physical page in edi to phys_next_free now we've zeroed bss
    mov [phys_next_free], ebx

    ; unmap stack guard page
    mov eax, stackguard
    shr eax, 12
    mov dword [PT + eax * 4], 0

    ; set up kernel stack
    mov esp, stackend

    ; map vram to 0xb8000
    push 0xb8000
    push vram
    call page_map
    add esp, 8

    ; initialize interrupts
    call interrupt_init

    ; initialize TSS
    mov [tss + TSS_ESP0], esp
    mov ax, ss
    mov [tss + TSS_SS0], ax
    mov word [tss + TSS_IOPB], TSS_SIZE
    mov ax, SEG_TSS
    mov edx, tss
    ltr ax

    ; enable interrupts
    sti

L:
    hlt
    jmp L

    ; call virt_alloc

mainloop:
    hlt
    jmp mainloop

global zero_page
zero_page:
    push edi
    mov edi, [esp + 8]
    mov ecx, 1024
    xor eax, eax
    rep stosd
    pop edi
    ret

global panic
panic:
    mov edi, vram
    mov ah, 0x4f
    mov esi, .msg
.prefix:
    lodsb
    test al, al
    jz .prefix_done
    stosw
    jmp .prefix
.prefix_done:
    mov esi, [esp + 4]
.copy:
    lodsb
    test al, al
    jz .copy_done
    stosw
    jmp .copy
.copy_done:
    cli
    hlt
.msg db "PANIC: ", 0

global critical
critical:
    pushf
    pop eax
    bt eax, 9 ; IF
    salc ; set al if carry set
    xor al, 1
    movzx eax, al ; extend al to entire register
    ret

global critical_begin
critical_begin:
    call critical
    cli ; clear interrupt
    ret

global critical_end
critical_end:
    mov eax, [esp + 4]
    test eax, eax
    jz .return ; if IF was already 0, just return
    sti
.return:
    ret

physbase    equ 0x00110000
initpdent   equ physpd + (physbase >> 22) * 4
initptent   equ physpd + PAGE_SIZE + (physbase >> 12) * 4
initlen     equ kernel - init
physpd      equ end - KERNEL_BASE + physbase

gdtr:
    dw gdt.end - gdt - 1
.offset:
    dd gdt

gdt:
    ; entry 0x00 : null
    dq 0
    ; entry 0x08 : code
    dw 0xffff ; limit 0xfffff, 0:15
    dw 0x0000 ; base 0, 0:15
    db 0x00   ; base 0, 16:23
    db GDT_PRESENT | GDT_CODE | GDT_WX
    db 0xcf   ; 32 bit, 4 KiB granularity, limit 0xfffff 16:19
    db 0x00   ; base 0, 24:31
    ; entry 0x10 : data
    dw 0xffff ; limit 0xfffff, 0:15
    dw 0x0000 ; base 0, 0:15
    db 0x00   ; base 0, 16:23
    db GDT_PRESENT | GDT_DATA | GDT_WX
    db 0xcf   ; 32 bit, 4 KiB granularity, limit 0xfffff 16:19
    db 0x00   ; base 0, 24:31
    ; entry 0x18 : code
    dw 0xffff ; limit 0xfffff, 0:15
    dw 0x0000 ; base 0, 0:15
    db 0x00   ; base 0, 16:23
    db GDT_PRESENT | GDT_CODE | GDT_WX | GDT_USER
    db 0xcf   ; 32 bit, 4 KiB granularity, limit 0xfffff 16:19
    db 0x00   ; base 0, 24:31
    ; entry 0x20 : data
    dw 0xffff ; limit 0xfffff, 0:15
    dw 0x0000 ; base 0, 0:15
    db 0x00   ; base 0, 16:23
    db GDT_PRESENT | GDT_DATA | GDT_WX | GDT_USER
    db 0xcf   ; 32 bit, 4 KiB granularity, limit 0xfffff 16:19
    db 0x00   ; base 0, 24:31
    ; entry 0x28 ; tss
    dw TSS_SIZE & 0xffff ; limit 0:15
    dw (KERNEL_BASE + tss - bssbegin) & 0xffff ; base 0:15
    db (KERNEL_BASE + tss - bssbegin) >> 16 & 0xff ; base 16:23
    db 0x89 ; flags
    db 0x40 | ((TSS_SIZE >> 16) & 0x0f) ; 32 bit, 1 byte granularity, limit 16:19
    db (KERNEL_BASE + tss - bssbegin) >> 24 & 0xff ; base 24:31
.end:


section .bss
bssbegin equ $$
stackguard  resb 0x1000
stack       resb 0x1000
stackend    equ stack + 0x1000
global _temp_page
_temp_page  resb 0x1000
global vram
vram        resb 0x1000
tss         resb TSS_SIZE
