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
extern virt_to_phys

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
    ; page directory entry needs to be user accessible for later
    mov dword [initpdent], (physpd + PAGE_SIZE) + (PAGE_PRESENT | PAGE_RW | PAGE_USER)
    mov ecx, 1024
    xor eax, eax
    rep stosd ; clear PT
    mov dword [initptent], KERNEL_PHYS_BASE + (PAGE_PRESENT | PAGE_RW)

    ; recursively map page directory
    mov dword [physpd + 1023 * 4], physpd + (PAGE_PRESENT | PAGE_RW)

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
    lea eax, [esi + KERNEL_PHYS_BASE - KERNEL_BASE] ; convert virt to phys for kernel map
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
    ; edi contains next physical page available for use
    ; save it in ebx
    mov ebx, edi

    ; nerf temp mapping of first init page via recursively mapped PD
    mov dword [PTE(KERNEL_PHYS_BASE)], 0

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

    ; ebp still contains phys pointer to task data set up for us by early loader
    mov [task_phys], ebp

    ; unmap stack guard page
    mov eax, stackguard
    shr eax, 12
    mov dword [PT + eax * 4], 0

    ; set up kernel stack
    mov esp, stackend

    ; initialize interrupts
    call interrupt_init

    ; initialize TSS
    mov [tss + TSS_ESP0], esp
    mov ax, ss
    mov [tss + TSS_SS0], ax
    mov word [tss + TSS_IOPB], TSS_SIZE
    ; set TSS base in GDT
    mov edx, tss
    mov [gdt.tss_base_0_15], dx
    shr edx, 16
    mov [gdt.tss_base_16_23], dl
    mov [gdt.tss_base_24_31], dh
    ; load TSS
    mov ax, SEG_TSS
    ltr ax

    ; set up 1 MiB of memory for VM86 task
    ; just identity map to low memory for now
    xor edi, edi
.vm86_alloc_loop:
    ; allocate and map page
    push PAGE_RW | PAGE_USER
    mov eax, edi
    and eax, 0xfffff ; emulate disabled A20 line
    push eax
    push edi
    call page_map
    add esp, 12
.next:
    add edi, 0x1000
    cmp edi, 0x00110000
    jb .vm86_alloc_loop

    ; task_phys is guaranteed to point to low memory which is identity mapped
    mov ebx, [task_phys]

    ; set up iret stack in preparation for VM8086 switch
    movzx eax, word [ebx + TASK_DS]
    push 0          ; GS
    push 0          ; FS
    push eax        ; DS
    push eax        ; ES
    movzx eax, word [ebx + TASK_SS]
    push eax        ; SS
    movzx eax, word [ebx + TASK_SP]
    push eax        ; ESP
    pushf
    pop eax
    and eax, 0xffff
    or eax, FLAG_INTERRUPT | FLAG_VM8086
    push eax        ; EFLAGS
    movzx eax, word [ebx + TASK_CS]
    push eax        ; CS
    movzx eax, word [ebx + TASK_IP]
    push eax        ; EIP

    ; clear general registers
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor ebp, ebp
    xor esi, esi
    xor edi, edi

    ; switch to VM8086
    iret

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
    mov edi, 0xb8000
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
    xor eax, 1 ; critical returns 1 if IF off, invert to get IF state
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

initpdent   equ physpd + (KERNEL_PHYS_BASE >> 22) * 4
initptent   equ physpd + PAGE_SIZE + (KERNEL_PHYS_BASE >> 12) * 4
initlen     equ kernel - init
physpd      equ end - KERNEL_BASE + KERNEL_PHYS_BASE

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
.tss_base_0_15:
    dw 0 ; base 0:15
.tss_base_16_23:
    db 0 ; base 16:23
    db 0x89 ; flags
    db 0x40 | ((TSS_SIZE >> 16) & 0x0f) ; 32 bit, 1 byte granularity, limit 16:19
.tss_base_24_31:
    db 0 ; base 24:31
.end:

section .bss
stackguard  resb 0x1000
stack       resb 0x1000
stackend    equ stack + 0x1000
global _temp_page
_temp_page  resb 0x1000
align 4
tss         resb TSS_SIZE
align 4
task_phys   resb 4
align 4
task        resb TASK_SIZE
