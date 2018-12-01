%define KERNEL_BASE 0xc0000000
%define PAGE_SIZE 4096

%define GDT_PRESENT 0x80
%define GDT_DATA    0x10
%define GDT_CODE    0x18
%define GDT_TSS     0x00
%define GDT_WX      0x02
%define GDT_USER    0x60

%define SEG_KCODE   0x08
%define SEG_KDATA   0x10
%define SEG_UCODE   0x0b
%define SEG_UDATA   0x13
