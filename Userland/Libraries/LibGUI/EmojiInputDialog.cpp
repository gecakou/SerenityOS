/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/LexicalPath.h>
#include <AK/StringBuilder.h>
#include <AK/Utf32View.h>
#include <LibCore/DirIterator.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/EmojiInputDialog.h>
#include <LibGUI/EmojiInputDialogGML.h>
#include <LibGUI/Event.h>
#include <LibGUI/Frame.h>
#include <LibGUI/ScrollableContainerWidget.h>
#include <stdlib.h>

namespace GUI {

static Vector<u32> supported_emoji_code_points()
{
    Vector<u32> code_points;
    Core::DirIterator dt("/res/emoji", Core::DirIterator::SkipDots);
    while (dt.has_next()) {
        auto filename = dt.next_path();
        auto lexical_path = LexicalPath(filename);
        if (lexical_path.extension() != "png")
            continue;
        auto basename = lexical_path.basename();
        if (!basename.starts_with("U+"sv))
            continue;
        // FIXME: Handle multi code point emojis.
        if (basename.contains('_'))
            continue;
        u32 code_point = strtoul(basename.to_string().characters() + 2, nullptr, 16);
        code_points.append(code_point);
    }
    return code_points;
}

EmojiInputDialog::EmojiInputDialog(Window* parent_window)
    : Dialog(parent_window)
{
    auto& main_widget = set_main_widget<Frame>();
    if (!main_widget.load_from_gml(emoji_input_dialog_gml))
        VERIFY_NOT_REACHED();

    set_frameless(true);
    resize(400, 300);

    auto& scrollable_container = *main_widget.find_descendant_of_type_named<GUI::ScrollableContainerWidget>("scrollable_container"sv);
    m_emojis_widget = main_widget.find_descendant_of_type_named<GUI::Widget>("emojis"sv);
    m_code_points = supported_emoji_code_points();

    scrollable_container.horizontal_scrollbar().set_visible(false);
    update_displayed_emoji();

    on_active_window_change = [this](bool is_active_window) {
        if (!is_active_window)
            close();
    };
}

void EmojiInputDialog::update_displayed_emoji()
{
    constexpr int button_size = 20;
    constexpr size_t columns = 18;
    size_t rows = ceil_div(m_code_points.size(), columns);
    size_t index = 0;

    for (size_t row = 0; row < rows && index < m_code_points.size(); ++row) {
        auto& horizontal_container = m_emojis_widget->add<Widget>();
        auto& horizontal_layout = horizontal_container.set_layout<HorizontalBoxLayout>();
        horizontal_layout.set_spacing(0);
        for (size_t column = 0; column < columns; ++column) {
            if (index < m_code_points.size()) {
                // FIXME: Also emit U+FE0F for single code point emojis, currently
                // they get shown as text glyphs if available.
                // This will require buttons to don't calculate their length as 2,
                // currently it just shows an ellipsis. It will also require some
                // tweaking of the mechanism that is currently being used to insert
                // which is a key event with a single code point.
                StringBuilder builder;
                builder.append(Utf32View(&m_code_points[index++], 1));
                auto emoji_text = builder.to_string();
                auto& button = horizontal_container.add<Button>(emoji_text);
                button.set_fixed_size(button_size, button_size);
                button.set_button_style(Gfx::ButtonStyle::Coolbar);
                button.on_click = [this, button = &button](auto) {
                    m_selected_emoji_text = button->text();
                    done(ExecResult::OK);
                };
            } else {
                horizontal_container.add<Widget>();
            }
        }
    }
}

void EmojiInputDialog::event(Core::Event& event)
{
    if (event.type() == Event::KeyDown) {
        auto& key_event = static_cast<KeyEvent&>(event);
        if (key_event.key() == Key_Escape) {
            done(ExecResult::Cancel);
            return;
        }
    }
    Dialog::event(event);
}

}
