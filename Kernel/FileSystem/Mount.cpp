/*
 * Copyright (c) 2018-2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/Custody.h>
#include <Kernel/FileSystem/FileSystem.h>
#include <Kernel/FileSystem/Inode.h>
#include <Kernel/FileSystem/Mount.h>

namespace Kernel {

Mount::Mount(NonnullRefPtr<Inode> source, int flags)
    : m_guest_fs(source->fs())
    , m_guest(move(source))
    , m_flags(flags)
{
}

Mount::Mount(NonnullRefPtr<Inode> source, NonnullRefPtr<Custody> host_custody, int flags)
    : m_guest_fs(source->fs())
    , m_guest(move(source))
    , m_host_custody(move(host_custody))
    , m_flags(flags)
{
    if (flags & MS_IMMUTABLE)
        m_immutable.set();
}

void Mount::set_flags(int flags)
{
    if (flags & MS_IMMUTABLE)
        m_immutable.set();
    m_flags = flags;
}

void Mount::delete_mount_from_list(Mount& mount)
{
    dbgln("VirtualFileSystem: Unmounting file system {}...", mount.guest_fs().fsid());
    VERIFY(mount.m_vfs_list_node.is_in_list());
    mount.m_vfs_list_node.remove();
    delete &mount;
}

ErrorOr<NonnullOwnPtr<KString>> Mount::absolute_path() const
{
    if (!m_host_custody)
        return KString::try_create("/"sv);
    return m_host_custody->try_serialize_absolute_path();
}

RefPtr<Inode> Mount::host()
{
    if (!m_host_custody)
        return nullptr;
    return m_host_custody->inode();
}

RefPtr<Inode const> Mount::host() const
{
    if (!m_host_custody)
        return nullptr;
    return m_host_custody->inode();
}

RefPtr<Custody const> Mount::host_custody() const
{
    return m_host_custody;
}

RefPtr<Custody> Mount::host_custody()
{
    return m_host_custody;
}

}
