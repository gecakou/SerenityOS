#include "IRCAppWindow.h"
#include "IRCWindow.h"
#include "IRCWindowListModel.h"
#include <LibGUI/GApplication.h>
#include <LibGUI/GStackWidget.h>
#include <LibGUI/GTableView.h>
#include <LibGUI/GBoxLayout.h>
#include <LibGUI/GToolBar.h>
#include <LibGUI/GAction.h>
#include <LibGUI/GMenu.h>
#include <LibGUI/GMenuBar.h>
#include <LibGUI/GMessageBox.h>
#include <stdio.h>

IRCAppWindow::IRCAppWindow()
    : m_client("127.0.0.1", 6667)
{
    set_title(String::format("IRC Client: %s@%s:%d", m_client.nickname().characters(), m_client.hostname().characters(), m_client.port()));
    set_rect(200, 200, 600, 400);
    setup_actions();
    setup_menus();
    setup_widgets();

    setup_client();
}

IRCAppWindow::~IRCAppWindow()
{
}

void IRCAppWindow::setup_client()
{
    m_client.aid_create_window = [this] (void* owner, IRCWindow::Type type, const String& name) {
        return &create_window(owner, type, name);
    };
    m_client.aid_get_active_window = [this] {
        return static_cast<IRCWindow*>(m_container->active_widget());
    };
    m_client.aid_update_window_list = [this] {
        m_window_list->model()->update();
    };

    m_client.on_connect = [this] {
        m_client.join_channel("#test");
    };

    m_client.connect();
}

void IRCAppWindow::setup_actions()
{
    m_join_action = GAction::create("Join channel", GraphicsBitmap::load_from_file(GraphicsBitmap::Format::RGBA32, "/res/icons/16x16/irc-join.rgb", { 16, 16 }), [] (auto&) {
        printf("FIXME: Implement join action\n");
    });

    m_part_action = GAction::create("Part from channel", GraphicsBitmap::load_from_file(GraphicsBitmap::Format::RGBA32, "/res/icons/16x16/irc-part.rgb", { 16, 16 }), [] (auto&) {
        printf("FIXME: Implement part action\n");
    });

    m_whois_action = GAction::create("Whois user", GraphicsBitmap::load_from_file(GraphicsBitmap::Format::RGBA32, "/res/icons/16x16/irc-whois.rgb", { 16, 16 }), [] (auto&) {
        printf("FIXME: Implement whois action\n");
        GMessageBox box("Who would you like to WHOIS?", "Whois user");
        int code = box.exec();
        dbgprintf("GMessageBox::exec() returned %d\n", code);
    });

    m_open_query_action = GAction::create("Open query", GraphicsBitmap::load_from_file(GraphicsBitmap::Format::RGBA32, "/res/icons/16x16/irc-open-query.rgb", { 16, 16 }), [] (auto&) {
        printf("FIXME: Implement open-query action\n");
    });

    m_close_query_action = GAction::create("Close query", GraphicsBitmap::load_from_file(GraphicsBitmap::Format::RGBA32, "/res/icons/16x16/irc-close-query.rgb", { 16, 16 }), [] (auto&) {
        printf("FIXME: Implement close-query action\n");
    });
}

void IRCAppWindow::setup_menus()
{
    auto menubar = make<GMenuBar>();
    auto app_menu = make<GMenu>("IRC Client");
    app_menu->add_action(GAction::create("Quit", { Mod_Alt, Key_F4 }, [] (const GAction&) {
        dbgprintf("Terminal: Quit menu activated!\n");
        GApplication::the().quit(0);
        return;
    }));
    menubar->add_menu(move(app_menu));

    auto server_menu = make<GMenu>("Server");
    server_menu->add_action(*m_join_action);
    server_menu->add_action(*m_part_action);
    server_menu->add_separator();
    server_menu->add_action(*m_whois_action);
    server_menu->add_action(*m_open_query_action);
    server_menu->add_action(*m_close_query_action);
    menubar->add_menu(move(server_menu));

    auto help_menu = make<GMenu>("Help");
    help_menu->add_action(GAction::create("About", [] (const GAction&) {
        dbgprintf("FIXME: Implement Help/About\n");
    }));
    menubar->add_menu(move(help_menu));

    GApplication::the().set_menubar(move(menubar));
}

void IRCAppWindow::setup_widgets()
{
    auto* widget = new GWidget(nullptr);
    set_main_widget(widget);
    widget->set_layout(make<GBoxLayout>(Orientation::Vertical));

    auto* toolbar = new GToolBar(widget);
    toolbar->add_action(*m_join_action);
    toolbar->add_action(*m_part_action.copy_ref());
    toolbar->add_separator();
    toolbar->add_action(*m_whois_action);
    toolbar->add_action(*m_open_query_action);
    toolbar->add_action(*m_close_query_action);

    auto* horizontal_container = new GWidget(widget);
    horizontal_container->set_layout(make<GBoxLayout>(Orientation::Horizontal));

    m_window_list = new GTableView(horizontal_container);
    m_window_list->set_headers_visible(false);
    m_window_list->set_alternating_row_colors(false);
    m_window_list->set_model(OwnPtr<IRCWindowListModel>(m_client.client_window_list_model()));
    m_window_list->set_size_policy(SizePolicy::Fixed, SizePolicy::Fill);
    m_window_list->set_preferred_size({ 100, 0 });
    m_client.client_window_list_model()->on_activation = [this] (IRCWindow& window) {
        m_container->set_active_widget(&window);
        window.clear_unread_count();
    };

    m_container = new GStackWidget(horizontal_container);

    create_window(&m_client, IRCWindow::Server, "Server");
}

IRCWindow& IRCAppWindow::create_window(void* owner, IRCWindow::Type type, const String& name)
{
    return *new IRCWindow(m_client, owner, type, name, m_container);
}
