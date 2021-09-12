/*
 * Copyright (c) 2019-2020, Jesse Buhagiar <jooster669@gmail.com>
 * Copyright (c) 2020-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "MonitorSettingsWidget.h"
#include <Applications/DisplaySettings/MonitorSettingsGML.h>
#include <LibGUI/Button.h>
#include <LibGUI/ComboBox.h>
#include <LibGUI/ItemListModel.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/RadioButton.h>
#include <LibGUI/WindowServerConnection.h>

namespace DisplaySettings {

MonitorSettingsWidget::MonitorSettingsWidget()
{
    create_resolution_list();
    create_frame();
    load_current_settings();
}

void MonitorSettingsWidget::create_resolution_list()
{
    // TODO: Find a better way to get the default resolution
    m_resolutions.append({ 640, 480 });
    m_resolutions.append({ 800, 600 });
    m_resolutions.append({ 1024, 768 });
    m_resolutions.append({ 1280, 720 });
    m_resolutions.append({ 1280, 768 });
    m_resolutions.append({ 1280, 960 });
    m_resolutions.append({ 1280, 1024 });
    m_resolutions.append({ 1360, 768 });
    m_resolutions.append({ 1368, 768 });
    m_resolutions.append({ 1440, 900 });
    m_resolutions.append({ 1600, 900 });
    m_resolutions.append({ 1600, 1200 });
    m_resolutions.append({ 1920, 1080 });
    m_resolutions.append({ 2048, 1152 });
    m_resolutions.append({ 2560, 1080 });
    m_resolutions.append({ 2560, 1440 });
}

void MonitorSettingsWidget::create_frame()
{
    load_from_gml(monitor_settings_window_gml);

    m_monitor_widget = *find_descendant_of_type_named<DisplaySettings::MonitorWidget>("monitor_widget");

    m_screen_combo = *find_descendant_of_type_named<GUI::ComboBox>("screen_combo");
    m_screen_combo->set_only_allow_values_from_model(true);
    m_screen_combo->set_model(*GUI::ItemListModel<String>::create(m_screens));
    m_screen_combo->on_change = [this](auto&, const GUI::ModelIndex& index) {
        m_selected_screen_index = index.row();
        selected_screen_index_changed();
    };

    m_resolution_combo = *find_descendant_of_type_named<GUI::ComboBox>("resolution_combo");
    m_resolution_combo->set_only_allow_values_from_model(true);
    m_resolution_combo->set_model(*GUI::ItemListModel<Gfx::IntSize>::create(m_resolutions));
    m_resolution_combo->on_change = [this](auto&, const GUI::ModelIndex& index) {
        auto& selected_screen = m_screen_layout.screens[m_selected_screen_index];
        selected_screen.resolution = m_resolutions.at(index.row());
        // Try to auto re-arrange things if there are overlaps or disconnected screens
        m_screen_layout.normalize();
        m_monitor_widget->set_desktop_resolution(selected_screen.resolution);
        m_monitor_widget->update();
    };

    m_display_scale_radio_1x = *find_descendant_of_type_named<GUI::RadioButton>("scale_1x");
    m_display_scale_radio_1x->on_checked = [this](bool checked) {
        if (checked) {
            auto& selected_screen = m_screen_layout.screens[m_selected_screen_index];
            selected_screen.scale_factor = 1;
            // Try to auto re-arrange things if there are overlaps or disconnected screens
            m_screen_layout.normalize();
            m_monitor_widget->set_desktop_scale_factor(1);
            m_monitor_widget->update();
        }
    };
    m_display_scale_radio_2x = *find_descendant_of_type_named<GUI::RadioButton>("scale_2x");
    m_display_scale_radio_2x->on_checked = [this](bool checked) {
        if (checked) {
            auto& selected_screen = m_screen_layout.screens[m_selected_screen_index];
            selected_screen.scale_factor = 2;
            // Try to auto re-arrange things if there are overlaps or disconnected screens
            m_screen_layout.normalize();
            m_monitor_widget->set_desktop_scale_factor(2);
            m_monitor_widget->update();
        }
    };
}

void MonitorSettingsWidget::load_current_settings()
{
    m_screen_layout = GUI::WindowServerConnection::the().get_screen_layout();

    m_screens.clear();
    for (size_t i = 0; i < m_screen_layout.screens.size(); i++) {
        if (i == m_screen_layout.main_screen_index)
            m_screens.append(String::formatted("{}: {} (main screen)", i + 1, m_screen_layout.screens[i].device));
        else
            m_screens.append(String::formatted("{}: {}", i + 1, m_screen_layout.screens[i].device));
    }
    m_selected_screen_index = m_screen_layout.main_screen_index;
    m_screen_combo->set_selected_index(m_selected_screen_index);
    selected_screen_index_changed();
}

void MonitorSettingsWidget::selected_screen_index_changed()
{
    auto& screen = m_screen_layout.screens[m_selected_screen_index];
    if (screen.scale_factor != 1 && screen.scale_factor != 2) {
        dbgln("unexpected ScaleFactor {}, setting to 1", screen.scale_factor);
        screen.scale_factor = 1;
    }
    (screen.scale_factor == 1 ? m_display_scale_radio_1x : m_display_scale_radio_2x)->set_checked(true);
    m_monitor_widget->set_desktop_scale_factor(screen.scale_factor);

    // Let's attempt to find the current resolution and select it!
    auto index = m_resolutions.find_first_index(screen.resolution).value_or(0);
    Gfx::IntSize current_resolution = m_resolutions.at(index);
    m_monitor_widget->set_desktop_resolution(current_resolution);
    m_resolution_combo->set_selected_index(index);

    m_monitor_widget->update();
}

void MonitorSettingsWidget::apply_settings()
{
    // Fetch the latest configuration again, in case it has been changed by someone else.
    // This isn't technically race free, but if the user automates changing settings we can't help...
    auto current_layout = GUI::WindowServerConnection::the().get_screen_layout();
    if (m_screen_layout != current_layout) {
        auto result = GUI::WindowServerConnection::the().set_screen_layout(m_screen_layout, false);
        if (result.success()) {
            auto box = GUI::MessageBox::construct(window(), String::formatted("Do you want to keep the new settings? They will be reverted after 10 seconds."),
                "Apply new screen layout", GUI::MessageBox::Type::Question, GUI::MessageBox::InputType::YesNo);
            box->set_icon(window()->icon());

            // If after 10 seconds the user doesn't close the message box, just close it.
            auto timer = Core::Timer::construct(10000, [&] {
                box->close();
            });

            // If the user selects "No", closes the window or the window gets closed by the 10 seconds timer, revert the changes.
            if (box->exec() == GUI::MessageBox::ExecYes) {
                auto save_result = GUI::WindowServerConnection::the().save_screen_layout();
                if (!save_result.success()) {
                    GUI::MessageBox::show(window(), String::formatted("Error saving settings: {}", save_result.error_msg()),
                        "Unable to save setting", GUI::MessageBox::Type::Error);
                }
            } else {
                auto restore_result = GUI::WindowServerConnection::the().set_screen_layout(current_layout, false);
                if (!restore_result.success()) {
                    GUI::MessageBox::show(window(), String::formatted("Error restoring settings: {}", restore_result.error_msg()),
                        "Unable to restore setting", GUI::MessageBox::Type::Error);
                } else {
                    load_current_settings();
                }
            }
        } else {
            GUI::MessageBox::show(window(), String::formatted("Error setting screen layout: {}", result.error_msg()),
                "Unable to apply changes", GUI::MessageBox::Type::Error);
        }
    }
}

void MonitorSettingsWidget::show_screen_numbers(bool show)
{
    if (m_showing_screen_numbers == show)
        return;
    m_showing_screen_numbers = show;
    GUI::WindowServerConnection::the().async_show_screen_numbers(show);
}

}
