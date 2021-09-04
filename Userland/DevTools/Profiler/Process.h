/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "EventSerialNumber.h"
#include <AK/HashMap.h>
#include <AK/MappedFile.h>
#include <AK/OwnPtr.h>
#include <AK/Vector.h>
#include <LibELF/Image.h>

namespace Profiler {

struct MappedObject {
    NonnullRefPtr<MappedFile> file;
    ELF::Image elf;
};

extern HashMap<String, OwnPtr<MappedObject>> g_mapped_object_cache;

class LibraryMetadata {
public:
    struct Library {
        FlatPtr base;
        size_t size;
        String name;
        FlatPtr text_base;
        MappedObject* object { nullptr };

        String symbolicate(FlatPtr, u32* offset) const;
    };

    void handle_mmap(FlatPtr base, size_t size, String const& name);
    Library const* library_containing(FlatPtr) const;

private:
    mutable HashMap<String, NonnullOwnPtr<Library>> m_libraries;
};

struct Thread {
    pid_t tid;
    EventSerialNumber start_valid;
    EventSerialNumber end_valid;

    bool valid_at(EventSerialNumber serial) const
    {
        return serial >= start_valid && (end_valid == EventSerialNumber {} || serial <= end_valid);
    }
};

struct Process {
    pid_t pid {};
    String executable;
    String basename;
    HashMap<int, Vector<Thread>> threads {};
    LibraryMetadata library_metadata {};
    EventSerialNumber start_valid;
    EventSerialNumber end_valid;

    Thread* find_thread(pid_t tid, EventSerialNumber serial);
    void handle_thread_create(pid_t tid, EventSerialNumber serial);
    void handle_thread_exit(pid_t tid, EventSerialNumber serial);

    bool valid_at(EventSerialNumber serial) const
    {
        return serial >= start_valid && (end_valid == EventSerialNumber {} || serial <= end_valid);
    }
};

}
