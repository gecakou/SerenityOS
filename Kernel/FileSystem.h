#pragma once

#include "DiskDevice.h"
#include "InodeIdentifier.h"
#include "InodeMetadata.h"
#include "Limits.h"
#include "UnixTypes.h"
#include <AK/ByteBuffer.h>
#include <AK/HashMap.h>
#include <AK/OwnPtr.h>
#include <AK/Retainable.h>
#include <AK/RetainPtr.h>
#include <AK/AKString.h>
#include <AK/Function.h>
#include <AK/kstdio.h>
#include <AK/Lock.h>

static const dword mepoch = 476763780;

class Inode;
class FileDescriptor;
class VMObject;

class FS : public Retainable<FS> {
public:
    static void initialize_globals();
    virtual ~FS();

    unsigned fsid() const { return m_fsid; }
    static FS* from_fsid(dword);
    static void sync();

    virtual bool initialize() = 0;
    virtual const char* class_name() const = 0;
    virtual InodeIdentifier root_inode() const = 0;

    bool is_readonly() const { return m_readonly; }

    struct DirectoryEntry {
        DirectoryEntry(const char* name, InodeIdentifier, byte fileType);
        DirectoryEntry(const char* name, size_t name_length, InodeIdentifier, byte fileType);
        char name[256];
        size_t name_length { 0 };
        InodeIdentifier inode;
        byte fileType { 0 };
    };

    virtual RetainPtr<Inode> create_inode(InodeIdentifier parentInode, const String& name, mode_t, unsigned size, int& error) = 0;
    virtual RetainPtr<Inode> create_directory(InodeIdentifier parentInode, const String& name, mode_t, int& error) = 0;

    virtual RetainPtr<Inode> get_inode(InodeIdentifier) const = 0;

protected:
    FS();

private:
    unsigned m_fsid { 0 };
    bool m_readonly { false };
};

class Inode : public Retainable<Inode> {
    friend class VFS;
public:
    virtual ~Inode();

    virtual void one_retain_left() { }

    FS& fs() { return m_fs; }
    const FS& fs() const { return m_fs; }
    unsigned fsid() const;
    unsigned index() const { return m_index; }

    size_t size() const { return metadata().size; }
    bool is_symlink() const { return metadata().isSymbolicLink(); }
    bool is_directory() const { return metadata().isDirectory(); }
    bool is_character_device() const { return metadata().isCharacterDevice(); }
    mode_t mode() const { return metadata().mode; }

    InodeIdentifier identifier() const { return { fsid(), index() }; }
    virtual InodeMetadata metadata() const = 0;

    ByteBuffer read_entire(FileDescriptor* = nullptr) const;

    virtual ssize_t read_bytes(off_t, size_t, byte* buffer, FileDescriptor*) const = 0;
    virtual bool traverse_as_directory(Function<bool(const FS::DirectoryEntry&)>) const = 0;
    virtual InodeIdentifier lookup(const String& name) = 0;
    virtual String reverse_lookup(InodeIdentifier) = 0;
    virtual ssize_t write_bytes(off_t, size_t, const byte* data, FileDescriptor*) = 0;
    virtual bool add_child(InodeIdentifier child_id, const String& name, byte file_type, int& error) = 0;
    virtual bool remove_child(const String& name, int& error) = 0;
    virtual RetainPtr<Inode> parent() const = 0;
    virtual size_t directory_entry_count() const = 0;
    virtual bool chmod(mode_t, int& error) = 0;

    bool is_metadata_dirty() const { return m_metadata_dirty; }

    virtual int set_atime(time_t);
    virtual int set_ctime(time_t);
    virtual int set_mtime(time_t);
    virtual int increment_link_count();
    virtual int decrement_link_count();

    virtual void flush_metadata() = 0;

    void will_be_destroyed();

    void set_vmo(RetainPtr<VMObject>&&);
    VMObject* vmo() { return m_vmo.ptr(); }
    const VMObject* vmo() const { return m_vmo.ptr(); }

protected:
    Inode(FS& fs, unsigned index);
    void set_metadata_dirty(bool b) { m_metadata_dirty = b; }

    mutable Lock m_lock;

private:
    FS& m_fs;
    unsigned m_index { 0 };
    RetainPtr<VMObject> m_vmo;
    bool m_metadata_dirty { false };
};

inline FS* InodeIdentifier::fs()
{
    return FS::from_fsid(m_fsid);
}

inline const FS* InodeIdentifier::fs() const
{
    return FS::from_fsid(m_fsid);
}

inline bool InodeIdentifier::is_root_inode() const
{
    return (*this) == fs()->root_inode();
}

inline unsigned Inode::fsid() const
{
    return m_fs.fsid();
}

namespace AK {

template<>
struct Traits<InodeIdentifier> {
    // FIXME: This is a shitty hash.
    static unsigned hash(const InodeIdentifier& inode) { return Traits<unsigned>::hash(inode.fsid()) + Traits<unsigned>::hash(inode.index()); }
    static void dump(const InodeIdentifier& inode) { kprintf("%02u:%08u", inode.fsid(), inode.index()); }
};

}

