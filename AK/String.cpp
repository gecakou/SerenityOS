#include "AKString.h"
#include "StdLibExtras.h"

namespace AK {

bool String::operator==(const String& other) const
{
    if (!m_impl)
        return !other.m_impl;

    if (!other.m_impl)
        return false;

    if (length() != other.length())
        return false;
    
    return !memcmp(characters(), other.characters(), length());
}

String String::empty()
{
    return StringImpl::the_empty_stringimpl();
}

String String::isolated_copy() const
{
    if (!m_impl)
        return { };
    if (!m_impl->length())
        return empty();
    char* buffer;
    auto impl = StringImpl::create_uninitialized(length(), buffer);
    memcpy(buffer, m_impl->characters(), m_impl->length());
    return String(move(*impl));
}

String String::substring(size_t start, size_t length) const
{
    ASSERT(m_impl);
    ASSERT(start + length <= m_impl->length());
    // FIXME: This needs some input bounds checking.
    char* buffer;
    auto newImpl = StringImpl::create_uninitialized(length, buffer);
    memcpy(buffer, characters() + start, length);
    buffer[length] = '\0';
    return newImpl;
}

Vector<String> String::split(const char separator) const
{
    if (is_empty())
        return { };

    Vector<String> v;
    size_t substart = 0;
    for (size_t i = 0; i < length(); ++i) {
        char ch = characters()[i];
        if (ch == separator) {
            size_t sublen = i - substart;
            if (sublen != 0)
                v.append(substring(substart, sublen));
            substart = i + 1;
        }
    }
    size_t taillen = length() - substart;
    if (taillen != 0)
        v.append(substring(substart, taillen));
    if (characters()[length() - 1] == separator)
        v.append(empty());
    return v;
}

ByteBuffer String::to_byte_buffer() const
{
    if (!m_impl)
        return nullptr;
    return ByteBuffer::copy(reinterpret_cast<const byte*>(characters()), length());
}

unsigned String::toUInt(bool& ok) const
{
    unsigned value = 0;
    for (size_t i = 0; i < length(); ++i) {
        if (characters()[i] < '0' || characters()[i] > '9') {
            ok = false;
            return 0;
        }
        value = value * 10;
        value += characters()[i] - '0';
    }
    ok = true;
    return value;
}

}
