#include "Terminal.h"
#include <AK/AKString.h>
#include <SharedGraphics/Font.h>
#include <SharedGraphics/Painter.h>
#include <AK/StdLibExtras.h>
#include <LibC/stdlib.h>
#include <LibC/unistd.h>
#include <LibC/stdio.h>
#include <LibC/gui.h>

//#define TERMINAL_DEBUG

void Terminal::create_window()
{
    m_pixel_width = m_columns * font().glyph_width() + m_inset * 2;
    m_pixel_height = (m_rows * (font().glyph_height() + m_line_spacing)) + (m_inset * 2) - m_line_spacing;

    GUI_WindowParameters params;
    params.rect = { { 300, 300 }, { m_pixel_width, m_pixel_height } };
    params.background_color = 0x000000;
    strcpy(params.title, "Terminal");
    m_window_id = gui_create_window(&params);
    ASSERT(m_window_id > 0);
    if (m_window_id < 0) {
        perror("gui_create_window");
        exit(1);
    }

    // NOTE: We never release the backing store.
    GUI_WindowBackingStoreInfo info;
    int rc = gui_get_window_backing_store(m_window_id, &info);
    if (rc < 0) {
        perror("gui_get_window_backing_store");
        exit(1);
    }

    m_backing = GraphicsBitmap::create_wrapper(info.size, info.pixels);
    dbgprintf("(Terminal:%d) window backing %ux%u @ %p\n", getpid(), info.size.width, info.size.height, info.pixels);

}

Terminal::Terminal()
    : m_font(Font::default_font())
{
    m_line_height = font().glyph_height() + m_line_spacing;

    set_size(80, 25);
    m_horizontal_tabs = static_cast<byte*>(malloc(columns()));
    for (unsigned i = 0; i < columns(); ++i)
        m_horizontal_tabs[i] = (i % 8) == 0;
    // Rightmost column is always last tab on line.
    m_horizontal_tabs[columns() - 1] = 1;

    m_lines = new Line*[rows()];
    for (size_t i = 0; i < rows(); ++i)
        m_lines[i] = new Line(columns());
}

Terminal::Line::Line(word columns)
    : length(columns)
{
    characters = new byte[length];
    attributes = new Attribute[length];
    needs_invalidation = false;
    memset(characters, ' ', length);
}

Terminal::Line::~Line()
{
    delete [] characters;
    delete [] attributes;
}

void Terminal::Line::clear()
{
    memset(characters, ' ', length);
    for (word i = 0 ; i < length; ++i)
        attributes[i].reset();
}

Terminal::~Terminal()
{
    for (size_t i = 0; i < m_rows; ++i)
        delete m_lines[i];
    delete [] m_lines;
    free(m_horizontal_tabs);
}

void Terminal::clear()
{
    for (size_t i = 0; i < rows(); ++i)
        line(i).clear();
    set_cursor(0, 0);
}

inline bool is_valid_parameter_character(byte ch)
{
    return ch >= 0x30 && ch <= 0x3f;
}

inline bool is_valid_intermediate_character(byte ch)
{
    return ch >= 0x20 && ch <= 0x2f;
}

inline bool is_valid_final_character(byte ch)
{
    return ch >= 0x40 && ch <= 0x7e;
}

unsigned parseUInt(const String& str, bool& ok)
{
    unsigned value = 0;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] < '0' || str[i] > '9') {
            ok = false;
            return 0;
        }
        value = value * 10;
        value += str[i] - '0';
    }
    ok = true;
    return value;
}

enum ANSIColor : byte {
    Black = 0,
    Red,
    Green,
    Brown,
    Blue,
    Magenta,
    Cyan,
    LightGray,
    DarkGray,
    BrightRed,
    BrightGreen,
    Yellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    White,
};

static inline Color ansi_color(unsigned color)
{
    switch (color) {
    case ANSIColor::Black: return Color(0, 0, 0);
    case ANSIColor::Red: return Color(225, 56, 43);
    case ANSIColor::Green: return Color(57, 181, 74);
    case ANSIColor::Brown: return Color(255, 199, 6);
    case ANSIColor::Blue: return Color(0, 111, 184);
    case ANSIColor::Magenta: return Color(118, 38, 113);
    case ANSIColor::Cyan: return Color(44, 181, 233);
    case ANSIColor::LightGray: return Color(204, 204, 204);
    case ANSIColor::DarkGray: return Color(128, 128, 128);
    case ANSIColor::BrightRed: return Color(255, 0, 0);
    case ANSIColor::BrightGreen: return Color(0, 255, 0);
    case ANSIColor::Yellow: return Color(255, 255, 0);
    case ANSIColor::BrightBlue: return Color(0, 0, 255);
    case ANSIColor::BrightMagenta: return Color(255, 0, 255);
    case ANSIColor::BrightCyan: return Color(0, 255, 255);
    case ANSIColor::White: return Color(255, 255, 255);
    }
    ASSERT_NOT_REACHED();
    return Color::White;
}

void Terminal::escape$m(const Vector<unsigned>& params)
{
    for (auto param : params) {
        switch (param) {
        case 0:
            // Reset
            m_current_attribute.reset();
            break;
        case 1:
            // Bold
            m_current_attribute.bold = true;
            break;
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            // Foreground color
            m_current_attribute.foreground_color = param - 30;
            break;
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            // Background color
            m_current_attribute.background_color = param - 30;
            break;
        }
    }
}

void Terminal::escape$s(const Vector<unsigned>&)
{
    m_saved_cursor_row = m_cursor_row;
    m_saved_cursor_column = m_cursor_column;
}

void Terminal::escape$u(const Vector<unsigned>&)
{
    set_cursor(m_saved_cursor_row, m_saved_cursor_column);
}

void Terminal::escape$H(const Vector<unsigned>& params)
{
    unsigned row = 1;
    unsigned col = 1;
    if (params.size() >= 1)
        row = params[0];
    if (params.size() >= 2)
        col = params[1];
    set_cursor(row - 1, col - 1);
}

void Terminal::escape$A(const Vector<unsigned>& params)
{
    int num = 1;
    if (params.size() >= 1)
        num = params[0];
    if (num == 0)
        num = 1;
    int new_row = (int)m_cursor_row - num;
    if (new_row < 0)
        new_row = 0;
    set_cursor(new_row, m_cursor_column);
}

void Terminal::escape$B(const Vector<unsigned>& params)
{
    int num = 1;
    if (params.size() >= 1)
        num = params[0];
    if (num == 0)
        num = 1;
    int new_row = (int)m_cursor_row + num;
    if (new_row >= m_rows)
        new_row = m_rows - 1;
    set_cursor(new_row, m_cursor_column);
}

void Terminal::escape$C(const Vector<unsigned>& params)
{
    int num = 1;
    if (params.size() >= 1)
        num = params[0];
    if (num == 0)
        num = 1;
    int new_column = (int)m_cursor_column + num;
    if (new_column >= m_columns)
        new_column = m_columns - 1;
    set_cursor(m_cursor_row, new_column);
}

void Terminal::escape$D(const Vector<unsigned>& params)
{
    int num = 1;
    if (params.size() >= 1)
        num = params[0];
    if (num == 0)
        num = 1;
    int new_column = (int)m_cursor_column - num;
    if (new_column < 0)
        new_column = 0;
    set_cursor(m_cursor_row, new_column);
}

void Terminal::escape$K(const Vector<unsigned>& params)
{
    int mode = 0;
    if (params.size() >= 1)
        mode = params[0];
    switch (mode) {
    case 0:
        for (int i = m_cursor_column; i < m_columns; ++i) {
            put_character_at(m_cursor_row, i, ' ');
        }
        break;
    case 1:
        // FIXME: Clear from cursor to beginning of screen.
        notImplemented();
        break;
    case 2:
        notImplemented();
        break;
    default:
        notImplemented();
        break;
    }
}

void Terminal::escape$J(const Vector<unsigned>& params)
{
    int mode = 0;
    if (params.size() >= 1)
        mode = params[0];
    switch (mode) {
    case 0:
        // FIXME: Clear from cursor to end of screen.
        notImplemented();
        break;
    case 1:
        // FIXME: Clear from cursor to beginning of screen.
        notImplemented();
        break;
    case 2:
        clear();
        break;
    case 3:
        // FIXME: <esc>[3J should also clear the scrollback buffer.
        clear();
        break;
    default:
        notImplemented();
        break;
    }
}

void Terminal::execute_escape_sequence(byte final)
{
    auto paramparts = String((const char*)m_parameters.data(), m_parameters.size()).split(';');
    Vector<unsigned> params;
    for (auto& parampart : paramparts) {
        bool ok;
        unsigned value = parseUInt(parampart, ok);
        if (!ok) {
            // FIXME: Should we do something else?
            return;
        }
        params.append(value);
    }
    switch (final) {
    case 'A': escape$A(params); break;
    case 'B': escape$B(params); break;
    case 'C': escape$C(params); break;
    case 'D': escape$D(params); break;
    case 'H': escape$H(params); break;
    case 'J': escape$J(params); break;
    case 'K': escape$K(params); break;
    case 'm': escape$m(params); break;
    case 's': escape$s(params); break;
    case 'u': escape$u(params); break;
    default:
        dbgprintf("Terminal::execute_escape_sequence: Unhandled final '%c'\n", final);
        break;
    }

    m_parameters.clear();
    m_intermediates.clear();
}

void Terminal::scroll_up()
{
    word new_row = m_cursor_row;
    if (m_cursor_row == (rows() - 1)) {
        // NOTE: We have to invalidate the cursor first.
        invalidate_cursor();
        delete m_lines[0];
        for (word row = 1; row < rows(); ++row)
            m_lines[row - 1] = m_lines[row];
        m_lines[m_rows - 1] = new Line(m_columns);
        ++m_rows_to_scroll_backing_store;
    } else {
        ++new_row;
    }
    set_cursor(new_row, 0);
}

void Terminal::set_cursor(unsigned row, unsigned column)
{
    if (row == m_cursor_row && column == m_cursor_column)
        return;
    ASSERT(row < rows());
    ASSERT(column < columns());
    invalidate_cursor();
    m_cursor_row = row;
    m_cursor_column = column;
    if (column != columns() - 1)
        m_stomp = false;
    invalidate_cursor();
}

void Terminal::put_character_at(unsigned row, unsigned column, byte ch)
{
    ASSERT(row < rows());
    ASSERT(column < columns());
    line(row).characters[column] = ch;
    line(row).attributes[column] = m_current_attribute;
}

void Terminal::on_char(byte ch)
{
#ifdef TERMINAL_DEBUG
    dbgprintf("Terminal::on_char: %b (%c)\n", ch, ch);
#endif
    switch (m_escape_state) {
    case ExpectBracket:
        if (ch == '[')
            m_escape_state = ExpectParameter;
        else
            m_escape_state = Normal;
        return;
    case ExpectParameter:
        if (is_valid_parameter_character(ch)) {
            m_parameters.append(ch);
            return;
        }
        m_escape_state = ExpectIntermediate;
        // fall through
    case ExpectIntermediate:
        if (is_valid_intermediate_character(ch)) {
            m_intermediates.append(ch);
            return;
        }
        m_escape_state = ExpectFinal;
        // fall through
    case ExpectFinal:
        if (is_valid_final_character(ch)) {
            m_escape_state = Normal;
            execute_escape_sequence(ch);
            return;
        }
        m_escape_state = Normal;
        return;
    case Normal:
        break;
    }

    switch (ch) {
    case '\0':
        return;
    case '\033':
        m_escape_state = ExpectBracket;
        return;
    case 8: // Backspace
        if (m_cursor_column) {
            set_cursor(m_cursor_row, m_cursor_column - 1);
            put_character_at(m_cursor_row, m_cursor_column, ' ');
            return;
        }
        return;
    case '\a':
        // FIXME: Bell!
        return;
    case '\t': {
        for (unsigned i = m_cursor_column; i < columns(); ++i) {
            if (m_horizontal_tabs[i]) {
                set_cursor(m_cursor_row, i);
                return;
            }
        }
        return;
    }
    case '\r':
        set_cursor(m_cursor_row, 0);
        return;
    case '\n':
        scroll_up();
        return;
    }

    auto new_column = m_cursor_column + 1;
    if (new_column < columns()) {
        put_character_at(m_cursor_row, m_cursor_column, ch);
        set_cursor(m_cursor_row, new_column);
    } else {
        if (m_stomp) {
            m_stomp = false;
            scroll_up();
            put_character_at(m_cursor_row, m_cursor_column, ch);
            set_cursor(m_cursor_row, 1);
        } else {
            // Curious: We wait once on the right-hand side
            m_stomp = true;
            put_character_at(m_cursor_row, m_cursor_column, ch);
        }
    }
}

void Terminal::set_size(word columns, word rows)
{
    m_columns = columns;
    m_rows = rows;
}

Rect Terminal::glyph_rect(word row, word column)
{
    int y = row * m_line_height;
    int x = column * font().glyph_width();
    return { x + m_inset, y + m_inset, font().glyph_width(), font().glyph_height() };
}

Rect Terminal::row_rect(word row)
{
    int y = row * m_line_height;
    return { m_inset, y + m_inset, font().glyph_width() * m_columns, font().glyph_height() };
}

inline Terminal::Attribute& Terminal::attribute_at(word row, word column)
{
    ASSERT(row < m_rows);
    ASSERT(column < m_columns);
    return line(row).attributes[column];
}

void Terminal::paint()
{
    Rect rect { 0, 0, m_pixel_width, m_pixel_height };
    Painter painter(*m_backing);

    for (size_t i = 0; i < rows(); ++i)
        line(i).needs_invalidation = false;

    if (m_rows_to_scroll_backing_store && m_rows_to_scroll_backing_store < m_rows) {
        int first_scanline = m_inset;
        int second_scanline = m_inset + (m_rows_to_scroll_backing_store * m_line_height);
        int num_rows_to_memcpy = m_rows - m_rows_to_scroll_backing_store;
        int scanlines_to_copy = (num_rows_to_memcpy * m_line_height) - m_line_spacing;
        fast_dword_copy(
            m_backing->scanline(first_scanline),
            m_backing->scanline(second_scanline),
            scanlines_to_copy * m_pixel_width
        );
        m_need_full_invalidation = true;
        attribute_at(max(0, m_cursor_row - m_rows_to_scroll_backing_store), m_cursor_column).dirty = true;
    }
    m_rows_to_scroll_backing_store = 0;

    for (word row = 0; row < m_rows; ++row) {
        for (word column = 0; column < m_columns; ++column) {
            auto& attribute = attribute_at(row, column);
            if (!attribute.dirty)
                continue;
            attribute.dirty = false;
            line(row).needs_invalidation = true;
            char ch = line(row).characters[column];
            auto character_rect = glyph_rect(row, column);
            auto character_background = ansi_color(attribute.background_color);
            painter.fill_rect(character_rect, character_background);
            if (ch == ' ')
                continue;
            painter.draw_glyph(character_rect.location(), ch, ansi_color(attribute.foreground_color));
        }
    }

    auto cursor_rect = glyph_rect(m_cursor_row, m_cursor_column);
    if (m_in_active_window)
        painter.fill_rect(cursor_rect, Color::MidGray);
    else
        painter.draw_rect(cursor_rect, Color::MidGray);

    line(m_cursor_row).needs_invalidation = true;

    if (m_belling) {
        m_need_full_invalidation = true;
        painter.draw_rect(rect, Color::Red);
    }

    if (m_need_full_invalidation) {
        invalidate_window();
        m_need_full_invalidation = false;
        return;
    }

    Rect invalidation_rect;
    for (int i = 0; i < m_rows; ++i) {
        if (line(i).needs_invalidation)
            invalidation_rect = invalidation_rect.united(row_rect(i));
    }
    invalidate_window(invalidation_rect);
}

void Terminal::invalidate_window(const Rect& a_rect)
{
    GUI_Rect rect = a_rect;
    int rc = gui_invalidate_window(m_window_id, a_rect.is_null() ? nullptr : &rect);
    if (rc < 0) {
        perror("gui_invalidate_window");
        exit(1);
    }
}

void Terminal::set_in_active_window(bool b)
{
    if (m_in_active_window == b)
        return;
    m_in_active_window = b;
    invalidate_cursor();
    paint();
}

void Terminal::invalidate_cursor()
{
    attribute_at(m_cursor_row, m_cursor_column).dirty = true;
}
