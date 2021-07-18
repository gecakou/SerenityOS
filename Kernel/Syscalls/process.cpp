/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Process.h>

namespace Kernel {

KResultOr<FlatPtr> Process::sys$getpid()
{
    VERIFY_NO_PROCESS_BIG_LOCK(this)
    REQUIRE_PROMISE(stdio);
    return pid().value();
}

KResultOr<FlatPtr> Process::sys$getppid()
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    REQUIRE_PROMISE(stdio);
    return m_ppid.value();
}

KResultOr<FlatPtr> Process::sys$get_process_name(Userspace<char*> buffer, size_t buffer_size)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    REQUIRE_PROMISE(stdio);
    if (m_name.length() + 1 > buffer_size)
        return ENAMETOOLONG;

    if (!copy_to_user(buffer, m_name.characters(), m_name.length() + 1))
        return EFAULT;
    return 0;
}

KResultOr<FlatPtr> Process::sys$set_process_name(Userspace<const char*> user_name, size_t user_name_length)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    REQUIRE_PROMISE(proc);
    if (user_name_length > 256)
        return ENAMETOOLONG;
    auto name = copy_string_from_user(user_name, user_name_length);
    if (name.is_null())
        return EFAULT;
    // Empty and whitespace-only names only exist to confuse users.
    if (name.is_whitespace())
        return EINVAL;
    m_name = move(name);
    return 0;
}

KResultOr<FlatPtr> Process::sys$set_coredump_metadata(Userspace<const Syscall::SC_set_coredump_metadata_params*> user_params)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    Syscall::SC_set_coredump_metadata_params params;
    if (!copy_from_user(&params, user_params))
        return EFAULT;
    if (params.key.length == 0 || params.key.length > 16 * KiB)
        return EINVAL;
    if (params.value.length > 16 * KiB)
        return EINVAL;
    auto copied_key = copy_string_from_user(params.key.characters, params.key.length);
    if (copied_key.is_null())
        return EFAULT;
    auto copied_value = copy_string_from_user(params.value.characters, params.value.length);
    if (copied_value.is_null())
        return EFAULT;
    if (!m_coredump_metadata.contains(copied_key) && m_coredump_metadata.size() >= 16)
        return EFAULT;
    m_coredump_metadata.set(move(copied_key), move(copied_value));
    return 0;
}

}
