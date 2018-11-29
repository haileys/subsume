use32
extern end
extern page_map
extern phys_alloc
extern phys_free_list
extern phys_next_free
extern temp_map
extern temp_unmap
extern textend

%define PAGESIZE 4096

; kernel is loaded at kernelbase = phys 0x00110000
; we need to:
;   * map kernelbase at 0xc0000000
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
    mov dword [initpdent], (physpd + PAGESIZE) + 0x03 ; present | rw
    mov ecx, 1024
    xor eax, eax
    rep stosd ; clear PT
    mov dword [initptent], physbase + 0x03 ; present | rw

    ; recursively map page directory
    mov dword [physpd + 1023 * 4], physpd + 0x03

    ; load esi with higher kernel base
    mov esi, 0xc0000000
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
    lea eax, [esi + physbase - 0xc0000000] ; convert virt to phys for kernel map
    or eax, 0x03 ; present | rw
    mov [ebx], eax ; set PT entry
    ; advance esi (virt addr) to next page
    add esi, PAGESIZE
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

    call allocvirt

    cli
    hlt

;
; virtual page allocation routines
;

; allocates a new virtual page in kernel space, returns in EAX
allocvirt:
    pushf
    cli
    ; read head of virtual free list
    mov eax, [_virtfreelist]
    test eax, eax
    jz .alloc
    ; if there is a page in the free list take it
    push eax
    mov eax, [eax]
    mov [_virtfreelist], eax
    pop eax
    popf
    ret
.alloc:
    push edx
    mov edx, [_virtnext]
    add dword [_virtnext], 0x1000
    push edx
    call phys_alloc
    pop edx

    ; save edx
    push edx

    push eax
    push edx
    call page_map
    add esp, 8

    pop edx

    mov eax, edx
    pop edx
    popf
    ret

; frees kernel page in EAX
freevirt:
    pushf
    cli
    push edx
    mov edx, [_virtfreelist]
    mov [eax], edx
    mov [_virtfreelist], eax
    pop edx
    popf
    ret

_virtfreelist   dd 0
_virtnext       dd end

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

; end of text section:
; textend     equ (0xc0000000 + ($ - init) + PAGESIZE) & ~0xfff

physbase    equ 0x00110000
initpdent   equ physpd + (physbase >> 22) * 4
initptent   equ physpd + PAGESIZE + (physbase >> 12) * 4
initlen     equ kernel - init
physpd      equ end - 0xc0000000 + physbase

section .bss

stackguard  resb 0x1000
stack       resb 0x1000
stackend    equ stack + 0x1000
global _temp_page
_temp_page  resb 0x1000
vram        resb 0x1000

; start of uninitialised variable section:
; stackguard  equ textend + PAGESIZE * 0 ; 4 KIB
; stack       equ textend + PAGESIZE * 1 ; 4 KiB
; stackend    equ textend + PAGESIZE * 2
; temppage    equ textend + PAGESIZE * 2 ; 4 KiB
; end         equ textend + PAGESIZE * 3
