#include "SyntheticFileSystem.h"
#include "FileDescriptor.h"
#include <AK/StdLib.h>

#ifndef SERENITY
typedef int InterruptDisabler;
#define ASSERT_INTERRUPTS_DISABLED()
#endif

//#define SYNTHFS_DEBUG

RetainPtr<SyntheticFileSystem> SyntheticFileSystem::create()
{
    return adopt(*new SyntheticFileSystem);
}

SyntheticFileSystem::SyntheticFileSystem()
{
}

SyntheticFileSystem::~SyntheticFileSystem()
{
}

bool SyntheticFileSystem::initialize()
{
    // Add a File for the root directory.
    // FIXME: This needs work.
    auto rootDir = make<File>();
    rootDir->metadata.inode = { id(), RootInodeIndex };
    rootDir->parent = { id(), RootInodeIndex };
    rootDir->metadata.mode = 0040555;
    rootDir->metadata.uid = 0;
    rootDir->metadata.gid = 0;
    rootDir->metadata.size = 0;
    rootDir->metadata.mtime = mepoch;
    m_inodes.set(RootInodeIndex, move(rootDir));

#ifndef SERENITY
    addFile(createTextFile("file", String("I'm a synthetic file!\n").toByteBuffer(), 0100644));
    addFile(createTextFile("message", String("Hey! This isn't my bottle!\n").toByteBuffer(), 0100644));
    addFile(createGeneratedFile("lunk", [] { return String("/home/andreas/file1").toByteBuffer(); }, 00120777));
#endif
    return true;
}

auto SyntheticFileSystem::createDirectory(String&& name) -> OwnPtr<File>
{
    auto file = make<File>();
    file->name = move(name);
    file->metadata.size = 0;
    file->metadata.uid = 0;
    file->metadata.gid = 0;
    file->metadata.mode = 0040555;
    file->metadata.mtime = mepoch;
    return file;
}

auto SyntheticFileSystem::createTextFile(String&& name, ByteBuffer&& contents, Unix::mode_t mode) -> OwnPtr<File>
{
    auto file = make<File>();
    file->data = contents;
    file->name = move(name);
    file->metadata.size = file->data.size();
    file->metadata.uid = 100;
    file->metadata.gid = 200;
    file->metadata.mode = mode;
    file->metadata.mtime = mepoch;
    return file;
}

auto SyntheticFileSystem::createGeneratedFile(String&& name, Function<ByteBuffer()>&& generator, Unix::mode_t mode) -> OwnPtr<File>
{
    auto file = make<File>();
    file->generator = move(generator);
    file->name = move(name);
    file->metadata.size = 0;
    file->metadata.uid = 0;
    file->metadata.gid = 0;
    file->metadata.mode = mode;
    file->metadata.mtime = mepoch;
    return file;
}

InodeIdentifier SyntheticFileSystem::addFile(OwnPtr<File>&& file, InodeIndex parent)
{
    ASSERT_INTERRUPTS_DISABLED();
    ASSERT(file);
    auto it = m_inodes.find(parent);
    ASSERT(it != m_inodes.end());
    InodeIdentifier newInode { id(), generateInodeIndex() };
    file->metadata.inode = newInode;
    file->parent = { id(), parent };
    (*it).value->children.append(file.ptr());
    m_inodes.set(newInode.index(), move(file));
    return newInode;
}

bool SyntheticFileSystem::removeFile(InodeIndex inode)
{
    ASSERT_INTERRUPTS_DISABLED();
    auto it = m_inodes.find(inode);
    if (it == m_inodes.end())
        return false;
    auto& file = *(*it).value;

    auto pit = m_inodes.find(file.parent.index());
    if (pit == m_inodes.end())
        return false;
    auto& parent = *(*pit).value;
    for (size_t i = 0; i < parent.children.size(); ++i) {
        if (parent.children[i]->metadata.inode.index() != inode) {
            continue;
        }
        parent.children.remove(i);
        break;
    }

    for (auto& child : file.children)
        removeFile(child->metadata.inode.index());
    m_inodes.remove(inode);
    return true;
}

const char* SyntheticFileSystem::className() const
{
    return "synthfs";
}

InodeIdentifier SyntheticFileSystem::rootInode() const
{
    return { id(), 1 };
}

bool SyntheticFileSystem::enumerateDirectoryInode(InodeIdentifier inode, Function<bool(const DirectoryEntry&)> callback) const
{
    InterruptDisabler disabler;
    ASSERT(inode.fileSystemID() == id());
#ifdef SYNTHFS_DEBUG
    kprintf("synthfs: enumerateDirectoryInode %u\n", inode.index());
#endif

    auto it = m_inodes.find(inode.index());
    if (it == m_inodes.end())
        return false;
    const File& synInode = *(*it).value;
    if (!synInode.metadata.isDirectory())
        return false;

    callback({ ".", 1, synInode.metadata.inode, 2 });
    callback({ "..", 2, synInode.parent, 2 });

    for (auto& child : synInode.children)
        callback({ child->name.characters(), child->name.length(), child->metadata.inode, child->metadata.isDirectory() ? (byte)2 : (byte)1 });
    return true;
}

InodeMetadata SyntheticFileSystem::inodeMetadata(InodeIdentifier inode) const
{
    InterruptDisabler disabler;
    ASSERT(inode.fileSystemID() == id());
#ifdef SYNTHFS_DEBUG
    kprintf("synthfs: inodeMetadata(%u)\n", inode.index());
#endif

    auto it = m_inodes.find(inode.index());
    if (it == m_inodes.end())
        return { };
    return (*it).value->metadata;
}

bool SyntheticFileSystem::setModificationTime(InodeIdentifier, dword timestamp)
{
    (void) timestamp;
    kprintf("FIXME: Implement SyntheticFileSystem::setModificationTime().\n");
    return false;
}

InodeIdentifier SyntheticFileSystem::createInode(InodeIdentifier parentInode, const String& name, Unix::mode_t mode, unsigned size)
{
    (void) parentInode;
    (void) name;
    (void) mode;
    (void) size;
    kprintf("FIXME: Implement SyntheticFileSystem::createDirectoryInode().\n");
    return { };
}

bool SyntheticFileSystem::writeInode(InodeIdentifier, const ByteBuffer&)
{
    kprintf("FIXME: Implement SyntheticFileSystem::writeInode().\n");
    return false;
}

Unix::ssize_t SyntheticFileSystem::readInodeBytes(InodeIdentifier inode, Unix::off_t offset, Unix::size_t count, byte* buffer, FileDescriptor* handle) const
{
    ASSERT(inode.fileSystemID() == id());
#ifdef SYNTHFS_DEBUG
    kprintf("synthfs: readInode %u\n", inode.index());
#endif
    ASSERT(offset >= 0);
    ASSERT(buffer);

    const File* found_file;
    {
        InterruptDisabler disabler;
        auto it = m_inodes.find(inode.index());
        if (it == m_inodes.end())
            return false;
        found_file = (*it).value.ptr();
    }
    const File& file = *found_file;
    ByteBuffer generatedData;
    if (file.generator) {
        if (!handle) {
            generatedData = file.generator();
        } else {
            if (!handle->generatorCache())
                handle->generatorCache() = file.generator();
            generatedData = handle->generatorCache();
        }
    }

    auto* data = generatedData ? &generatedData : &file.data;
    Unix::ssize_t nread = min(static_cast<Unix::off_t>(data->size() - offset), static_cast<Unix::off_t>(count));
    memcpy(buffer, data->pointer() + offset, nread);
    if (nread == 0 && handle && handle->generatorCache())
        handle->generatorCache().clear();
    return nread;
}

InodeIdentifier SyntheticFileSystem::makeDirectory(InodeIdentifier parentInode, const String& name, Unix::mode_t)
{
    (void) parentInode;
    (void) name;
    kprintf("FIXME: Implement SyntheticFileSystem::makeDirectory().\n");
    return { };
}

auto SyntheticFileSystem::generateInodeIndex() -> InodeIndex
{
    return m_nextInodeIndex++;
}

InodeIdentifier SyntheticFileSystem::findParentOfInode(InodeIdentifier inode) const
{
    auto it = m_inodes.find(inode.index());
    if (it == m_inodes.end())
        return { };
    return (*it).value->parent;
}
