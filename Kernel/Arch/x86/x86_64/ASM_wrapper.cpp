/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>

#include <Kernel/Arch/x86/ASM_wrapper.h>
#include <Kernel/Arch/x86/CPU.h>
#include <Kernel/Arch/x86/Processor.h>

namespace Kernel {

UNMAP_AFTER_INIT void write_cr0(FlatPtr value)
{
    asm volatile("mov %%rax, %%cr0" ::"a"(value));
}

UNMAP_AFTER_INIT void write_cr4(FlatPtr value)
{
    asm volatile("mov %%rax, %%cr4" ::"a"(value));
}
FlatPtr read_cr0()
{
    FlatPtr cr0;
    asm("mov %%cr0, %%rax"
        : "=a"(cr0));
    return cr0;
}

FlatPtr read_cr2()
{
    FlatPtr cr2;
    asm("mov %%cr2, %%rax"
        : "=a"(cr2));
    return cr2;
}

FlatPtr read_cr3()
{
    FlatPtr cr3;
    asm("mov %%cr3, %%rax"
        : "=a"(cr3));
    return cr3;
}

void write_cr3(FlatPtr cr3)
{
    // NOTE: If you're here from a GPF crash, it's very likely that a PDPT entry is incorrect, not this!
    asm volatile("mov %%rax, %%cr3" ::"a"(cr3)
                 : "memory");
}

FlatPtr read_cr4()
{
    FlatPtr cr4;
    asm("mov %%cr4, %%rax"
        : "=a"(cr4));
    return cr4;
}

#define DEFINE_DEBUG_REGISTER(index)                         \
    FlatPtr read_dr##index()                                 \
    {                                                        \
        FlatPtr value;                                       \
        asm("mov %%dr" #index ", %%rax"                      \
            : "=a"(value));                                  \
        return value;                                        \
    }                                                        \
    void write_dr##index(FlatPtr value)                      \
    {                                                        \
        asm volatile("mov %%rax, %%dr" #index ::"a"(value)); \
    }

DEFINE_DEBUG_REGISTER(0);
DEFINE_DEBUG_REGISTER(1);
DEFINE_DEBUG_REGISTER(2);
DEFINE_DEBUG_REGISTER(3);
DEFINE_DEBUG_REGISTER(6);
DEFINE_DEBUG_REGISTER(7);

}
