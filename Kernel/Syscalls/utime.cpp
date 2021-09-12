/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/Process.h>

namespace Kernel {

KResultOr<FlatPtr> Process::sys$utime(Userspace<const char*> user_path, size_t path_length, Userspace<const struct utimbuf*> user_buf)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    REQUIRE_PROMISE(fattr);
    auto path = TRY(get_syscall_path_argument(user_path, path_length));
    utimbuf buf;
    if (user_buf) {
        TRY(copy_from_user(&buf, user_buf));
    } else {
        auto now = kgettimeofday().to_truncated_seconds();
        // Not a bug!
        buf = { now, now };
    }
    return VirtualFileSystem::the().utime(path->view(), current_directory(), buf.actime, buf.modtime);
}

}
