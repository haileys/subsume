%define KERNEL_BASE         0xc0000000
%define KERNEL_PHYS_BASE    0x00110000
%define PAGE_SIZE           0x00001000

%define GDT_PRESENT 0x80
%define GDT_DATA    0x10
%define GDT_CODE    0x18
%define GDT_TSS     0x09
%define GDT_WX      0x02
%define GDT_USER    0x60

%define SEG_KCODE   0x08
%define SEG_KDATA   0x10
%define SEG_UCODE   0x1b
%define SEG_UDATA   0x23
%define SEG_TSS     0x28

%define TSS_ESP0    0x04
%define TSS_SS0     0x08
%define TSS_IOPB    0x66
%define TSS_SIZE    104

%define PAGE_PRESENT    0x001
%define PAGE_RW         0x002
%define PAGE_USER       0x004
%define PAGE_FLAGS      0xfff

%define FLAG_INTERRUPT  (1 << 9)
%define FLAG_VM8086     (1 << 17)

%define TASK_SIZE       (2 * 5)
%define TASK_CS         0
%define TASK_IP         2
%define TASK_DS         4
%define TASK_SS         6
%define TASK_SP         8

%define REALDATA_FONT       0                           ; size = 4096
%define REALDATA_VBE_INFO   (REALDATA_FONT + 4096)      ; size = 512
%define REALDATA_TASK       (REALDATA_VBE_INFO + 512)   ; size = TASK_SIZE
%define REALDATA_MEMMAP     (REALDATA_TASK + TASK_SIZE) ; size indeterminate
