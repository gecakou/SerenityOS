/*
 * Copyright (c) 2020, Itamar S. <itamar8910@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "StackFrameUtils.h"

namespace Debug::StackFrameUtils {

Optional<StackFrameInfo> get_info(ProcessInspector const& inspector, FlatPtr current_ebp)
{
#if ARCH(X86_64)
    constexpr ptrdiff_t frame_pointer_return_address_offset = 8;
    constexpr ptrdiff_t frame_pointer_previous_frame_pointer_offset = 0;
#elif ARCH(AARCH64)
    constexpr ptrdiff_t frame_pointer_return_address_offset = 8;
    constexpr ptrdiff_t frame_pointer_previous_frame_pointer_offset = 0;
#elif ARCH(RISCV64)
    constexpr ptrdiff_t frame_pointer_return_address_offset = -8;
    constexpr ptrdiff_t frame_pointer_previous_frame_pointer_offset = -16;
#else
#    error Unknown architecture
#endif

    auto return_address = inspector.peek(current_ebp + frame_pointer_return_address_offset);
    auto next_ebp = inspector.peek(current_ebp + frame_pointer_previous_frame_pointer_offset);
    if (!return_address.has_value() || !next_ebp.has_value())
        return {};

    StackFrameInfo info = { return_address.value(), next_ebp.value() };
    return info;
}

}
