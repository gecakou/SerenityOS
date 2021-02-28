/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
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

#include <AK/Badge.h>
#include <AK/ByteBuffer.h>
#include <AK/RefCounted.h>
#include <Kernel/FileSystem/FIFO.h>
#include <Kernel/FileSystem/Inode.h>
#include <Kernel/FileSystem/InodeMetadata.h>
#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/KBuffer.h>
#include <Kernel/VirtualAddress.h>

namespace Kernel {

class FileDescriptionData {
public:
    virtual ~FileDescriptionData() = default;
};

class FileDescription : public RefCounted<FileDescription> {
    MAKE_SLAB_ALLOCATED(FileDescription)
public:
    static KResultOr<NonnullRefPtr<FileDescription>> create(Custody&);
    static KResultOr<NonnullRefPtr<FileDescription>> create(File&);
    ~FileDescription();

    Thread::FileBlocker::BlockFlags should_unblock(Thread::FileBlocker::BlockFlags) const;

    bool is_readable() const { return m_readable; }
    bool is_writable() const { return m_writable; }

    void set_readable(bool b) { m_readable = b; }
    void set_writable(bool b) { m_writable = b; }

    void set_rw_mode(int options)
    {
        set_readable(options & O_RDONLY);
        set_writable(options & O_WRONLY);
    }

    KResult close();

    off_t seek(off_t, int whence);
    KResultOr<size_t> read(UserOrKernelBuffer&, size_t);
    KResultOr<size_t> write(const UserOrKernelBuffer& data, size_t);
    KResult stat(::stat&);

    KResult chmod(mode_t);

    bool can_read() const;
    bool can_write() const;

    ssize_t get_dir_entries(UserOrKernelBuffer& buffer, ssize_t);

    KResultOr<NonnullOwnPtr<KBuffer>> read_entire_file();

    String absolute_path() const;

    bool is_direct() const { return m_direct; }

    bool is_directory() const { return m_is_directory; }

    File& file() { return *m_file; }
    const File& file() const { return *m_file; }

    bool is_device() const;
    const Device* device() const;
    Device* device();

    bool is_tty() const;
    const TTY* tty() const;
    TTY* tty();

    bool is_master_pty() const;
    const MasterPTY* master_pty() const;
    MasterPTY* master_pty();

    InodeMetadata metadata() const;
    Inode* inode() { return m_inode.ptr(); }
    const Inode* inode() const { return m_inode.ptr(); }

    Custody* custody() { return m_custody.ptr(); }
    const Custody* custody() const { return m_custody.ptr(); }

    KResultOr<Region*> mmap(Process&, const Range&, size_t offset, int prot, bool shared);

    bool is_blocking() const { return m_is_blocking; }
    void set_blocking(bool b) { m_is_blocking = b; }
    bool should_append() const { return m_should_append; }
    void set_should_append(bool s) { m_should_append = s; }

    u32 file_flags() const { return m_file_flags; }
    void set_file_flags(u32);

    bool is_socket() const;
    Socket* socket();
    const Socket* socket() const;

    bool is_fifo() const;
    FIFO* fifo();
    FIFO::Direction fifo_direction() const { return m_fifo_direction; }
    void set_fifo_direction(Badge<FIFO>, FIFO::Direction direction) { m_fifo_direction = direction; }

    OwnPtr<FileDescriptionData>& data() { return m_data; }

    void set_original_inode(Badge<VFS>, NonnullRefPtr<Inode>&& inode) { m_inode = move(inode); }

    KResult truncate(u64);

    off_t offset() const { return m_current_offset; }

    KResult chown(uid_t, gid_t);

    FileBlockCondition& block_condition();

private:
    friend class VFS;
    explicit FileDescription(File&);

    KResult attach();

    void evaluate_block_conditions()
    {
        block_condition().unblock();
    }

    RefPtr<Custody> m_custody;
    RefPtr<Inode> m_inode;
    NonnullRefPtr<File> m_file;

    off_t m_current_offset { 0 };

    OwnPtr<FileDescriptionData> m_data;

    u32 m_file_flags { 0 };

    bool m_readable : 1 { false };
    bool m_writable : 1 { false };
    bool m_is_blocking : 1 { true };
    bool m_is_directory : 1 { false };
    bool m_should_append : 1 { false };
    bool m_direct : 1 { false };
    FIFO::Direction m_fifo_direction { FIFO::Direction::Neither };

    Lock m_lock { "FileDescription" };
};

}
