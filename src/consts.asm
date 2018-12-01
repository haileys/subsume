%define KERNEL_BASE 0xc0000000
%define PAGE_SIZE 4096

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
