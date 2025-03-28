section .text

%macro swapgs_if_necessary 0
    cmp BYTE [rsp+8], 0x8 ; Is the previous CPL = Kernel Code? (Offset 24 because there are two pushq before isr_common + the padding for the SS)
    je %%skip ; If so, there is no need to swapgs
    swapgs

%%skip:
%endmacro

%macro ISR_NOERROR 1
[GLOBAL ISR_%1]
    swapgs_if_necessary
    push QWORD %1
    push QWORD 0
    jmp isr_common
%endmacro

%macro ISR_ERROR 1
[GLOBAL ISR_%1]
    swapgs_if_necessary
    push QWORD %1
    jmp isr_common
%endmacro

%macro IRQ 1
[GLOBAL IRQ_%1]
    swapgs_if_necessary
    push QWORD 0
    push QWORD %1
    jmp irq_common
%endmacro

ISR_0: ISR_NOERROR 0 ;Division By zero
ISR_1: ISR_NOERROR 1 ;Debug
ISR_2: ISR_NOERROR 2 ;NMI
ISR_3: ISR_NOERROR 3 ;Breakpoint
ISR_4: ISR_NOERROR 4 ;Overflow
ISR_5: ISR_NOERROR 5 ;Bound Range exceded
ISR_6: ISR_NOERROR 6 ;Invalid opcode
ISR_7: ISR_NOERROR 7 ;Device Not available
ISR_8: ISR_ERROR 8 ;Double Fault
; Isr 9 - Coprocessor segment overrun (does not exist on x86_64)
ISR_10: ISR_ERROR 10 ;Invalid TSS
ISR_11: ISR_ERROR 11 ;Segment Not Present
ISR_12: ISR_ERROR 12 ;Stack-Segment fault
ISR_13: ISR_ERROR 13 ;General Protection Fault
ISR_14: ISR_ERROR 14 ;Page Fault
; Isr 15 is reserved
ISR_16: ISR_NOERROR 16 ;x87 FPU error
ISR_17: ISR_ERROR 17 ;Alignment Check
ISR_18: ISR_NOERROR 18 ;Machine check
ISR_19: ISR_NOERROR 19 ;SIMD FP Exception
ISR_20: ISR_NOERROR 20 ;Virtualization Exception
ISR_21: ISR_ERROR 21 ;Control Protection Exception
; Isrs 22 thorugh 27 are reserved
ISR_28: ISR_NOERROR 28 ;Hypervisor Injection Exception
ISR_29: ISR_ERROR 29 ;VMM Communication Exception
ISR_30: ISR_ERROR 30 ;Security exception
; ISR 31 is reserved

IRQ_0: IRQ 0 ; Entry 32 in the IDT
IRQ_1: IRQ 1 ; Entry 33 in the IDT
IRQ_2: IRQ 2 ; Entry 34 in the IDT
IRQ_3: IRQ 3 ; Entry 35 in the IDT
IRQ_4: IRQ 4 ; Entry 36 in the IDT
IRQ_5: IRQ 5 ; Entry 37 in the IDT
IRQ_6: IRQ 6 ; Entry 38 in the IDT
IRQ_7: IRQ 7 ; Entry 39 in the IDT
IRQ_8: IRQ 8 ; Entry 40 in the IDT
IRQ_9: IRQ 9 ; Entry 41 in the IDT
IRQ_10: IRQ 10 ; Entry 42 in the IDT
IRQ_11: IRQ 11 ; Entry 43 in the IDT
IRQ_12: IRQ 12 ; Entry 44 in the IDT
IRQ_13: IRQ 13 ; Entry 45 in the IDT
IRQ_14: IRQ 14 ; Entry 46 in the IDT
IRQ_15: IRQ 15 ; Entry 48 in the IDT

extern isr_handler
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16 ; Clear out the two pushq's done before (pushq and error code in case of ISR_ERROR)

    swapgs_if_necessary
    iretq

extern irq_handler
irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16 ; Clear out the two pushq's done before

    swapgs_if_necessary
    iretq