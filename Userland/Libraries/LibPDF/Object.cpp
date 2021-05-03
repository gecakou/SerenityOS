/*
 * Copyright (c) 2021, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Hex.h>
#include <LibPDF/Document.h>
#include <LibPDF/Object.h>

namespace PDF {

template<typename T>
NonnullRefPtr<T> Object::resolved_to(Document* document) const
{
    if (is_indirect_object_ref()) {
        auto target = document->get_or_load_object(static_cast<const IndirectObjectRef*>(this)->index());
        return object_cast<T>(target);
    }
    if (is_indirect_object()) {
        auto target = static_cast<const IndirectObject*>(this)->object();
        return object_cast<T>(target);
    }
    return *this;
}

#define DEFINE_ACCESSORS(class_name, snake_name)                                                           \
    NonnullRefPtr<class_name> ArrayObject::get_##snake_name##_at(Document* document, size_t index) const   \
    {                                                                                                      \
        return m_elements[index].as_object()->resolved_to<class_name>(document);                           \
    }                                                                                                      \
                                                                                                           \
    NonnullRefPtr<class_name> DictObject::get_##snake_name(Document* document, const FlyString& key) const \
    {                                                                                                      \
        return get_object(key)->resolved_to<class_name>(document);                                         \
    }
ENUMERATE_DIRECT_OBJECT_TYPES(DEFINE_ACCESSORS)
#undef DEFINE_INDEXER

static void append_indent(StringBuilder& builder, int indent)
{
    for (int i = 0; i < indent; i++)
        builder.append("  ");
}

String StringObject::to_string(int) const
{
    if (is_binary())
        return String::formatted("<{}>", encode_hex(string().bytes()).to_uppercase());
    return String::formatted("({})", string());
}

String NameObject::to_string(int) const
{
    StringBuilder builder;
    builder.appendff("/{}", this->name());
    return builder.to_string();
}

String ArrayObject::to_string(int indent) const
{
    StringBuilder builder;
    builder.append("[\n");
    bool first = true;

    for (auto& element : elements()) {
        if (!first)
            builder.append(",\n");
        first = false;
        append_indent(builder, indent + 1);
        builder.appendff("{}", element.to_string(indent));
    }

    builder.append('\n');
    append_indent(builder, indent);
    builder.append(']');
    return builder.to_string();
}

String DictObject::to_string(int indent) const
{
    StringBuilder builder;
    builder.append("<<\n");
    bool first = true;

    for (auto& [key, value] : map()) {
        if (!first)
            builder.append(",\n");
        first = false;
        append_indent(builder, indent + 1);
        builder.appendff("/{} ", key);
        builder.appendff("{}", value.to_string(indent + 1));
    }

    builder.append('\n');
    append_indent(builder, indent);
    builder.append(">>");
    return builder.to_string();
}

String StreamObject::to_string(int indent) const
{
    StringBuilder builder;
    builder.append("stream\n");
    append_indent(builder, indent);
    builder.appendff("{}\n", dict()->to_string(indent + 1));
    append_indent(builder, indent + 1);

    auto string = encode_hex(bytes());
    while (true) {
        if (string.length() > 60) {
            builder.appendff("{}\n", string.substring(0, 60));
            append_indent(builder, indent);
            string = string.substring(60);
            continue;
        }

        builder.appendff("{}\n", string);
        break;
    }

    append_indent(builder, indent);
    builder.append("endstream");
    return builder.to_string();
}

String IndirectObject::to_string(int indent) const
{
    StringBuilder builder;
    builder.appendff("{} {} obj\n", index(), generation_index());
    append_indent(builder, indent + 1);
    builder.append(object()->to_string(indent + 1));
    builder.append('\n');
    append_indent(builder, indent);
    builder.append("endobj");
    return builder.to_string();
}

String IndirectObjectRef::to_string(int) const
{
    return String::formatted("{} {} R", index(), generation_index());
}

}
