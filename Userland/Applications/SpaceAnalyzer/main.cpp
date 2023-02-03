/*
 * Copyright (c) 2021-2022, the SerenityOS developers.
 * Copyright (c) 2023, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Tree.h"
#include "TreeMapWidget.h"
#include <AK/LexicalPath.h>
#include <AK/String.h>
#include <AK/URL.h>
#include <Applications/SpaceAnalyzer/SpaceAnalyzerGML.h>
#include <LibCore/File.h>
#include <LibDesktop/Launcher.h>
#include <LibGUI/Application.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Breadcrumbbar.h>
#include <LibGUI/Clipboard.h>
#include <LibGUI/FileIconProvider.h>
#include <LibGUI/Icon.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Statusbar.h>
#include <LibGfx/Bitmap.h>
#include <LibMain/Main.h>

static constexpr auto APP_NAME = "Space Analyzer"sv;

static DeprecatedString get_absolute_path_to_selected_node(SpaceAnalyzer::TreeMapWidget const& treemapwidget, bool include_last_node = true)
{
    StringBuilder path_builder;
    for (size_t k = 0; k < treemapwidget.path_size() - (include_last_node ? 0 : 1); k++) {
        if (k != 0) {
            path_builder.append('/');
        }
        TreeNode const* node = treemapwidget.path_node(k);
        path_builder.append(node->name());
    }
    return path_builder.to_deprecated_string();
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    auto app = TRY(GUI::Application::try_create(arguments));

    // Configure application window.
    auto app_icon = GUI::Icon::default_icon("app-space-analyzer"sv);
    auto window = TRY(GUI::Window::try_create());
    window->set_title(APP_NAME);
    window->resize(640, 480);
    window->set_icon(app_icon.bitmap_for_size(16));

    // Load widgets.
    auto mainwidget = TRY(window->set_main_widget<GUI::Widget>());
    TRY(mainwidget->load_from_gml(space_analyzer_gml));
    auto& breadcrumbbar = *mainwidget->find_descendant_of_type_named<GUI::Breadcrumbbar>("breadcrumbbar");
    auto& treemapwidget = *mainwidget->find_descendant_of_type_named<SpaceAnalyzer::TreeMapWidget>("tree_map");
    auto& statusbar = *mainwidget->find_descendant_of_type_named<GUI::Statusbar>("statusbar");

    treemapwidget.set_focus(true);

    auto file_menu = TRY(window->try_add_menu("&File"));
    TRY(file_menu->try_add_action(GUI::Action::create("&Analyze", [&](auto&) {
        // FIXME: Just modify the tree in memory instead of traversing the entire file system
        if (auto result = treemapwidget.analyze(statusbar); result.is_error()) {
            GUI::MessageBox::show_error(window, DeprecatedString::formatted("{}", result.error()));
        }
    })));
    TRY(file_menu->try_add_separator());
    TRY(file_menu->try_add_action(GUI::CommonActions::make_quit_action([&](auto&) {
        app->quit();
    })));

    auto help_menu = TRY(window->try_add_menu("&Help"));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_command_palette_action(window)));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_about_action(APP_NAME, app_icon, window)));

    auto open_icon = TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/open.png"sv));
    // Configure the nodes context menu.
    auto open_folder_action = GUI::Action::create("Open Folder", { Mod_Ctrl, Key_O }, open_icon, [&](auto&) {
        Desktop::Launcher::open(URL::create_with_file_scheme(get_absolute_path_to_selected_node(treemapwidget)));
    });
    auto open_containing_folder_action = GUI::Action::create("Open Containing Folder", { Mod_Ctrl, Key_O }, open_icon, [&](auto&) {
        LexicalPath path { get_absolute_path_to_selected_node(treemapwidget) };
        Desktop::Launcher::open(URL::create_with_file_scheme(path.dirname(), path.basename()));
    });

    auto copy_icon = TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/edit-copy.png"sv));
    auto copy_path_action = GUI::Action::create("Copy Path to Clipboard", { Mod_Ctrl, Key_C }, copy_icon, [&](auto&) {
        GUI::Clipboard::the().set_plain_text(get_absolute_path_to_selected_node(treemapwidget));
    });
    auto delete_action = GUI::CommonActions::make_delete_action([&](auto&) {
        DeprecatedString selected_node_path = get_absolute_path_to_selected_node(treemapwidget);
        bool try_again = true;
        while (try_again) {
            try_again = false;

            auto deletion_result = Core::File::remove(selected_node_path, Core::File::RecursionMode::Allowed);
            if (deletion_result.is_error()) {
                auto retry_message_result = GUI::MessageBox::show(window,
                    DeprecatedString::formatted("Failed to delete \"{}\": {}. Retry?",
                        selected_node_path,
                        deletion_result.error()),
                    "Deletion failed"sv,
                    GUI::MessageBox::Type::Error,
                    GUI::MessageBox::InputType::YesNo);
                if (retry_message_result == GUI::MessageBox::ExecResult::Yes) {
                    try_again = true;
                }
            } else {
                GUI::MessageBox::show(window,
                    DeprecatedString::formatted("Successfully deleted \"{}\".", selected_node_path),
                    "Deletion completed"sv,
                    GUI::MessageBox::Type::Information,
                    GUI::MessageBox::InputType::OK);
            }
        }

        if (auto result = treemapwidget.analyze(statusbar); result.is_error()) {
            GUI::MessageBox::show_error(window, DeprecatedString::formatted("{}", result.error()));
        }
    });

    auto context_menu = TRY(GUI::Menu::try_create());
    TRY(context_menu->try_add_action(open_folder_action));
    TRY(context_menu->try_add_action(open_containing_folder_action));
    TRY(context_menu->try_add_action(copy_path_action));
    TRY(context_menu->try_add_action(delete_action));

    // Configure event handlers.
    breadcrumbbar.on_segment_click = [&](size_t index) {
        VERIFY(index < treemapwidget.path_size());
        treemapwidget.set_viewpoint(index);
    };
    treemapwidget.on_path_change = [&]() {
        StringBuilder builder;

        breadcrumbbar.clear_segments();
        for (size_t k = 0; k < treemapwidget.path_size(); k++) {
            if (k == 0) {
                if (treemapwidget.viewpoint() == 0)
                    window->set_title("/ - SpaceAnalyzer");

                breadcrumbbar.append_segment("/", GUI::FileIconProvider::icon_for_path("/").bitmap_for_size(16), "/", "/");
                continue;
            }

            const TreeNode* node = treemapwidget.path_node(k);

            builder.append('/');
            builder.append(node->name());

            // Sneakily set the window title here, while the StringBuilder holds the right amount of the path.
            if (k == treemapwidget.viewpoint())
                window->set_title(DeprecatedString::formatted("{} - SpaceAnalyzer", builder.string_view()));

            breadcrumbbar.append_segment(node->name(), GUI::FileIconProvider::icon_for_path(builder.string_view()).bitmap_for_size(16), builder.string_view(), builder.string_view());
        }
        breadcrumbbar.set_selected_segment(treemapwidget.viewpoint());
    };
    treemapwidget.on_context_menu_request = [&](const GUI::ContextMenuEvent& event) {
        DeprecatedString selected_node_path = get_absolute_path_to_selected_node(treemapwidget);
        if (selected_node_path.is_empty())
            return;
        delete_action->set_enabled(Core::File::can_delete_or_move(selected_node_path));
        if (Core::File::is_directory(selected_node_path)) {
            open_folder_action->set_visible(true);
            open_containing_folder_action->set_visible(false);
        } else {
            open_folder_action->set_visible(false);
            open_containing_folder_action->set_visible(true);
        }
        context_menu->popup(event.screen_position());
    };

    // At startup automatically do an analysis of root.
    TRY(treemapwidget.analyze(statusbar));

    window->show();
    return app->exec();
}
