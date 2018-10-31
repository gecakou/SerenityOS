#pragma once

void kmalloc_init();
void *kmalloc(DWORD size) __attribute__ ((malloc));
void* kmalloc_eternal(size_t) __attribute__ ((malloc));
void kfree(void*);

bool is_kmalloc_address(void*);

extern volatile DWORD sum_alloc;
extern volatile DWORD sum_free;
extern volatile dword kmalloc_sum_eternal;

inline void* operator new(size_t, void* p) { return p; }
inline void* operator new[](size_t, void* p) { return p; }
