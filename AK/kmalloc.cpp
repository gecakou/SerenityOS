#include <cstdio>
#include "SimpleMalloc.h"
#include <new>

extern "C" {

void* kcalloc(dword nmemb, dword size)
{
    if (!nmemb || !size)
        return nullptr;
    return SimpleMalloc::allocateZeroed(nmemb * size);
}

void* kmalloc(dword size)
{
    if (!size)
        return nullptr;
    return SimpleMalloc::allocate(size);
}

void kfree(void* ptr)
{
    if (!ptr)
        return;
    SimpleMalloc::free((byte*)ptr);
}

void* krealloc(void* ptr, dword size)
{
    if (!ptr)
        return ptr;
    return SimpleMalloc::reallocate((byte*)ptr, size);
}

}

void* operator new(std::size_t size)
{
    return kmalloc(size);
}

void* operator new[](std::size_t size)
{
    return kmalloc(size);
}

void operator delete(void* ptr)
{
    return kfree(ptr);
}

void operator delete[](void* ptr)
{
    return kfree(ptr);
}

void operator delete(void* ptr, size_t)
{
    return kfree(ptr);
}

void operator delete[](void* ptr, size_t)
{
    return kfree(ptr);
}

