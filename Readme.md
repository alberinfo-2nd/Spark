Spark aims to be a lightweight x86_64 OS with a simple microkernel and reliable security features.

*Memory Layout*

From 0x0 to 0xfffff -> Restricted, use for grub and other reserved memory areas
From 0x100000 to 0x7fffffffffff -> Lower half. Used for user-space and (maybe) MMIO
From 0xffff800000000000 to 0xffffffff7fffffff -> Higer Half; Kernel heap, MMIO and other supervisor-mode memory allocs
From 0xffffffff80000000 to 0xffffffffffffffff -> Kernel Higher Half; Has the .code, .data and other sections of the kernel. Initial kernel stack and other basic and necessary resources are in this zone


*PMM*
The PMM serves as a backer for the VMM. This means that it can only allocate 4KiB pages (or multiples of).

Its a simple lockless bitmap, where each CPU starts with the same amount of memory, and can dynamically increase its memory pool by requesting it from another CPU when needed