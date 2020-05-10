/*
 * Copyright (c) 2020, The SerenityOS developers.
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
#include <AK/Types.h>
#include <AK/Vector.h>
#include <stdlib.h>

namespace Line {

class Style {
public:
    enum class XtermColor : int {
        Default = 9,
        Black = 0,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
    };

    struct UnderlineTag {
    };
    struct BoldTag {
    };
    struct ItalicTag {
    };
    struct Color {
        explicit Color(XtermColor color)
            : m_xterm_color(color)
            , m_is_rgb(false)
        {
        }
        Color(u8 r, u8 g, u8 b)
            : m_rgb_color({ r, g, b })
            , m_is_rgb(true)
        {
        }

        XtermColor m_xterm_color { XtermColor::Default };
        Vector<u8, 3> m_rgb_color;
        bool m_is_rgb { false };
    };

    struct Background : public Color {
        explicit Background(XtermColor color)
            : Color(color)
        {
        }
        Background(u8 r, u8 g, u8 b)
            : Color(r, g, b)
        {
        }
        String to_vt_escape() const;
    };

    struct Foreground : public Color {
        explicit Foreground(XtermColor color)
            : Color(color)
        {
        }
        Foreground(u8 r, u8 g, u8 b)
            : Color(r, g, b)
        {
        }

        String to_vt_escape() const;
    };

    static constexpr UnderlineTag Underline {};
    static constexpr BoldTag Bold {};
    static constexpr ItalicTag Italic {};

    // prepare for the horror of templates
    template<typename T, typename... Rest>
    Style(const T& style_arg, Rest... rest)
        : Style(rest...)
    {
        set(style_arg);
    }
    Style() { }

    bool underline() const { return m_underline; }
    bool bold() const { return m_bold; }
    bool italic() const { return m_italic; }
    Background background() const { return m_background; }
    Foreground foreground() const { return m_foreground; }

    void set(const ItalicTag&) { m_italic = true; }
    void set(const BoldTag&) { m_bold = true; }
    void set(const UnderlineTag&) { m_underline = true; }
    void set(const Background& bg) { m_background = bg; }
    void set(const Foreground& fg) { m_foreground = fg; }

private:
    bool m_underline { false };
    bool m_bold { false };
    bool m_italic { false };
    Background m_background { XtermColor::Default };
    Foreground m_foreground { XtermColor::Default };
};
}
