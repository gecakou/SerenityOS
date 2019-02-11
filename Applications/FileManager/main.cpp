#include <LibGUI/GWindow.h>
#include <LibGUI/GWidget.h>
#include <LibGUI/GBoxLayout.h>
#include <LibGUI/GApplication.h>
#include <LibGUI/GStatusBar.h>
#include <unistd.h>
#include <stdio.h>
#include "DirectoryView.h"

static GWindow* make_window();

int main(int argc, char** argv)
{
    GApplication app(argc, argv);

    auto* window = make_window();
    window->set_should_exit_app_on_close(true);
    window->show();

    return app.exec();
}

GWindow* make_window()
{
    auto* window = new GWindow;
    window->set_title("FileManager");
    window->set_rect(20, 200, 240, 300);

    auto* widget = new GWidget;
    window->set_main_widget(widget);

    widget->set_layout(make<GBoxLayout>(Orientation::Vertical));

    auto* directory_view = new DirectoryView(widget);

    auto* statusbar = new GStatusBar(widget);
    statusbar->set_text("Welcome!");

    directory_view->on_path_change = [window] (const String& new_path) {
        window->set_title(String::format("FileManager: %s", new_path.characters()));
    };

    directory_view->on_status_message = [statusbar] (String message) {
        statusbar->set_text(move(message));
    };

    directory_view->open("/");

    return window;
}

