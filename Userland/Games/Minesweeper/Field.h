/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Noncopyable.h>
#include <LibCore/Timer.h>
#include <LibGUI/Frame.h>
#include <LibGfx/Palette.h>

class Field;
class SquareButton;
class SquareLabel;

class Square {
    AK_MAKE_NONCOPYABLE(Square);

public:
    Square();
    ~Square();

    Field* field { nullptr };
    bool is_swept { false };
    bool has_mine { false };
    bool has_flag { false };
    bool is_considering { false };
    size_t row { 0 };
    size_t column { 0 };
    size_t number { 0 };
    RefPtr<SquareButton> button;
    RefPtr<SquareLabel> label;

    template<typename Callback>
    void for_each_neighbor(Callback);
};

class Field final : public GUI::Frame {
    C_OBJECT(Field)
    friend class Square;
    friend class SquareLabel;

public:
    Field(GUI::Label& flag_label, GUI::Label& time_label, GUI::Button& face_button, Function<void(Gfx::IntSize)> on_size_changed);
    virtual ~Field() override;

    size_t rows() const { return m_rows; }
    size_t columns() const { return m_columns; }
    size_t mine_count() const { return m_mine_count; }
    int square_size() const { return 15; }
    bool is_single_chording() const { return m_single_chording; }

    void set_field_size(size_t rows, size_t columns, size_t mine_count);
    void set_single_chording(bool new_val);

    void reset();

private:
    virtual void paint_event(GUI::PaintEvent&) override;

    void on_square_clicked(Square&);
    void on_square_right_clicked(Square&);
    void on_square_middle_clicked(Square&);
    void on_square_chorded(Square&);
    void game_over();
    void win();
    void reveal_mines();
    void set_chord_preview(Square&, bool);
    void set_flag(Square&, bool);

    Square& square(size_t row, size_t column) { return *m_squares[row * columns() + column]; }
    const Square& square(size_t row, size_t column) const { return *m_squares[row * columns() + column]; }

    void flood_fill(Square&);
    void on_square_clicked_impl(Square&, bool);

    template<typename Callback>
    void for_each_square(Callback);

    enum class Face {
        Default,
        Good,
        Bad
    };
    void set_face(Face);

    size_t m_rows { 0 };
    size_t m_columns { 0 };
    size_t m_mine_count { 0 };
    size_t m_unswept_empties { 0 };
    Vector<OwnPtr<Square>> m_squares;
    RefPtr<Gfx::Bitmap> m_mine_bitmap;
    RefPtr<Gfx::Bitmap> m_flag_bitmap;
    RefPtr<Gfx::Bitmap> m_badflag_bitmap;
    RefPtr<Gfx::Bitmap> m_consider_bitmap;
    RefPtr<Gfx::Bitmap> m_default_face_bitmap;
    RefPtr<Gfx::Bitmap> m_good_face_bitmap;
    RefPtr<Gfx::Bitmap> m_bad_face_bitmap;
    RefPtr<Gfx::Bitmap> m_number_bitmap[8];
    Gfx::Palette m_mine_palette;
    GUI::Button& m_face_button;
    GUI::Label& m_flag_label;
    GUI::Label& m_time_label;
    RefPtr<Core::Timer> m_timer;
    size_t m_time_elapsed { 0 };
    size_t m_flags_left { 0 };
    Face m_face { Face::Default };
    bool m_chord_preview { false };
    bool m_first_click { true };
    bool m_single_chording { true };
    Function<void(Gfx::IntSize)> m_on_size_changed;
};
