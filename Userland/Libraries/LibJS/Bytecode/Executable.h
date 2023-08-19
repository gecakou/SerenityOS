/*
 * Copyright (c) 2021-2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FlyString.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/WeakPtr.h>
#include <LibJS/Bytecode/BasicBlock.h>
#include <LibJS/Bytecode/IdentifierTable.h>
#include <LibJS/Bytecode/RegexTable.h>
#include <LibJS/Bytecode/StringTable.h>

namespace JS::Bytecode {

struct PropertyLookupCache {
    WeakPtr<Shape> shape;
    Optional<u32> property_offset;
    u64 unique_shape_serial_number { 0 };
};

struct GlobalVariableCache : public PropertyLookupCache {
    u64 environment_serial_number { 0 };
};

struct Executable {
    FlyString name;
    Vector<PropertyLookupCache> property_lookup_caches;
    Vector<GlobalVariableCache> global_variable_caches;
    Vector<NonnullOwnPtr<BasicBlock>> basic_blocks;
    NonnullOwnPtr<StringTable> string_table;
    NonnullOwnPtr<IdentifierTable> identifier_table;
    NonnullOwnPtr<RegexTable> regex_table;
    size_t number_of_registers { 0 };
    bool is_strict_mode { false };

    String const& get_string(StringTableIndex index) const { return string_table->get(index); }
    FlyString const& get_identifier(IdentifierTableIndex index) const { return identifier_table->get(index); }

    void dump() const;
};

}
