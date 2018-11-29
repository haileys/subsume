use32
global init
global critical_begin
global critical_end
extern textend
extern end

%define PAGESIZE 4096

; kernel is loaded at kernelbase = phys 0x00110000
; we need to:
;   * map kernelbase at 0xc0000000
;   * identity map first 1 Mib + 64 KiB
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
    mov [_physnext], edi

    ; zero out bss
    mov edi, textend
    mov ecx, end
    sub ecx, edi
    shr ecx, 2
    xor eax, eax
    rep stosw

    ; unmap stack guard page
    mov eax, stackguard
    shr eax, 12
    mov dword [PT + eax * 4], 0

    ; set up kernel stack
    mov esp, stackend

    call allocphys

    cli
    hlt

;
; physical page allocation routines
;

; allocates physical page and returns physical address in EAX
allocphys:
    pushf
    cli
    ; see if there's a free physical page available in the free list:
    mov eax, [_physfreelist]
    test eax, eax
    jz .alloc
    ; if there is, map it at the temp address
    push eax
    call tempmap
    ; read pointer to next free phys page and store at free list head:
    mov eax, [temppage]
    mov [_physfreelist], eax
    ; zero phys page
    push edi
    push ecx
    mov ecx, 1024
    mov edi, temppage
    xor eax, eax
    rep stosd
    pop ecx
    pop edi
    ; remove temp mapping
    call tempunmap
    ; pop phys addr and return
    pop eax
    popf
    ret
.alloc:
    ; no phys pages are in the freelist, need to allocate a new one
    mov eax, [_physnext]
    add dword [_physnext], 4096
    ; zero new phys page
    push eax
    call tempmap
    push edi
    push ecx
    mov edi, temppage
    mov ecx, 1024
    xor eax, eax
    rep stosd
    pop ecx
    pop edi
    call tempunmap
    pop eax
    popf
    ret

; frees physical page for address given in EAX
freephys:
    pushf
    cli
    ; temp map physical page
    push eax
    call tempmap
    ; write current free list head to first dword of page
    mov eax, [_physfreelist]
    mov [temppage], eax
    ; unmap temp page
    call tempunmap
    ; store just freed page at free list head
    pop eax
    mov [_physfreelist], eax
    popf
    ret

_physfreelist:   dd 0
_physnext:       dd 0

;
; page mapping routines
;

; map the temp page to a physical address given in EAX
tempmap:
    push edx
    or eax, 0x03 ; present | rw
    mov edx, temppage
    shr edx, 12
    mov [PT + edx * 4], eax
    invlpg [temppage]
    pop edx
    ret

; unmap the temp page
tempunmap:
    push edx
    mov edx, temppage
    shr edx, 12
    mov dword [PT + edx * 4], 0
    invlpg [temppage]
    pop edx
    ret

; map the virtual page given by EDX to the physical page given by EAX
pagemap:
    pushf
    cli
    ; first check if there is a page directory mapping existing for EDX
    push edx
    shr edx, 22
    cmp dword [PD + edx * 4], 0
    jne .ptexist
    ; we need to allocate a page table and set the PDE
    push eax
    call allocphys
    or eax, 0x03 ; rw | present
    mov [PD + edx], eax
    pop eax
    ; invalidate page table
    mov edx, [esp] ; restore EDX without popping
    shr edx, 12
    invlpg [PT + edx * 4]
    jmp .map
.ptexist:
    mov edx, [esp] ; restore EDX without popping
    shr edx, 12
.map:
    or eax, 0x03 ; rw | present
    mov [PT + edx * 4], eax
    pop edx
    invlpg [edx]
    popf
    ret

; unmap the virtual page given by EAX
; returns physical page in EAX for freeing
pageunmap:
    pushf
    cli
    push edx
    xor edx, edx
    shr eax, 12
    xchg edx, [PT + eax * 4]
    and edx, ~0xfff ; clear any flags
    mov eax, edx
    pop edx
    popf
    ret

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
    call allocphys
    call pagemap
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

critical_begin:
    pushf
    pop eax
    bt eax, 9 ; IF
    salc ; set al if carry set
    movzx eax, al ; extend al to entire eax register
    cli ; clear interrupt
    ret

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
temppage    resb 0x1000

; start of uninitialised variable section:
; stackguard  equ textend + PAGESIZE * 0 ; 4 KIB
; stack       equ textend + PAGESIZE * 1 ; 4 KiB
; stackend    equ textend + PAGESIZE * 2
; temppage    equ textend + PAGESIZE * 2 ; 4 KiB
; end         equ textend + PAGESIZE * 3
