/*
 * Copyright (c) 2020, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringView.h>
#include <Kernel/FileSystem/DevFS.h>
#include <Kernel/FileSystem/VirtualFileSystem.h>

namespace Kernel {

KResultOr<NonnullRefPtr<DevFS>> DevFS::try_create()
{
    return adopt_nonnull_ref_or_enomem(new (nothrow) DevFS);
}

DevFS::DevFS()
{
}

size_t DevFS::allocate_inode_index()
{
    MutexLocker locker(m_lock);
    m_next_inode_index = m_next_inode_index.value() + 1;
    VERIFY(m_next_inode_index > 0);
    return 1 + m_next_inode_index.value();
}

DevFS::~DevFS()
{
}

KResult DevFS::initialize()
{
    m_root_inode = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) DevFSRootDirectoryInode(*this)));
    return KSuccess;
}

Inode& DevFS::root_inode()
{
    return *m_root_inode;
}

DevFSInode::DevFSInode(DevFS& fs)
    : Inode(fs, fs.allocate_inode_index())
{
}

KResultOr<size_t> DevFSInode::read_bytes(off_t, size_t, UserOrKernelBuffer&, OpenFileDescription*) const
{
    VERIFY_NOT_REACHED();
}

KResult DevFSInode::traverse_as_directory(Function<bool(FileSystem::DirectoryEntryView const&)>) const
{
    VERIFY_NOT_REACHED();
}

KResultOr<NonnullRefPtr<Inode>> DevFSInode::lookup(StringView)
{
    VERIFY_NOT_REACHED();
}

void DevFSInode::flush_metadata()
{
}

KResultOr<size_t> DevFSInode::write_bytes(off_t, size_t, const UserOrKernelBuffer&, OpenFileDescription*)
{
    VERIFY_NOT_REACHED();
}

KResultOr<NonnullRefPtr<Inode>> DevFSInode::create_child(StringView, mode_t, dev_t, UserID, GroupID)
{
    return EROFS;
}

KResult DevFSInode::add_child(Inode&, const StringView&, mode_t)
{
    return EROFS;
}

KResult DevFSInode::remove_child(const StringView&)
{
    return EROFS;
}

KResult DevFSInode::chmod(mode_t)
{
    return EPERM;
}

KResult DevFSInode::chown(UserID, GroupID)
{
    return EPERM;
}

KResult DevFSInode::truncate(u64)
{
    return EPERM;
}

StringView DevFSLinkInode::name() const
{
    return m_name->view();
}

DevFSLinkInode::~DevFSLinkInode()
{
}

DevFSLinkInode::DevFSLinkInode(DevFS& fs, NonnullOwnPtr<KString> name)
    : DevFSInode(fs)
    , m_name(move(name))
{
}

KResultOr<size_t> DevFSLinkInode::read_bytes(off_t offset, size_t, UserOrKernelBuffer& buffer, OpenFileDescription*) const
{
    MutexLocker locker(m_inode_lock);
    VERIFY(offset == 0);
    VERIFY(m_link);
    TRY(buffer.write(m_link->characters() + offset, m_link->length()));
    return m_link->length();
}

InodeMetadata DevFSLinkInode::metadata() const
{
    InodeMetadata metadata;
    metadata.inode = { fsid(), index() };
    metadata.mode = S_IFLNK | 0555;
    metadata.uid = 0;
    metadata.gid = 0;
    metadata.size = 0;
    metadata.mtime = mepoch;
    return metadata;
}

KResultOr<size_t> DevFSLinkInode::write_bytes(off_t offset, size_t count, UserOrKernelBuffer const& buffer, OpenFileDescription*)
{
    auto new_string = TRY(buffer.try_copy_into_kstring(count));

    MutexLocker locker(m_inode_lock);
    VERIFY(offset == 0);
    VERIFY(buffer.is_kernel_buffer());
    m_link = move(new_string);
    return count;
}

DevFSDirectoryInode::DevFSDirectoryInode(DevFS& fs)
    : DevFSInode(fs)
{
}
DevFSDirectoryInode::~DevFSDirectoryInode()
{
}

InodeMetadata DevFSDirectoryInode::metadata() const
{
    InodeMetadata metadata;
    metadata.inode = { fsid(), 1 };
    metadata.mode = 0040555;
    metadata.uid = 0;
    metadata.gid = 0;
    metadata.size = 0;
    metadata.mtime = mepoch;
    return metadata;
}

DevFSRootDirectoryInode::DevFSRootDirectoryInode(DevFS& fs)
    : DevFSDirectoryInode(fs)
{
}

KResult DevFSRootDirectoryInode::traverse_as_directory(Function<bool(FileSystem::DirectoryEntryView const&)> callback) const
{
    MutexLocker locker(fs().m_lock);
    callback({ ".", identifier(), 0 });
    callback({ "..", identifier(), 0 });
    for (auto& node : m_nodes) {
        InodeIdentifier identifier = { fsid(), node.index() };
        callback({ node.name(), identifier, 0 });
    }
    return KSuccess;
}

KResultOr<NonnullRefPtr<Inode>> DevFSRootDirectoryInode::lookup(StringView name)
{
    MutexLocker locker(fs().m_lock);
    for (auto& node : m_nodes) {
        if (node.name() == name)
            return node;
    }
    return ENOENT;
}

KResult DevFSRootDirectoryInode::remove_child(const StringView& name)
{
    MutexLocker locker(fs().m_lock);
    for (auto& node : m_nodes) {
        if (node.name() == name) {
            m_nodes.remove(node);
            return KSuccess;
        }
    }
    return KResult(ENOENT);
}
KResultOr<NonnullRefPtr<Inode>> DevFSRootDirectoryInode::create_child(StringView name, mode_t mode, dev_t device_mode, UserID, GroupID)
{
    MutexLocker locker(fs().m_lock);

    for (auto& node : m_nodes) {
        if (node.name() == name)
            return KResult(EEXIST);
    }

    InodeMetadata metadata;
    metadata.mode = mode;
    if (metadata.is_directory()) {
        if (name != "pts")
            return EROFS;
        auto new_directory_inode = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) DevFSPtsDirectoryInode(fs())));
        m_nodes.append(*new_directory_inode);
        return new_directory_inode;
    }
    if (metadata.is_device()) {
        auto name_kstring = TRY(KString::try_create(name));
        unsigned major = major_from_encoded_device(device_mode);
        unsigned minor = minor_from_encoded_device(device_mode);
        auto new_device_inode = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) DevFSDeviceInode(fs(), major, minor, is_block_device(mode), move(name_kstring))));
        TRY(new_device_inode->chmod(mode));
        m_nodes.append(*new_device_inode);
        return new_device_inode;
    }
    if (metadata.is_symlink()) {
        auto name_kstring = TRY(KString::try_create(name));
        auto new_link_inode = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) DevFSLinkInode(fs(), move(name_kstring))));
        m_nodes.append(*new_link_inode);
        return new_link_inode;
    }
    return EROFS;
}

DevFSRootDirectoryInode::~DevFSRootDirectoryInode()
{
}
InodeMetadata DevFSRootDirectoryInode::metadata() const
{
    InodeMetadata metadata;
    metadata.inode = { fsid(), 1 };
    metadata.mode = 0040555;
    metadata.uid = 0;
    metadata.gid = 0;
    metadata.size = 0;
    metadata.mtime = mepoch;
    return metadata;
}

DevFSDeviceInode::DevFSDeviceInode(DevFS& fs, unsigned major_number, unsigned minor_number, bool block_device, NonnullOwnPtr<KString> name)
    : DevFSInode(fs)
    , m_name(move(name))
    , m_major_number(major_number)
    , m_minor_number(minor_number)
    , m_block_device(block_device)
{
}

DevFSDeviceInode::~DevFSDeviceInode()
{
}

KResult DevFSDeviceInode::chown(UserID uid, GroupID gid)
{
    MutexLocker locker(m_inode_lock);
    m_uid = uid;
    m_gid = gid;
    return KSuccess;
}

KResult DevFSDeviceInode::chmod(mode_t mode)
{
    MutexLocker locker(m_inode_lock);
    mode &= 0777;
    if (m_required_mode == mode)
        return KSuccess;
    m_required_mode = mode;
    return KSuccess;
}

StringView DevFSDeviceInode::name() const
{
    return m_name->view();
}

KResultOr<size_t> DevFSDeviceInode::read_bytes(off_t offset, size_t count, UserOrKernelBuffer& buffer, OpenFileDescription* description) const
{
    MutexLocker locker(m_inode_lock);
    VERIFY(!!description);
    RefPtr<Device> device = Device::get_device(m_major_number, m_minor_number);
    if (!device)
        return KResult(ENODEV);
    if (!device->can_read(*description, offset))
        return KResult(ENOTIMPL);
    auto result = const_cast<Device&>(*device).read(*description, offset, buffer, count);
    if (result.is_error())
        return result;
    return result.value();
}

InodeMetadata DevFSDeviceInode::metadata() const
{
    MutexLocker locker(m_inode_lock);
    InodeMetadata metadata;
    metadata.inode = { fsid(), index() };
    metadata.mode = (m_block_device ? S_IFBLK : S_IFCHR) | m_required_mode;
    metadata.uid = m_uid;
    metadata.gid = m_gid;
    metadata.size = 0;
    metadata.mtime = mepoch;
    metadata.major_device = m_major_number;
    metadata.minor_device = m_minor_number;
    return metadata;
}
KResultOr<size_t> DevFSDeviceInode::write_bytes(off_t offset, size_t count, const UserOrKernelBuffer& buffer, OpenFileDescription* description)
{
    MutexLocker locker(m_inode_lock);
    VERIFY(!!description);
    RefPtr<Device> device = Device::get_device(m_major_number, m_minor_number);
    if (!device)
        return KResult(ENODEV);
    if (!device->can_write(*description, offset))
        return KResult(ENOTIMPL);
    auto result = const_cast<Device&>(*device).write(*description, offset, buffer, count);
    if (result.is_error())
        return result;
    return result.value();
}

DevFSPtsDirectoryInode::DevFSPtsDirectoryInode(DevFS& fs)
    : DevFSDirectoryInode(fs)
{
}
KResult DevFSPtsDirectoryInode::traverse_as_directory(Function<bool(FileSystem::DirectoryEntryView const&)> callback) const
{
    MutexLocker locker(m_inode_lock);
    callback({ ".", identifier(), 0 });
    callback({ "..", identifier(), 0 });
    return KSuccess;
}

KResultOr<NonnullRefPtr<Inode>> DevFSPtsDirectoryInode::lookup(StringView)
{
    return ENOENT;
}

DevFSPtsDirectoryInode::~DevFSPtsDirectoryInode()
{
}
InodeMetadata DevFSPtsDirectoryInode::metadata() const
{
    InodeMetadata metadata;
    metadata.inode = { fsid(), index() };
    metadata.mode = 0040555;
    metadata.uid = 0;
    metadata.gid = 0;
    metadata.size = 0;
    metadata.mtime = mepoch;
    return metadata;
}

}
