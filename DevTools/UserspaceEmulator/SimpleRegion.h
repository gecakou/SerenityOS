/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
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

#include "SoftMMU.h"

namespace UserspaceEmulator {

class SimpleRegion final : public SoftMMU::Region {
public:
    SimpleRegion(u32 base, u32 size);
    virtual ~SimpleRegion() override;

    virtual ValueWithShadow<u8> read8(u32 offset) override;
    virtual ValueWithShadow<u16> read16(u32 offset) override;
    virtual ValueWithShadow<u32> read32(u32 offset) override;
    virtual ValueWithShadow<u64> read64(u32 offset) override;

    virtual void write8(u32 offset, ValueWithShadow<u8>) override;
    virtual void write16(u32 offset, ValueWithShadow<u16>) override;
    virtual void write32(u32 offset, ValueWithShadow<u32>) override;
    virtual void write64(u32 offset, ValueWithShadow<u64>) override;

    u8* data() { return m_data; }
    u8* shadow_data() { return m_shadow_data; }

    virtual u8* cacheable_ptr(u32 offset) override;

private:
    u8* m_data { nullptr };
    u8* m_shadow_data { nullptr };
};

}
