#include "WSWindow.h"
#include "WSWindowManager.h"
#include "WSMessage.h"
#include "WSMessageLoop.h"
#include "Process.h"
#include <WindowServer/WSClientConnection.h>

WSWindow::WSWindow(WSMenu& menu)
    : m_lock("WSWindow (menu)")
    , m_type(WSWindowType::Menu)
    , m_menu(&menu)
{
    WSWindowManager::the().add_window(*this);
}

WSWindow::WSWindow(int client_id, int window_id)
    : m_lock("WSWindow (normal)")
    , m_client_id(client_id)
    , m_type(WSWindowType::Normal)
    , m_window_id(window_id)
{
    WSWindowManager::the().add_window(*this);
}

WSWindow::~WSWindow()
{
    WSWindowManager::the().remove_window(*this);
}

void WSWindow::set_title(String&& title)
{
    {
        WSWindowLocker locker(*this);
        if (m_title == title)
            return;
        m_title = move(title);
    }

    WSWindowManager::the().notify_title_changed(*this);
}

void WSWindow::set_rect(const Rect& rect)
{
    Rect old_rect;
    {
        WSWindowLocker locker(*this);

        Process* process = nullptr;
        auto* client = WSClientConnection::from_client_id(m_client_id);
        if (client)
            process = client->process();

        if (!process && !m_menu)
            return;
        if (m_rect == rect)
            return;
        old_rect = m_rect;
        m_rect = rect;
        if (!m_backing || old_rect.size() != rect.size()) {
            if (process)
                m_backing = GraphicsBitmap::create(*process, m_rect.size());
            if (m_menu)
                m_backing = GraphicsBitmap::create_kernel_only(m_rect.size());
        }
    }
    WSWindowManager::the().notify_rect_changed(*this, old_rect, rect);
}

// FIXME: Just use the same types.
static GUI_MouseButton to_api(MouseButton button)
{
    switch (button) {
    case MouseButton::None: return GUI_MouseButton::NoButton;
    case MouseButton::Left: return GUI_MouseButton::Left;
    case MouseButton::Right: return GUI_MouseButton::Right;
    case MouseButton::Middle: return GUI_MouseButton::Middle;
    }
    ASSERT_NOT_REACHED();
}

void WSWindow::on_message(WSMessage& message)
{
    if (m_menu) {
        m_menu->on_window_message(message);
        return;
    }

    GUI_ServerMessage gui_event;
    gui_event.window_id = window_id();

    switch (message.type()) {
    case WSMessage::MouseMove:
        gui_event.type = GUI_ServerMessage::Type::MouseMove;
        gui_event.mouse.position = static_cast<WSMouseEvent&>(message).position();
        gui_event.mouse.button = GUI_MouseButton::NoButton;
        gui_event.mouse.buttons = static_cast<WSMouseEvent&>(message).buttons();
        break;
    case WSMessage::MouseDown:
        gui_event.type = GUI_ServerMessage::Type::MouseDown;
        gui_event.mouse.position = static_cast<WSMouseEvent&>(message).position();
        gui_event.mouse.button = to_api(static_cast<WSMouseEvent&>(message).button());
        gui_event.mouse.buttons = static_cast<WSMouseEvent&>(message).buttons();
        break;
    case WSMessage::MouseUp:
        gui_event.type = GUI_ServerMessage::Type::MouseUp;
        gui_event.mouse.position = static_cast<WSMouseEvent&>(message).position();
        gui_event.mouse.button = to_api(static_cast<WSMouseEvent&>(message).button());
        gui_event.mouse.buttons = static_cast<WSMouseEvent&>(message).buttons();
        break;
    case WSMessage::KeyDown:
        gui_event.type = GUI_ServerMessage::Type::KeyDown;
        gui_event.key.character = static_cast<WSKeyEvent&>(message).character();
        gui_event.key.key = static_cast<WSKeyEvent&>(message).key();
        gui_event.key.alt = static_cast<WSKeyEvent&>(message).alt();
        gui_event.key.ctrl = static_cast<WSKeyEvent&>(message).ctrl();
        gui_event.key.shift = static_cast<WSKeyEvent&>(message).shift();
        break;
    case WSMessage::KeyUp:
        gui_event.type = GUI_ServerMessage::Type::KeyUp;
        gui_event.key.character = static_cast<WSKeyEvent&>(message).character();
        gui_event.key.key = static_cast<WSKeyEvent&>(message).key();
        gui_event.key.alt = static_cast<WSKeyEvent&>(message).alt();
        gui_event.key.ctrl = static_cast<WSKeyEvent&>(message).ctrl();
        gui_event.key.shift = static_cast<WSKeyEvent&>(message).shift();
        break;
    case WSMessage::WindowActivated:
        gui_event.type = GUI_ServerMessage::Type::WindowActivated;
        break;
    case WSMessage::WindowDeactivated:
        gui_event.type = GUI_ServerMessage::Type::WindowDeactivated;
        break;
    case WSMessage::WindowCloseRequest:
        gui_event.type = GUI_ServerMessage::Type::WindowCloseRequest;
        break;
    default:
        break;
    }

    if (gui_event.type == GUI_ServerMessage::Type::Invalid)
        return;

    {
        WSWindowLocker window_locker(*this);
        if (auto* client = WSClientConnection::from_client_id(m_client_id)) {
            if (auto* process = client->process()) {
                LOCKER(process->gui_events_lock());
                process->gui_events().append(move(gui_event));
            }
        }
    }
}

void WSWindow::set_global_cursor_tracking_enabled(bool enabled)
{
    dbgprintf("WSWindow{%p} global_cursor_tracking <- %u\n", enabled);
    m_global_cursor_tracking_enabled = enabled;
}

void WSWindow::set_visible(bool b)
{
    if (m_visible == b)
        return;
    m_visible = b;
    invalidate();
}

void WSWindow::invalidate()
{
    WSWindowManager::the().invalidate(*this);
}
