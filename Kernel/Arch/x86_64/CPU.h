/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Leon Albrecht <leon2002.la@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/VirtualAddress.h>

namespace Kernel {

struct [[gnu::packed]] TSS64
{
    u32 __1; // Link?
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 __2; // IST0 -> empty?
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 __3;
    u16 __4;
    u16 iomapbase;
};

union [[gnu::packed]] Descriptor
{
    struct {
        u16 limit_lo;
        u16 base_lo;
        u8 base_hi;
        u8 type : 4;
        u8 descriptor_type : 1;
        u8 dpl : 2;
        u8 segment_present : 1;
        u8 limit_hi : 4;
        u8 : 1;
        u8 operation_size64 : 1;
        u8 operation_size32 : 1;
        u8 granularity : 1;
        u8 base_hi2;
    };
    struct {
        u32 low;
        u32 high;
    };

    enum Type {
        Invalid = 0,
        AvailableTSS_16bit = 0x1,
        LDT = 0x2,
        BusyTSS_16bit = 0x3,
        CallGate_16bit = 0x4,
        TaskGate = 0x5,
        InterruptGate_16bit = 0x6,
        TrapGate_16bit = 0x7,
        AvailableTSS_32bit = 0x9,
        BusyTSS_32bit = 0xb,
        CallGate_32bit = 0xc,
        InterruptGate_32bit = 0xe,
        TrapGate_32bit = 0xf,
    };

    VirtualAddress base() const
    {
        FlatPtr base = base_lo;
        base |= base_hi << 16u;
        base |= base_hi2 << 24u;
        return VirtualAddress { base };
    }

    void set_base(VirtualAddress base)
    {
        base_lo = base.get() & 0xffffu;
        base_hi = (base.get() >> 16u) & 0xffu;
        base_hi2 = (base.get() >> 24u) & 0xffu;
    }

    void set_limit(u32 length)
    {
        limit_lo = length & 0xffff;
        limit_hi = (length >> 16) & 0xf;
    }
};

}
