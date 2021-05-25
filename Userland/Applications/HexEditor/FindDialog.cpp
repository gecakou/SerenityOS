/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FindDialog.h"
#include <AK/Hex.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <Applications/HexEditor/FindDialogGML.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/Label.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/RadioButton.h>
#include <LibGUI/TextBox.h>
#include <LibGUI/Widget.h>

struct Option {
    String title;
    OptionId opt;
    bool enabled;
    bool default_action;
};

static const Vector<Option> options = {
    { "ASCII String", OPTION_ASCII_STRING, true, true },
    { "Hex value", OPTION_HEX_VALUE, true, false },
};

int FindDialog::show(GUI::Window* parent_window, String& out_text, ByteBuffer& out_buffer)
{
    auto dialog = FindDialog::construct();

    if (parent_window)
        dialog->set_icon(parent_window->icon());

    if (!out_text.is_empty() && !out_text.is_null())
        dialog->m_text_editor->set_text(out_text);

    auto result = dialog->exec();

    if (result != GUI::Dialog::ExecOK)
        return result;

    auto processed = dialog->process_input(dialog->text_value(), dialog->selected_option());

    out_text = dialog->text_value();

    if (processed.is_error()) {
        GUI::MessageBox::show_error(parent_window, processed.error());
        result = GUI::Dialog::ExecAborted;
    } else {
        out_buffer = move(processed.value());
    }

    dbgln("Find: value={} option={}", dialog->text_value().characters(), (int)dialog->selected_option());
    return result;
}

Result<ByteBuffer, String> FindDialog::process_input(String text_value, OptionId opt)
{
    dbgln("process_input opt={}", (int)opt);
    switch (opt) {
    case OPTION_ASCII_STRING: {
        if (text_value.is_empty())
            return String("Input is empty");

        return text_value.to_byte_buffer();
    }

    case OPTION_HEX_VALUE: {
        text_value.replace(" ", "", true);
        auto decoded = decode_hex(text_value.substring_view(0, text_value.length()));
        if (!decoded.has_value())
            return String("Input contains invalid hex values.");

        return decoded.value();
    }

    default:
        VERIFY_NOT_REACHED();
    }
}

FindDialog::FindDialog()
    : Dialog(nullptr)
{
    resize(280, 146);
    center_on_screen();
    set_resizable(false);
    set_title("Find");

    auto& main_widget = set_main_widget<GUI::Widget>();
    if (!main_widget.load_from_gml(find_dialog_gml))
        VERIFY_NOT_REACHED();

    m_text_editor = *main_widget.find_descendant_of_type_named<GUI::TextBox>("text_editor");
    m_ok_button = *main_widget.find_descendant_of_type_named<GUI::Button>("ok_button");
    m_cancel_button = *main_widget.find_descendant_of_type_named<GUI::Button>("cancel_button");

    auto& radio_container = *main_widget.find_descendant_of_type_named<GUI::Widget>("radio_container");
    for (size_t i = 0; i < options.size(); i++) {
        auto action = options[i];
        auto& radio = radio_container.add<GUI::RadioButton>();
        radio.set_enabled(action.enabled);
        radio.set_text(action.title);

        radio.on_checked = [this, i](auto) {
            m_selected_option = options[i].opt;
        };

        if (action.default_action) {
            radio.set_checked(true);
            m_selected_option = options[i].opt;
        }
    }

    m_text_editor->on_return_pressed = [this] {
        m_ok_button->click();
    };

    m_ok_button->on_click = [this](auto) {
        m_text_value = m_text_editor->text();
        done(ExecResult::ExecOK);
    };

    m_cancel_button->on_click = [this](auto) {
        done(ExecResult::ExecCancel);
    };
}

FindDialog::~FindDialog()
{
}
