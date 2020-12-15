/*
 * Copyright (c) 2019-2020, Jesse Buhagiar <jooster669@gmail.com>
 * Copyright (c) 2020, Itamar S. <itamar8910@gmail.com>
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

#include <AK/ByteBuffer.h>
#include <Kernel/CoreDump.h>
#include <Kernel/FileSystem/Custody.h>
#include <Kernel/FileSystem/FileDescription.h>
#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/Process.h>
#include <Kernel/Ptrace.h>
#include <Kernel/RTC.h>
#include <Kernel/SpinLock.h>
#include <Kernel/VM/ProcessPagingScope.h>
#include <LibELF/CoreDump.h>
#include <LibELF/exec_elf.h>

namespace Kernel {

OwnPtr<CoreDump> CoreDump::create(Process& process, const String& output_path)
{
    auto fd = create_target_file(process, output_path);
    if (!fd)
        return nullptr;
    return adopt_own(*new CoreDump(process, fd.release_nonnull()));
}

CoreDump::CoreDump(Process& process, NonnullRefPtr<FileDescription>&& fd)
    : m_process(process)
    , m_fd(move(fd))
    , m_num_program_headers(process.m_regions.size() + 1) // +1 for NOTE segment
{
}

CoreDump::~CoreDump()
{
}

RefPtr<FileDescription> CoreDump::create_target_file(const Process& process, const String& output_path)
{
    LexicalPath lexical_path(output_path);
    auto output_directory = lexical_path.dirname();
    if (VFS::the().open_directory(output_directory, VFS::the().root_custody()).is_error()) {
        auto res = VFS::the().mkdir(output_directory, 0777, VFS::the().root_custody());
        if (res.is_error())
            return nullptr;
    }
    auto tmp_dir = VFS::the().open_directory(output_directory, VFS::the().root_custody());
    if (tmp_dir.is_error())
        return nullptr;
    auto fd_or_error = VFS::the().open(
        lexical_path.basename(),
        O_CREAT | O_WRONLY | O_EXCL,
        0, // We will enable reading from userspace when we finish generating the coredump file
        *tmp_dir.value(),
        UidAndGid { process.uid(), process.gid() });

    if (fd_or_error.is_error())
        return nullptr;

    return fd_or_error.value();
}

void CoreDump::write_elf_header()
{
    Elf32_Ehdr elf_file_header;
    elf_file_header.e_ident[EI_MAG0] = 0x7f;
    elf_file_header.e_ident[EI_MAG1] = 'E';
    elf_file_header.e_ident[EI_MAG2] = 'L';
    elf_file_header.e_ident[EI_MAG3] = 'F';
    elf_file_header.e_ident[EI_CLASS] = ELFCLASS32;
    elf_file_header.e_ident[EI_DATA] = ELFDATA2LSB;
    elf_file_header.e_ident[EI_VERSION] = EV_CURRENT;
    elf_file_header.e_ident[EI_OSABI] = 0; // ELFOSABI_NONE
    elf_file_header.e_ident[EI_ABIVERSION] = 0;
    elf_file_header.e_ident[EI_PAD + 1] = 0;
    elf_file_header.e_ident[EI_PAD + 2] = 0;
    elf_file_header.e_ident[EI_PAD + 3] = 0;
    elf_file_header.e_ident[EI_PAD + 4] = 0;
    elf_file_header.e_ident[EI_PAD + 5] = 0;
    elf_file_header.e_ident[EI_PAD + 6] = 0;
    elf_file_header.e_type = ET_CORE;
    elf_file_header.e_machine = EM_386;
    elf_file_header.e_version = 1;
    elf_file_header.e_entry = 0;
    elf_file_header.e_phoff = sizeof(Elf32_Ehdr);
    elf_file_header.e_shoff = 0;
    elf_file_header.e_flags = 0;
    elf_file_header.e_ehsize = sizeof(Elf32_Ehdr);
    elf_file_header.e_shentsize = sizeof(Elf32_Shdr);
    elf_file_header.e_phentsize = sizeof(Elf32_Phdr);
    elf_file_header.e_phnum = m_num_program_headers;
    elf_file_header.e_shnum = 0;
    elf_file_header.e_shstrndx = SHN_UNDEF;

    (void)m_fd->write(UserOrKernelBuffer::for_kernel_buffer(reinterpret_cast<uint8_t*>(&elf_file_header)), sizeof(Elf32_Ehdr));
}

void CoreDump::write_program_headers(size_t notes_size)
{
    size_t offset = sizeof(Elf32_Ehdr) + m_num_program_headers * sizeof(Elf32_Phdr);
    for (auto& region : m_process.m_regions) {
        Elf32_Phdr phdr {};

        phdr.p_type = PT_LOAD;
        phdr.p_offset = offset;
        phdr.p_vaddr = reinterpret_cast<uint32_t>(region.vaddr().as_ptr());
        phdr.p_paddr = 0;

        phdr.p_filesz = region.page_count() * PAGE_SIZE;
        phdr.p_memsz = region.page_count() * PAGE_SIZE;
        phdr.p_align = 0;

        phdr.p_flags = region.is_readable() ? PF_R : 0;
        if (region.is_writable())
            phdr.p_flags |= PF_W;
        if (region.is_executable())
            phdr.p_flags |= PF_X;

        offset += phdr.p_filesz;

        (void)m_fd->write(UserOrKernelBuffer::for_kernel_buffer(reinterpret_cast<uint8_t*>(&phdr)), sizeof(Elf32_Phdr));
    }

    Elf32_Phdr notes_pheader {};
    notes_pheader.p_type = PT_NOTE;
    notes_pheader.p_offset = offset;
    notes_pheader.p_vaddr = 0;
    notes_pheader.p_paddr = 0;
    notes_pheader.p_filesz = notes_size;
    notes_pheader.p_memsz = 0;
    notes_pheader.p_align = 0;
    notes_pheader.p_flags = 0;

    (void)m_fd->write(UserOrKernelBuffer::for_kernel_buffer(reinterpret_cast<uint8_t*>(&notes_pheader)), sizeof(Elf32_Phdr));
}

void CoreDump::write_regions()
{
    for (auto& region : m_process.m_regions) {
        if (region.is_kernel())
            continue;

        region.set_readable(true);
        region.remap();

        auto& vmobj = region.vmobject();
        for (size_t i = 0; i < region.page_count(); i++) {
            PhysicalPage* page = vmobj.physical_pages()[region.first_page_index() + i];

            uint8_t zero_buffer[PAGE_SIZE] = {};
            Optional<UserOrKernelBuffer> src_buffer;

            if (page) {
                src_buffer = UserOrKernelBuffer::for_user_buffer(reinterpret_cast<uint8_t*>((region.vaddr().as_ptr() + (i * PAGE_SIZE))), PAGE_SIZE);
            } else {
                // If the current page is not backed by a physical page, we zero it in the coredump file.
                // TODO: Do we want to include the contents of pages that have not been faulted-in in the coredump?
                //       (A page may not be backed by a physical page because it has never been faulted in when the process ran).
                src_buffer = UserOrKernelBuffer::for_kernel_buffer(zero_buffer);
            }
            (void)m_fd->write(src_buffer.value(), PAGE_SIZE);
        }
    }
}

void CoreDump::write_notes_segment(ByteBuffer& notes_segment)
{
    (void)m_fd->write(UserOrKernelBuffer::for_kernel_buffer(notes_segment.data()), notes_segment.size());
}

ByteBuffer CoreDump::create_notes_threads_data() const
{
    ByteBuffer threads_data;

    m_process.for_each_thread([&](Thread& thread) {
        ByteBuffer entry_buff;

        ELF::Core::ThreadInfo info {};
        info.header.type = ELF::Core::NotesEntryHeader::Type::ThreadInfo;
        info.tid = thread.tid().value();
        Ptrace::copy_kernel_registers_into_ptrace_registers(info.regs, thread.get_register_dump_from_stack());

        entry_buff.append((void*)&info, sizeof(info));

        threads_data += entry_buff;

        return IterationDecision::Continue;
    });
    return threads_data;
}

ByteBuffer CoreDump::create_notes_regions_data() const
{
    ByteBuffer regions_data;
    for (size_t region_index = 0; region_index < m_process.m_regions.size(); ++region_index) {

        ByteBuffer memory_region_info_buffer;
        ELF::Core::MemoryRegionInfo info {};
        info.header.type = ELF::Core::NotesEntryHeader::Type::MemoryRegionInfo;

        auto& region = m_process.m_regions[region_index];
        info.region_start = reinterpret_cast<uint32_t>(region.vaddr().as_ptr());
        info.region_end = reinterpret_cast<uint32_t>(region.vaddr().as_ptr() + region.size());
        info.program_header_index = region_index;

        memory_region_info_buffer.append((void*)&info, sizeof(info));

        auto name = region.name();
        if (name.is_null())
            name = String::empty();
        memory_region_info_buffer.append(name.characters(), name.length() + 1);

        regions_data += memory_region_info_buffer;
    }
    return regions_data;
}

ByteBuffer CoreDump::create_notes_segment_data() const
{
    ByteBuffer notes_buffer;

    notes_buffer += create_notes_threads_data();
    notes_buffer += create_notes_regions_data();

    ELF::Core::NotesEntryHeader null_entry {};
    null_entry.type = ELF::Core::NotesEntryHeader::Type::Null;
    notes_buffer.append(&null_entry, sizeof(null_entry));

    return notes_buffer;
}

void CoreDump::write()
{
    ProcessPagingScope scope(m_process);

    ByteBuffer notes_segment = create_notes_segment_data();

    write_elf_header();
    write_program_headers(notes_segment.size());
    write_regions();
    write_notes_segment(notes_segment);

    (void)m_fd->chmod(0400); // Make coredump file readable
}

}
