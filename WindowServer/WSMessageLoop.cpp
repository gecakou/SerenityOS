#include "WSMessageLoop.h"
#include "WSMessage.h"
#include "WSMessageReceiver.h"
#include "WSWindowManager.h"
#include "WSScreen.h"
#include "PS2MouseDevice.h"
#include <Kernel/Keyboard.h>
#include <AK/Bitmap.h>
#include "Process.h"

//#define WSEVENTLOOP_DEBUG

static WSMessageLoop* s_the;

WSMessageLoop::WSMessageLoop()
    : m_lock("WSMessageLoop")
{
    if (!s_the)
        s_the = this;
}

WSMessageLoop::~WSMessageLoop()
{
}

WSMessageLoop& WSMessageLoop::the()
{
    ASSERT(s_the);
    return *s_the;
}

int WSMessageLoop::exec()
{
    ASSERT(m_server_process == current);

    m_keyboard_fd = m_server_process->sys$open("/dev/keyboard", O_RDONLY);
    m_mouse_fd = m_server_process->sys$open("/dev/psaux", O_RDONLY);

    ASSERT(m_keyboard_fd >= 0);
    ASSERT(m_mouse_fd >= 0);

    m_running = true;
    for (;;) {
        wait_for_message();

        Vector<QueuedMessage> messages;
        {
            ASSERT_INTERRUPTS_ENABLED();
            LOCKER(m_lock);
            messages = move(m_queued_messages);
        }

        for (auto& queued_message : messages) {
            auto* receiver = queued_message.receiver;
            auto& message = *queued_message.message;
#ifdef WSEVENTLOOP_DEBUG
            dbgprintf("WSMessageLoop: receiver{%p} message %u (%s)\n", receiver, (unsigned)event.type(), event.name());
#endif
            if (!receiver) {
                dbgprintf("WSMessage type %u with no receiver :(\n", message.type());
                ASSERT_NOT_REACHED();
                return 1;
            } else {
                receiver->on_message(message);
            }
        }
    }
}

void WSMessageLoop::post_message(WSMessageReceiver* receiver, OwnPtr<WSMessage>&& message, bool unsafe)
{
    if (unsafe) {
        // FIXME: This is such a hack. It should not exist.
        m_queued_messages.append({ receiver, move(message) });
        if (current != m_server_process)
            m_server_process->request_wakeup();
        return;
    }
    LOCKER(m_lock);
#ifdef WSEVENTLOOP_DEBUG
    dbgprintf("WSMessageLoop::post_message: {%u} << receiver=%p, message=%p\n", m_queued_messages.size(), receiver, message.ptr());
#endif

    if (message->type() == WSMessage::WM_ClientFinishedPaint) {
        auto& invalidation_message = static_cast<WSClientFinishedPaintMessage&>(*message);
        for (auto& queued_message : m_queued_messages) {
            if (receiver == queued_message.receiver && queued_message.message->type() == WSMessage::WM_ClientFinishedPaint) {
                auto& queued_invalidation_message = static_cast<WSClientFinishedPaintMessage&>(*queued_message.message);
                if (queued_invalidation_message.rect().is_empty() || queued_invalidation_message.rect().contains(invalidation_message.rect())) {
#ifdef WSEVENTLOOP_DEBUG
                    dbgprintf("Swallow WM_ClientFinishedPaint\n");
#endif
                    return;
                }
            }
        }
    }

    if (message->type() == WSMessage::WM_ClientWantsToPaint) {
        auto& invalidation_message = static_cast<WSClientWantsToPaintMessage&>(*message);
        for (auto& queued_message : m_queued_messages) {
            if (receiver == queued_message.receiver && queued_message.message->type() == WSMessage::WM_ClientWantsToPaint) {
                auto& queued_invalidation_message = static_cast<WSClientWantsToPaintMessage&>(*queued_message.message);
                if (queued_invalidation_message.rect().is_empty() || queued_invalidation_message.rect().contains(invalidation_message.rect())) {
#ifdef WSEVENTLOOP_DEBUG
                    dbgprintf("Swallow WM_ClientWantsToPaint\n");
#endif
                    return;
                }
            }
        }
    }

    m_queued_messages.append({ receiver, move(message) });

    if (current != m_server_process)
        m_server_process->request_wakeup();
}

void WSMessageLoop::wait_for_message()
{
    fd_set rfds;
    memset(&rfds, 0, sizeof(rfds));
    auto bitmap = Bitmap::wrap((byte*)&rfds, FD_SETSIZE);
    bitmap.set(m_keyboard_fd, true);
    bitmap.set(m_mouse_fd, true);
    Syscall::SC_select_params params;
    params.nfds = max(m_keyboard_fd, m_mouse_fd) + 1;
    params.readfds = &rfds;
    params.writefds = nullptr;
    params.exceptfds = nullptr;
    struct timeval timeout = { 0, 0 };
    if (m_queued_messages.is_empty())
        params.timeout = nullptr;
    else
        params.timeout = &timeout;
    int rc = m_server_process->sys$select(&params);
    memory_barrier();
    if (rc < 0) {
        ASSERT_NOT_REACHED();
    }

    if (bitmap.get(m_keyboard_fd))
        drain_keyboard();
    if (bitmap.get(m_mouse_fd))
        drain_mouse();
}

void WSMessageLoop::drain_mouse()
{
    auto& screen = WSScreen::the();
    auto& mouse = PS2MouseDevice::the();
    bool prev_left_button = screen.left_mouse_button_pressed();
    bool prev_right_button = screen.right_mouse_button_pressed();
    int dx = 0;
    int dy = 0;
    while (mouse.can_read(*m_server_process)) {
        byte data[3];
        ssize_t nread = mouse.read(*m_server_process, (byte*)data, sizeof(data));
        ASSERT(nread == sizeof(data));
        bool left_button = data[0] & 1;
        bool right_button = data[0] & 2;
        bool x_overflow = data[0] & 0x40;
        bool y_overflow = data[0] & 0x80;
        bool x_sign = data[0] & 0x10;
        bool y_sign = data[0] & 0x20;

        if (x_overflow || y_overflow)
            continue;

        int x = data[1];
        int y = data[2];
        if (x && x_sign)
            x -= 0x100;
        if (y && y_sign)
            y -= 0x100;

        dx += x;
        dy += -y;
        if (left_button != prev_left_button || right_button != prev_right_button || !mouse.can_read(*m_server_process)) {
            prev_left_button = left_button;
            prev_right_button = right_button;
            screen.on_receive_mouse_data(dx, dy, left_button, right_button);
            dx = 0;
            dy = 0;
        }
    }
}

void WSMessageLoop::drain_keyboard()
{
    auto& screen = WSScreen::the();
    auto& keyboard = Keyboard::the();
    while (keyboard.can_read(*m_server_process)) {
        Keyboard::Event event;
        ssize_t nread = keyboard.read(*m_server_process, (byte*)&event, sizeof(Keyboard::Event));
        ASSERT(nread == sizeof(Keyboard::Event));
        screen.on_receive_keyboard_data(event);
    }
}
