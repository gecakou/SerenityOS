#pragma once

#include <AK/AKString.h>
#include <SharedGraphics/Rect.h>

class WSMenuItem {
public:
    enum Type {
        None,
        Text,
        Separator,
    };

    explicit WSMenuItem(const String& text);
    explicit WSMenuItem(Type);
    ~WSMenuItem();

    Type type() const { return m_type; }
    bool enabled() const { return m_enabled; }

    String text() const { return m_text; }

    void set_rect(const Rect& rect) { m_rect = rect; }
    Rect rect() const { return m_rect; }

private:
    Type m_type { None };
    bool m_enabled { true };
    String m_text;
    Rect m_rect;
};

