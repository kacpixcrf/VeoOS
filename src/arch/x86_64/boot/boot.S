.section .multiboot, "a"
.balign 8
.global uefi_header_magic
uefi_header_magic:
    .ascii "VEOEFI64"
    .quad uefi_entry

.global uefi_mb_info
.balign 64
uefi_mb_info:
    .skip 128

.section .text
.code64
.global uefi_entry
.type uefi_entry, @function
uefi_entry:
    cld

    movq %cr0, %rax
    btrq $16, %rax
    movq %rax, %cr0

    movq $stack_top, %rsp
    movq %rcx, %rdi
    sti
    call kernel_main

4:
    cli
    hlt
    jmp 4b

.section .bss
.balign 16
stack_bottom:
    .skip 65536
.balign 16
stack_top:
