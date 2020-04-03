/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
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

#include <LibGUI/AbstractButton.h>
#include <LibGUI/Dialog.h>

namespace GUI {


class ColorButton : public AbstractButton {
    C_OBJECT(ColorButton)

public:
    explicit ColorButton(Color color = {});
    virtual ~ColorButton() override;

    void set_selected(bool selected);
    Color color() const { return m_color; }

    Function<void(const Color)> on_click;

protected:
    virtual void click() override;
    virtual void paint_event(PaintEvent&) override;

private:
    Color m_color;
    bool m_selected;
};


class CustomColor final : public GUI::Widget {
    C_OBJECT(CustomColor);

public:
    CustomColor();

    Function<void(Color)> on_pick;
    void clear_last_position();

private:
    RefPtr<Gfx::Bitmap> m_custom_colors;
    bool m_status = false;
    Gfx::Point m_last_position;

    void fire_event(GUI::MouseEvent& event);

    virtual void mousedown_event(GUI::MouseEvent&) override;
    virtual void mouseup_event(GUI::MouseEvent&) override;
    virtual void mousemove_event(GUI::MouseEvent&) override;
    virtual void paint_event(GUI::PaintEvent&) override;
    virtual void resize_event(ResizeEvent&) override;
};


class ColorPicker final : public Dialog {
    C_OBJECT(ColorPicker)

public:
    virtual ~ColorPicker() override;

    Color color() const { return m_color; }

private:
    explicit ColorPicker(Color, Window* parent_window = nullptr, String title = "Edit Color");

    void build_ui();
    void build_ui_custom(Widget& root_container);
    void build_ui_palette(Widget& root_container);
    void update_color_widgets();
    void create_color_button(Widget& container, unsigned rgb);

    Color m_color;

    Vector<ColorButton*> m_color_widgets;
    RefPtr<Widget> m_manin_container;
    RefPtr<CustomColor> m_custom_color;
    RefPtr<Frame> m_preview_widget;
    RefPtr<TextBox> m_html_text;
    RefPtr<SpinBox> m_red_spinbox;
    RefPtr<SpinBox> m_green_spinbox;
    RefPtr<SpinBox> m_blue_spinbox;
};

}
