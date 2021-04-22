/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "TextEditorWidget.h"
#include <LibCore/ArgsParser.h>
#include <LibGUI/Menubar.h>
#include <LibGfx/Bitmap.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (pledge("stdio recvfd sendfd thread rpath accept cpath wpath unix fattr", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    auto app = GUI::Application::construct(argc, argv);

    if (pledge("stdio recvfd sendfd thread rpath accept cpath wpath unix", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    const char* preview_mode = "auto";
    int initial_line_number = 0;
    const char* file_to_edit = nullptr;
    Core::ArgsParser parser;
    parser.add_option(preview_mode, "Preview mode, one of 'none', 'html', 'markdown', 'auto'", "preview-mode", '\0', "mode");
    parser.add_option(initial_line_number, "Start at line number", "line-number", 'l', "line");
    parser.add_positional_argument(file_to_edit, "File to edit", "file", Core::ArgsParser::Required::No);

    parser.parse(argc, argv);

    StringView preview_mode_view = preview_mode;

    auto app_icon = GUI::Icon::default_icon("app-text-editor");

    auto window = GUI::Window::construct();
    window->resize(640, 400);

    auto& text_widget = window->set_main_widget<TextEditorWidget>();

    text_widget.editor().set_focus(true);

    window->on_close_request = [&]() -> GUI::Window::CloseRequestDecision {
        if (text_widget.request_close())
            return GUI::Window::CloseRequestDecision::Close;
        return GUI::Window::CloseRequestDecision::StayOpen;
    };

    if (preview_mode_view == "auto") {
        text_widget.set_auto_detect_preview_mode(true);
    } else if (preview_mode_view == "markdown") {
        text_widget.set_preview_mode(TextEditorWidget::PreviewMode::Markdown);
    } else if (preview_mode_view == "html") {
        text_widget.set_preview_mode(TextEditorWidget::PreviewMode::HTML);
    } else if (preview_mode_view == "none") {
        text_widget.set_preview_mode(TextEditorWidget::PreviewMode::None);
    } else {
        warnln("Invalid mode '{}'", preview_mode);
        return 1;
    }

    auto menubar = GUI::Menubar::construct();
    text_widget.initialize_menubar(menubar);
    window->set_menubar(menubar);

    if (file_to_edit)
        if (!text_widget.open_file(file_to_edit))
            return 1;

    text_widget.update_title();

    if (initial_line_number != 0)
        text_widget.editor().set_cursor_and_focus_line(initial_line_number - 1, 0);

    window->show();
    window->set_icon(app_icon.bitmap_for_size(16));

    window->set_menubar(menubar);

    return app->exec();
}
