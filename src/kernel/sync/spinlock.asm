[BITS 64]
section .text

[GLOBAL SPINLOCK_acquire_lock]
SPINLOCK_acquire_lock:
    lock bts [rdi], 0 ; Atomically test and set bit 0
    jc .try_acquire
    ret

.try_acquire:
    pause
    test dword [rdi], 1
    jnz .try_acquire
    jmp SPINLOCK_acquire_lock

[GLOBAL SPINLOCK_release_lock]
SPINLOCK_release_lock:
    mov dword [rdi], 0
    ret