/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/FileDescription.h>
#include <Kernel/Process.h>

namespace Kernel {

KResultOr<int> Process::sys$dup2(int old_fd, int new_fd)
{
    REQUIRE_PROMISE(stdio);
    auto description = fds().file_description(old_fd);
    if (!description)
        return EBADF;
    if (old_fd == new_fd)
        return new_fd;
    if (new_fd < 0 || static_cast<size_t>(new_fd) >= fds().max_open())
        return EINVAL;
    m_fds[new_fd].set(*description);
    return new_fd;
}

}
