/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <AK/Format.h>
#include <wchar.h>

extern "C" {

size_t wcslen(wchar_t const* str)
{
    size_t len = 0;
    while (*(str++))
        ++len;
    return len;
}

wchar_t* wcscpy(wchar_t* dest, wchar_t const* src)
{
    wchar_t* original_dest = dest;
    while ((*dest++ = *src++) != '\0')
        ;
    return original_dest;
}

wchar_t* wcsncpy(wchar_t* dest, wchar_t const* src, size_t num)
{
    wchar_t* original_dest = dest;
    while (((*dest++ = *src++) != '\0') && ((size_t)(dest - original_dest) < num))
        ;
    return original_dest;
}

int wcscmp(wchar_t const* s1, wchar_t const* s2)
{
    while (*s1 == *s2++)
        if (*s1++ == 0)
            return 0;
    return *(wchar_t const*)s1 - *(wchar_t const*)--s2;
}

int wcsncmp(wchar_t const* s1, wchar_t const* s2, size_t n)
{
    if (!n)
        return 0;
    do {
        if (*s1 != *s2++)
            return *(wchar_t const*)s1 - *(wchar_t const*)--s2;
        if (*s1++ == 0)
            break;
    } while (--n);
    return 0;
}

wchar_t* wcschr(wchar_t const* str, int c)
{
    wchar_t ch = c;
    for (;; ++str) {
        if (*str == ch)
            return const_cast<wchar_t*>(str);
        if (!*str)
            return nullptr;
    }
}

wchar_t const* wcsrchr(wchar_t const* str, wchar_t wc)
{
    wchar_t* last = nullptr;
    wchar_t c;
    for (; (c = *str); ++str) {
        if (c == wc)
            last = const_cast<wchar_t*>(str);
    }
    return last;
}

wchar_t* wcscat(wchar_t* dest, wchar_t const* src)
{
    size_t dest_length = wcslen(dest);
    size_t i;
    for (i = 0; src[i] != '\0'; i++)
        dest[dest_length + i] = src[i];
    dest[dest_length + i] = '\0';
    return dest;
}

wchar_t* wcsncat(wchar_t* dest, wchar_t const* src, size_t n)
{
    size_t dest_length = wcslen(dest);
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[dest_length + i] = src[i];
    dest[dest_length + i] = '\0';
    return dest;
}

wchar_t* wcstok(wchar_t* str, wchar_t const* delim, wchar_t** ptr)
{
    wchar_t* used_str = str;
    if (!used_str) {
        used_str = *ptr;
    }

    size_t token_start = 0;
    size_t token_end = 0;
    size_t str_len = wcslen(used_str);
    size_t delim_len = wcslen(delim);

    for (size_t i = 0; i < str_len; ++i) {
        bool is_proper_delim = false;

        for (size_t j = 0; j < delim_len; ++j) {
            if (used_str[i] == delim[j]) {
                // Skip beginning delimiters
                if (token_end - token_start == 0) {
                    ++token_start;
                    break;
                }

                is_proper_delim = true;
            }
        }

        ++token_end;
        if (is_proper_delim && token_end > 0) {
            --token_end;
            break;
        }
    }

    if (used_str[token_start] == '\0')
        return nullptr;

    if (token_end == 0) {
        return &used_str[token_start];
    }

    used_str[token_end] = '\0';
    return &used_str[token_start];
}

long wcstol(wchar_t const*, wchar_t**, int)
{
    dbgln("FIXME: Implement wcstol()");
    TODO();
}

long long wcstoll(wchar_t const*, wchar_t**, int)
{
    dbgln("FIXME: Implement wcstoll()");
    TODO();
}

wint_t btowc(int)
{
    dbgln("FIXME: Implement btowc()");
    TODO();
}
}
