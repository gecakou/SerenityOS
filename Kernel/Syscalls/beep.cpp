/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/CommandLine.h>
#if ARCH(I386) || ARCH(X86_64)
#    include <Kernel/Arch/x86/common/PCSpeaker.h>
#endif
#include <Kernel/Process.h>

namespace Kernel {

ErrorOr<FlatPtr> Process::sys$beep()
{
    VERIFY_NO_PROCESS_BIG_LOCK(this);
    if (!kernel_command_line().is_pc_speaker_enabled())
        return ENODEV;
#if ARCH(I386) || ARCH(X86_64)
    PCSpeaker::tone_on(440);
    auto result = TRY(Thread::current()->sleep(Time::from_nanoseconds(200'000'000)));
    PCSpeaker::tone_off();
    if (result.was_interrupted())
        return EINTR;
    return 0;
#else
    return ENOTIMPL;
#endif
}

}
