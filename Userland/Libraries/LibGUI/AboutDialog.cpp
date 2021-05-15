/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <LibCore/ConfigFile.h>
#include <LibGUI/AboutDialog.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/ImageWidget.h>
#include <LibGUI/Label.h>
#include <LibGUI/Widget.h>
#include <LibGfx/Font.h>
#include <LibGfx/FontDatabase.h>

namespace GUI {

AboutDialog::AboutDialog(const StringView& name, const Gfx::Bitmap* icon, Window* parent_window)
    : Dialog(parent_window)
    , m_name(name)
    , m_icon(icon)
{
    resize(413, 205);
    set_title(String::formatted("About {}", m_name));
    set_resizable(false);

    if (parent_window)
        set_icon(parent_window->icon());

    auto& widget = set_main_widget<Widget>();
    widget.set_fill_with_background_color(true);
    widget.set_layout<VerticalBoxLayout>();
    widget.layout()->set_spacing(0);

    auto& banner_image = widget.add<GUI::ImageWidget>();
    banner_image.load_from_file("/res/graphics/brand-banner.png");

    auto& content_container = widget.add<Widget>();
    content_container.set_layout<HorizontalBoxLayout>();

    auto& left_container = content_container.add<Widget>();
    left_container.set_fixed_width(60);
    left_container.set_layout<VerticalBoxLayout>();
    left_container.layout()->set_margins({ 0, 12, 0, 0 });

    if (icon) {
        auto& icon_wrapper = left_container.add<Widget>();
        icon_wrapper.set_fixed_size(32, 48);
        icon_wrapper.set_layout<VerticalBoxLayout>();

        auto& icon_image = icon_wrapper.add<ImageWidget>();
        icon_image.set_bitmap(m_icon);
    }

    auto& right_container = content_container.add<Widget>();
    right_container.set_layout<VerticalBoxLayout>();
    right_container.layout()->set_margins({ 0, 12, 12, 8 });

    auto make_label = [&](const StringView& text, bool bold = false) {
        auto& label = right_container.add<Label>(text);
        label.set_text_alignment(Gfx::TextAlignment::CenterLeft);
        label.set_fixed_height(14);
        if (bold)
            label.set_font(Gfx::FontDatabase::default_bold_font());
    };
    make_label(m_name, true);
    // If we are displaying a dialog for an application, insert 'SerenityOS' below the application name
    if (m_name != "SerenityOS")
        make_label("SerenityOS");
    make_label(version_string());
    make_label("Copyright \xC2\xA9 the SerenityOS developers, 2018-2021");

    right_container.layout()->add_spacer();

    auto& button_container = right_container.add<Widget>();
    button_container.set_fixed_height(23);
    button_container.set_layout<HorizontalBoxLayout>();
    button_container.layout()->add_spacer();
    auto& ok_button = button_container.add<Button>("OK");
    ok_button.set_fixed_size(80, 23);
    ok_button.on_click = [this](auto) {
        done(Dialog::ExecOK);
    };
}

AboutDialog::~AboutDialog()
{
}

String AboutDialog::version_string() const
{
    auto version_config = Core::ConfigFile::open("/res/version.ini");
    auto major_version = version_config->read_entry("Version", "Major", "0");
    auto minor_version = version_config->read_entry("Version", "Minor", "0");
    auto git_version = version_config->read_entry("Version", "Git", "");

    StringBuilder builder;
    builder.append("Version ");
    builder.append(major_version);
    builder.append('.');
    builder.append(minor_version);

    if (git_version != "") {
        builder.append(".g");
        builder.append(git_version);
    }

    return builder.to_string();
}

}
