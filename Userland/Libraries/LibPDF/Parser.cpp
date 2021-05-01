/*
 * Copyright (c) 2021, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ScopeGuard.h>
#include <AK/TypeCasts.h>
#include <LibPDF/Document.h>
#include <LibPDF/Parser.h>
#include <ctype.h>
#include <math.h>

namespace PDF {

template<typename T, typename... Args>
static NonnullRefPtr<T> make_object(Args... args) requires(IsBaseOf<Object, T>)
{
    return adopt_ref(*new T(forward<Args>(args)...));
}

Parser::Parser(Badge<Document>, const ReadonlyBytes& bytes)
    : m_reader(bytes)
{
}

bool Parser::perform_validation()
{
    return !sloppy_is_linearized() && parse_header();
}

Parser::XRefTableAndTrailer Parser::parse_last_xref_table_and_trailer()
{
    m_reader.move_to(m_reader.bytes().size() - 1);
    VERIFY(navigate_to_before_eof_marker());
    navigate_to_after_startxref();
    VERIFY(!m_reader.done());

    m_reader.set_reading_forwards();
    auto xref_offset_value = parse_number();
    VERIFY(xref_offset_value.is_int());
    auto xref_offset = xref_offset_value.as_int();

    m_reader.move_to(xref_offset);
    auto xref_table = parse_xref_table();
    auto trailer = parse_file_trailer();

    return { xref_table, trailer };
}

bool Parser::parse_header()
{
    // TODO: Do something with the version?
    m_reader.set_reading_forwards();
    m_reader.move_to(0);
    if (m_reader.remaining() < 8 || !m_reader.matches("%PDF-"))
        return false;
    m_reader.move_by(5);

    char major_ver = m_reader.read();
    if (major_ver != '1' && major_ver != '2')
        return false;
    if (m_reader.read() != '.')
        return false;

    char minor_ver = m_reader.read();
    if (minor_ver < '0' || major_ver > '7')
        return false;
    consume_eol();

    // Parse optional high-byte comment, which signifies a binary file
    // TODO: Do something with this?
    auto comment = parse_comment();
    if (!comment.is_empty()) {
        auto binary = comment.length() >= 4;
        if (binary) {
            for (size_t i = 0; i < comment.length() && binary; i++)
                binary = static_cast<u8>(comment[i]) > 128;
        }
    }

    return true;
}

XRefTable Parser::parse_xref_table()
{
    VERIFY(m_reader.matches("xref"));
    m_reader.move_by(4);
    consume_eol();

    XRefTable table;

    while (true) {
        if (m_reader.matches("trailer"))
            break;

        Vector<XRefEntry> entries;

        auto starting_index_value = parse_number();
        auto starting_index = starting_index_value.as_int();
        auto object_count_value = parse_number();
        auto object_count = object_count_value.as_int();

        for (int i = 0; i < object_count; i++) {
            auto offset_string = String(m_reader.bytes().slice(m_reader.offset(), 10));
            m_reader.move_by(10);
            consume(' ');

            auto generation_string = String(m_reader.bytes().slice(m_reader.offset(), 5));
            m_reader.move_by(5);
            consume(' ');

            auto letter = m_reader.read();
            VERIFY(letter == 'n' || letter == 'f');

            // The line ending sequence can be one of the following:
            // SP CR, SP LF, or CR LF
            if (m_reader.matches(' ')) {
                consume();
                auto ch = consume();
                VERIFY(ch == '\r' || ch == '\n');
            } else {
                VERIFY(m_reader.matches("\r\n"));
                m_reader.move_by(2);
            }

            auto offset = strtol(offset_string.characters(), nullptr, 10);
            auto generation = strtol(generation_string.characters(), nullptr, 10);

            entries.append({ offset, static_cast<u16>(generation), letter == 'n' });
        }

        table.add_section({ starting_index, object_count, entries });
    }

    return table;
}

NonnullRefPtr<DictObject> Parser::parse_file_trailer()
{
    VERIFY(m_reader.matches("trailer"));
    m_reader.move_by(7);
    consume_whitespace();
    auto dict = parse_dict();

    VERIFY(m_reader.matches("startxref"));
    m_reader.move_by(9);
    consume_whitespace();

    m_reader.move_until([&](auto) { return matches_eol(); });
    consume_eol();
    VERIFY(m_reader.matches("%%EOF"));
    m_reader.move_by(5);
    consume_whitespace();
    VERIFY(m_reader.done());

    return dict;
}

bool Parser::navigate_to_before_eof_marker()
{
    m_reader.set_reading_backwards();

    while (!m_reader.done()) {
        m_reader.move_until([&](auto) { return matches_eol(); });
        if (m_reader.done())
            return false;

        consume_eol();
        if (!m_reader.matches("%%EOF"))
            continue;

        m_reader.move_by(5);
        if (!matches_eol())
            continue;
        consume_eol();
        return true;
    }

    return false;
}

bool Parser::navigate_to_after_startxref()
{
    m_reader.set_reading_backwards();

    while (!m_reader.done()) {
        m_reader.move_until([&](auto) { return matches_eol(); });
        auto offset = m_reader.offset() + 1;

        consume_eol();
        if (!m_reader.matches("startxref"))
            continue;

        m_reader.move_by(9);
        if (!matches_eol())
            continue;

        m_reader.move_to(offset);
        return true;
    }

    return false;
}

bool Parser::sloppy_is_linearized()
{
    ScopeGuard guard([&] {
        m_reader.move_to(0);
        m_reader.set_reading_forwards();
    });

    auto limit = min(1024ul, m_reader.bytes().size() - 1);
    m_reader.move_to(limit);
    m_reader.set_reading_backwards();

    while (!m_reader.done()) {
        m_reader.move_until('/');
        if (m_reader.matches("/Linearized"))
            return true;
        m_reader.move_by(1);
    }

    return false;
}

String Parser::parse_comment()
{
    if (!m_reader.matches('%'))
        return {};

    consume();
    auto comment_start_offset = m_reader.offset();
    m_reader.move_until([&] {
        return matches_eol();
    });
    String str = StringView(m_reader.bytes().slice(comment_start_offset, m_reader.offset() - comment_start_offset));
    consume_eol();
    consume_whitespace();
    return str;
}

Value Parser::parse_value()
{
    parse_comment();

    if (m_reader.matches("null")) {
        m_reader.move_by(4);
        consume_whitespace();
        return Value();
    }

    if (m_reader.matches("true")) {
        m_reader.move_by(4);
        consume_whitespace();
        return Value(true);
    }

    if (m_reader.matches("false")) {
        m_reader.move_by(5);
        consume_whitespace();
        return Value(false);
    }

    if (matches_number())
        return parse_possible_indirect_object_or_ref();

    if (m_reader.matches('/'))
        return parse_name();

    if (m_reader.matches("<<")) {
        auto dict = parse_dict();
        if (m_reader.matches("stream\n"))
            return parse_stream(dict);
        return dict;
    }

    if (m_reader.matches_any('(', '<'))
        return parse_string();

    if (m_reader.matches('['))
        return parse_array();

    dbgln("tried to parse value, but found char {} ({}) at offset {}", m_reader.peek(), static_cast<u8>(m_reader.peek()), m_reader.offset());
    VERIFY_NOT_REACHED();
}

Value Parser::parse_possible_indirect_object_or_ref()
{
    auto first_number = parse_number();
    if (!first_number.is_int() || !matches_number())
        return first_number;

    m_reader.save();
    auto second_number = parse_number();
    if (!second_number.is_int()) {
        m_reader.load();
        return first_number;
    }

    if (m_reader.matches('R')) {
        m_reader.discard();
        consume();
        consume_whitespace();
        return make_object<IndirectObjectRef>(first_number.as_int(), second_number.as_int());
    }

    if (m_reader.matches("obj")) {
        m_reader.discard();
        return parse_indirect_object(first_number.as_int(), second_number.as_int());
    }

    m_reader.load();
    return first_number;
}

NonnullRefPtr<IndirectObject> Parser::parse_indirect_object(int index, int generation)
{
    VERIFY(m_reader.matches("obj"));
    m_reader.move_by(3);
    if (matches_eol())
        consume_eol();
    auto value = parse_value();
    VERIFY(value.is_object());
    VERIFY(m_reader.matches("endobj"));
    VERIFY(consume_whitespace());

    return make_object<IndirectObject>(index, generation, value.as_object());
}

Value Parser::parse_number()
{
    size_t start_offset = m_reader.offset();
    bool is_float = false;

    if (m_reader.matches('+') || m_reader.matches('-'))
        consume();

    while (!m_reader.done()) {
        if (m_reader.matches('.')) {
            if (is_float)
                break;
            is_float = true;
            consume();
        } else if (isdigit(m_reader.peek())) {
            consume();
        } else {
            break;
        }
    }

    auto string = String(m_reader.bytes().slice(start_offset, m_reader.offset() - start_offset));
    float f = strtof(string.characters(), nullptr);
    if (is_float)
        return Value(f);

    VERIFY(floorf(f) == f);
    consume_whitespace();

    return Value(static_cast<int>(f));
}

NonnullRefPtr<NameObject> Parser::parse_name()
{
    consume('/');
    StringBuilder builder;

    while (true) {
        if (matches_whitespace())
            break;

        if (m_reader.matches('#')) {
            int hex_value = 0;
            for (int i = 0; i < 2; i++) {
                auto ch = consume();
                VERIFY(isxdigit(ch));
                hex_value *= 16;
                if (ch <= '9') {
                    hex_value += ch - '0';
                } else {
                    hex_value += ch - 'A' + 10;
                }
            }
            builder.append(static_cast<char>(hex_value));
            continue;
        }

        builder.append(consume());
    }

    consume_whitespace();

    return make_object<NameObject>(builder.to_string());
}

NonnullRefPtr<StringObject> Parser::parse_string()
{
    ScopeGuard guard([&] { consume_whitespace(); });

    if (m_reader.matches('('))
        return make_object<StringObject>(parse_literal_string(), false);
    return make_object<StringObject>(parse_hex_string(), true);
}

String Parser::parse_literal_string()
{
    consume('(');
    StringBuilder builder;
    auto opened_parens = 0;

    while (true) {
        if (m_reader.matches('(')) {
            opened_parens++;
            builder.append(consume());
        } else if (m_reader.matches(')')) {
            consume();
            if (opened_parens == 0)
                break;
            opened_parens--;
            builder.append(')');
        } else if (m_reader.matches('\\')) {
            consume();
            if (matches_eol()) {
                consume_eol();
                continue;
            }

            VERIFY(!m_reader.done());
            auto ch = consume();
            switch (ch) {
            case 'n':
                builder.append('\n');
                break;
            case 'r':
                builder.append('\r');
                break;
            case 't':
                builder.append('\t');
                break;
            case 'b':
                builder.append('\b');
                break;
            case 'f':
                builder.append('\f');
                break;
            case '(':
                builder.append('(');
                break;
            case ')':
                builder.append(')');
                break;
            case '\\':
                builder.append('\\');
                break;
            default: {
                if (ch >= '0' && ch <= '7') {
                    int octal_value = ch - '0';
                    for (int i = 0; i < 2; i++) {
                        auto octal_ch = consume();
                        if (octal_ch < '0' || octal_ch > '7')
                            break;
                        octal_value = octal_value * 8 + (octal_ch - '0');
                    }
                    builder.append(static_cast<char>(octal_value));
                } else {
                    builder.append(ch);
                }
            }
            }
        } else if (matches_eol()) {
            consume_eol();
            builder.append('\n');
        } else {
            builder.append(consume());
        }
    }

    VERIFY(opened_parens == 0);
    return builder.to_string();
}

String Parser::parse_hex_string()
{
    consume('<');
    StringBuilder builder;

    while (true) {
        if (m_reader.matches('>')) {
            consume();
            return builder.to_string();
        } else {
            int hex_value = 0;

            for (int i = 0; i < 2; i++) {
                auto ch = consume();
                if (ch == '>') {
                    // The hex string contains an odd number of characters, and the last character
                    // is assumed to be '0'
                    consume();
                    hex_value *= 16;
                    builder.append(static_cast<char>(hex_value));
                    return builder.to_string();
                }
                VERIFY(isxdigit(ch));

                hex_value *= 16;
                if (ch <= '9') {
                    hex_value += ch - '0';
                } else {
                    hex_value += ch - 'A' + 10;
                }
            }

            builder.append(static_cast<char>(hex_value));
        }
    }
}

NonnullRefPtr<ArrayObject> Parser::parse_array()
{
    consume('[');
    consume_whitespace();
    Vector<Value> values;

    while (!m_reader.matches(']'))
        values.append(parse_value());

    consume(']');
    consume_whitespace();

    return make_object<ArrayObject>(values);
}

NonnullRefPtr<DictObject> Parser::parse_dict()
{
    consume('<');
    consume('<');
    consume_whitespace();
    HashMap<FlyString, Value> map;

    while (true) {
        if (m_reader.matches(">>"))
            break;
        auto name = parse_name();
        auto value = parse_value();
        map.set(name->name(), value);
    }

    consume('>');
    consume('>');
    consume_whitespace();

    return make_object<DictObject>(map);
}

NonnullRefPtr<StreamObject> Parser::parse_stream(NonnullRefPtr<DictObject> dict)
{
    VERIFY(m_reader.matches("stream"));
    m_reader.move_by(6);
    consume_eol();

    auto length_value = dict->map().get("Length");
    VERIFY(length_value.has_value());
    auto length = length_value.value();
    VERIFY(length.is_int());

    auto bytes = m_reader.bytes().slice(m_reader.offset(), length.as_int());

    return make_object<StreamObject>(dict, bytes);
}

bool Parser::matches_eol() const
{
    return m_reader.matches_any(0xa, 0xd);
}

bool Parser::matches_whitespace() const
{
    return matches_eol() || m_reader.matches_any(0, 0x9, 0xc, ' ');
}

bool Parser::matches_number() const
{
    if (m_reader.done())
        return false;
    auto ch = m_reader.peek();
    return isdigit(ch) || ch == '-' || ch == '+';
}

void Parser::consume_eol()
{
    if (m_reader.matches("\r\n")) {
        consume(2);
    } else {
        auto consumed = consume();
        VERIFY(consumed == 0xd || consumed == 0xa);
    }
}

bool Parser::consume_whitespace()
{
    bool consumed = false;
    while (matches_whitespace()) {
        consumed = true;
        consume();
    }
    return consumed;
}

char Parser::consume()
{
    return m_reader.read();
}

void Parser::consume(char ch)
{
    VERIFY(consume() == ch);
}

}
