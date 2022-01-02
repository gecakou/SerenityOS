/*
 * Copyright (c) 2021, Peter Elliott <pelliott@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "../Taskbar/ShutdownDialog.h"
#include <LibCore/System.h>
#include <LibGUI/Icon.h>
#include <LibGUI/Widget.h>
#include <Services/LoginServer/LoginWindow.h>
#include <Services/LoginServer/LoginWindowGML.h>
#include <serenity.h>

LoginWindow::LoginWindow(GUI::Window* parent)
    : GUI::Window(parent)
{
    set_title("Log in to SerenityOS");
    resize(413, 170);
    center_on_screen();
    set_resizable(false);
    set_minimizable(false);
    set_closeable(false);
    set_icon(GUI::Icon::default_icon("ladyball").bitmap_for_size(16));

    auto& widget = set_main_widget<GUI::Widget>();
    widget.load_from_gml(login_window_gml);
    m_banner = *widget.find_descendant_of_type_named<GUI::ImageWidget>("banner");
    m_banner->load_from_file("/res/graphics/brand-banner.png");
    m_banner->set_auto_resize(true);

    m_username = *widget.find_descendant_of_type_named<GUI::TextBox>("username");
    m_username->set_focus(true);
    m_password = *widget.find_descendant_of_type_named<GUI::PasswordBox>("password");

    m_log_in_button = *widget.find_descendant_of_type_named<GUI::Button>("log_in");
    m_log_in_button->on_click = [&](auto) {
        if (on_submit)
            on_submit();
    };

    m_power_button = *widget.find_descendant_of_type_named<GUI::Button>("power");
    m_power_button->on_click = [](auto) {
        auto command = ShutdownDialog::show(static_cast<int>(ShutdownDialog::ActionCode::Logout));
        if (command.is_empty())
            return;
        auto child_pid = MUST(Core::System::posix_spawn(command[0], nullptr, nullptr, const_cast<char**>(command.data()), environ));
        VERIFY(!disown(child_pid));
    };

    m_username->on_return_pressed = [&]() { m_log_in_button->click(); };
    m_password->on_return_pressed = [&]() { m_log_in_button->click(); };
}
