%define STACK_SIZE 0x4000; 16KiB of memory for the stack.
%define MBOOT2_MAGIC 0x36d76289; Magic value that has to be in eax
%define HIGHER_HALF_ADDR 0xFFFFFFFF80000000

section .ltext
mboot_header_start:
    dd 0xe85250d6
    dd 0
    dd mboot_header_start - mboot_header_end
    dd 0x100000000 - (0xe85250d6 + (mboot_header_start - mboot_header_end))

    align 8
mboot_inforeq_start:
    dw 1
    dw 0
    dd mboot_inforeq_end - mboot_inforeq_start
    dd 2 ;Boot loader name
    dd 6 ;Memory map
    dd 7 ;Vbe info
    dd 8 ;Framebuffer info
    dd 14 ;Acpi 1.0 Rsdp info
    dd 15 ;Acpi 2.0+ Rsdp info
mboot_inforeq_end:

    align 8
mboot_end_start:
    dw 0
    dw 0
    dd mboot_end_end - mboot_end_start
mboot_end_end:

mboot_header_end:

[BITS 32]

extern early_setup

global _start
_start:
    cli

    cmp eax, MBOOT2_MAGIC
    jne no_boot
    
    push ebx
    
    call detect_cpuid
    call detect_lmode
    call check_NX_bit
    call check_syscalls

    xor ecx, ecx
    call map_PD
    call setup_paging
    call enable_paging
    
    push edi
    mov edi, 16

    call write_cr0

    mov edi, 7; Page-global enable
    call write_cr4

    pop edi

    pop ebx

    lgdt [lowgdt64.pointer]
    jmp lowgdt64.code:trampoline

detect_cpuid:
    pushfd
    pop eax
    mov ecx, eax; copy flags to ecx for comparing
    xor eax, (1 << 21) ; Flip id bit
    push eax
    popfd

    pushfd
    pop eax

    push ecx
    popfd

    xor eax, ecx
    jz no_boot

    mov eax, 0x80000000; is there an extended cpuid?
    cpuid
    cmp eax, 0x80000001; if eax < 0x80000001, there is extended cpuid. Thus, no lmode
    jb no_boot
    
    ret

detect_lmode:
    mov eax, 0x80000001; is there lmode?
    cpuid
    test edx, 1 << 29; If bit 29 is set, there is lmode
    jz no_boot

    ret

check_NX_bit:
    mov eax, 0x80000001; is NX bit available?
    cpuid
    test edx, 1 << 20; If bit 20 is set, NX is available
    jz no_boot ; We refuse to boot if the platform does not have NX-bit. (Kinda insecure, dont you think?)

    ret

check_syscalls:
    mov eax, 0x80000001; are syscalls available?
    cpuid
    test edx, 1 << 11; If bit 20 is set, NX is available
    jz no_boot ; We refuse to boot if the platform does not have syscalls.

    ret

map_PD:
    mov eax, 0x200000; 2MiB
    mul ecx; multiply eax(Offset in memory) by ecx(index/counter)
    or eax, 0b10000011; set present, r/w, and bit 7 which must be 1
    mov [PD + ecx * 8], eax
    ;inc ecx
    ;cmp ecx, 2; We only need a little bit of memory being mapped
    ;jne map_PD
    ret

setup_paging:
    mov eax, PD; move location of PD into eax
    or eax, 0b11; mark it as present and r/w
    mov [lowerPDPT], eax; move PD into first entry of PDPT
    mov [higherPDPT+(high_PDPT_idx*8)], eax; move PD into first entry of PDPT @higher_vma

    mov eax, lowerPDPT; move location of PDPT into eax
    or eax, 0b11; mark it ass present and r/w
    mov [PML4], eax; move PDPT into first entry of PML4

    mov eax, higherPDPT; move location of PDPT into eax
    or eax, 0b11; mark it ass present and r/w
    mov [PML4+(high_PML4_idx*8)], eax ; move PDPT into first entry of PML4 @higher_vma

    ret

enable_paging:
    mov eax, PML4; move location of PML4 into eax
    mov cr3, eax; load PML4 into cr3
    
    mov eax, cr4
    bts eax, 5; set PAE
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    bts eax, 8; enable long mode
    bts eax, 11; enable the NX
    wrmsr ; write to efer msr

    mov eax, cr0
    bts eax, 31; set paging
    mov cr0, eax
    ret

write_cr0:
    mov eax, cr0
    bts eax, edi
    mov cr0, eax

    ret

write_cr4:
    mov eax, cr4
    bts eax, edi
    mov cr4, eax

    ret

no_boot:
    cli
    hlt
    jmp no_boot ;Shouldnt execute, but....


[BITS 64]
trampoline:
    mov ax, lowgdt64.data
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rax, qword higher_half
    jmp rax


extern kentry

section .text
higher_half:
    mov rbp, qword stack
    mov rsp, qword stack_end

    mov rdi, HIGHER_HALF_ADDR
    add rdi, rbx

    call kentry

    cli
    hlt

section .lbss
align 4096; align to 4k
PML4: resb 4096; 512 entries of 8 bytes each, for a total of 4096 bytes.
lowerPDPT: resb 4096; same as above
higherPDPT: resb 4096; same as above
PD: resb 4096; same as above

section .lrodata
lowgdt64:
    dq 0
.code: equ $ - lowgdt64
    dw 0xFFFF
    dw 0
    dd 0x00209A00
.data: equ $ - lowgdt64
    dw 0xFFFF
    dw 0    
    dd 0x00009200
.pointer:
    dw $ - lowgdt64 - 1
    dq lowgdt64

section .bss
stack: resb STACK_SIZE
global stack_end
stack_end: