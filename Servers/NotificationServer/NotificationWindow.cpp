/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
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

#include "NotificationWindow.h"
#include <AK/HashTable.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/Desktop.h>
#include <LibGUI/Label.h>
#include <LibGUI/Widget.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Font.h>

namespace NotificationServer {

static HashTable<RefPtr<NotificationWindow>> s_windows;

NotificationWindow::NotificationWindow(const String& text, const String& title, const String& icon_path)
{
    s_windows.set(this);

    set_window_type(GUI::WindowType::Tooltip);

    Gfx::Rect lowest_notification_rect_on_screen;
    for (auto& window : s_windows) {
        if (window->m_original_rect.y() > lowest_notification_rect_on_screen.y())
            lowest_notification_rect_on_screen = window->m_original_rect;
    }

    Gfx::Rect rect;
    rect.set_width(240);
    rect.set_height(40);
    rect.set_location(GUI::Desktop::the().rect().top_right().translated(-rect.width() - 8, 26));

    if (!lowest_notification_rect_on_screen.is_null())
        rect.set_location(lowest_notification_rect_on_screen.bottom_left().translated(0, 8));

    set_rect(rect);

    m_original_rect = rect;

    auto& widget = set_main_widget<GUI::Widget>();
    widget.set_fill_with_background_color(true);

    widget.set_layout<GUI::HorizontalBoxLayout>();
    widget.layout()->set_margins({ 8, 8, 8, 8 });
    widget.layout()->set_spacing(6);

    if (auto icon = Gfx::Bitmap::load_from_file(icon_path)) {
        auto& icon_label = widget.add<GUI::Label>();
        icon_label.set_size_policy(GUI::SizePolicy::Fixed, GUI::SizePolicy::Fixed);
        icon_label.set_preferred_size(32, 32);
        icon_label.set_icon(icon);
    }

    auto& left_container = widget.add<GUI::Widget>();
    left_container.set_layout<GUI::VerticalBoxLayout>();

    auto& title_label = left_container.add<GUI::Label>(title);
    title_label.set_font(Gfx::Font::default_bold_font());
    title_label.set_text_alignment(Gfx::TextAlignment::CenterLeft);
    auto& text_label = left_container.add<GUI::Label>(text);
    text_label.set_text_alignment(Gfx::TextAlignment::CenterLeft);

    auto& right_container = widget.add<GUI::Widget>();
    right_container.set_size_policy(GUI::SizePolicy::Fixed, GUI::SizePolicy::Fill);
    right_container.set_preferred_size(36, 0);
    right_container.set_layout<GUI::HorizontalBoxLayout>();

    auto& button = right_container.add<GUI::Button>("Okay");
    button.on_click = [this] {
        s_windows.remove(this);
        close();
    };
}

NotificationWindow::~NotificationWindow()
{
}

}
