use32
org 0xc0000000

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
    mov dword [initpdent], (physpd + PAGESIZE) | 0x03 ; present | rw
    mov ecx, 1024
    xor eax, eax
    rep stosd ; clear PT
    mov dword [initptent], physbase | 0x03 ; present | rw

    ; recursively map page directory
    mov dword [physpd + 1023 * 4], physpd | 0x03

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
    rep stosw ; clear PT, advance edi
    or edx, 0x03 ; present | rw
    mov [ebx], edx
.ptexist:
    and edx, ~0xfff ; remove page flags, extract PT phys addr
    mov eax, esi ; load virt page to be mapped in edx again
    shr eax, 12
    and eax, 0x3ff ; find PT index
    lea ebx, [edx + eax * 4] ; find PT entry address
    ; build PT entry
    lea eax, [esi + physbase - 0xc0000000] ; convert virt to phys for kernel map
    or eax, 0x03 ; present | rw
    mov [ebx], eax ; set PT entry
    ; advance esi (virt addr) to next page
    add esi, PAGESIZE
    ; loop around if there are more pages to be mapped (edi < physinitend)
    cmp edi, physinitend
    jl .mapkernel
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

%define PDE(addr) (0xfffff000 + ((addr) >> 22) * 4)
%define PTE(addr) (0xffc00000 + ((addr) >> 12) * 4)

kernel:
    ; nerf temp mapping of first init page via recursively mapped PD
    mov dword [PTE(physbase)], 0

    cli
    hlt

end:

physbase    equ 0x00110000
initpdent   equ physpd + (physbase >> 22) * 4
initptent   equ physpd + PAGESIZE + ((physbase >> 12) & 0x3ff) * 4
initlen     equ kernel - init
physinitend equ physbase + PAGESIZE
physpd      equ physinitend

; times 8-(($-$$) % 8) db 0
