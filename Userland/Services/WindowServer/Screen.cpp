/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Screen.h"
#include "Compositor.h"
#include "Event.h"
#include "EventLoop.h"
#include "WindowManager.h"
#include <AK/Debug.h>
#include <Kernel/API/FB.h>
#include <Kernel/API/MousePacket.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

namespace WindowServer {

static Screen* s_the;

Screen& Screen::the()
{
    VERIFY(s_the);
    return *s_the;
}

Screen::Screen(unsigned desired_width, unsigned desired_height, int scale_factor)
{
    VERIFY(!s_the);
    s_the = this;
    m_framebuffer_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (m_framebuffer_fd < 0) {
        perror("failed to open /dev/fb0");
        VERIFY_NOT_REACHED();
    }

    if (fb_set_buffer(m_framebuffer_fd, 0) == 0) {
        m_can_set_buffer = true;
    }

    set_resolution(desired_width, desired_height, scale_factor);
    m_physical_cursor_location = physical_rect().center();
}

Screen::~Screen()
{
    close(m_framebuffer_fd);
}

bool Screen::set_resolution(int width, int height, int new_scale_factor)
{
    int new_physical_width = width * new_scale_factor;
    int new_physical_height = height * new_scale_factor;
    if (physical_width() == new_physical_width && physical_height() == new_physical_height) {
        VERIFY(scale_factor() != new_scale_factor);
        on_change_resolution(m_pitch, physical_width(), physical_height(), new_scale_factor);
        return true;
    }

    FBResolution physical_resolution { 0, (unsigned)new_physical_width, (unsigned)new_physical_height };
    int rc = fb_set_resolution(m_framebuffer_fd, &physical_resolution);
    dbgln_if(WSSCREEN_DEBUG, "fb_set_resolution() - return code {}", rc);

    if (rc == 0) {
        on_change_resolution(physical_resolution.pitch, physical_resolution.width, physical_resolution.height, new_scale_factor);
        return true;
    }
    if (rc == -1) {
        dbgln("Invalid resolution {}x{}", width, height);
        on_change_resolution(physical_resolution.pitch, physical_resolution.width, physical_resolution.height, new_scale_factor);
        return false;
    }
    VERIFY_NOT_REACHED();
}

void Screen::on_change_resolution(int pitch, int new_physical_width, int new_physical_height, int new_scale_factor)
{
    if (physical_width() != new_physical_width || physical_height() != new_physical_height) {
        if (m_framebuffer) {
            size_t previous_size_in_bytes = m_size_in_bytes;
            int rc = munmap(m_framebuffer, previous_size_in_bytes);
            VERIFY(rc == 0);
        }

        int rc = fb_get_size_in_bytes(m_framebuffer_fd, &m_size_in_bytes);
        VERIFY(rc == 0);

        m_framebuffer = (Gfx::RGBA32*)mmap(nullptr, m_size_in_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_framebuffer_fd, 0);
        VERIFY(m_framebuffer && m_framebuffer != (void*)-1);
    }

    m_pitch = pitch;
    m_width = new_physical_width / new_scale_factor;
    m_height = new_physical_height / new_scale_factor;
    m_scale_factor = new_scale_factor;

    m_physical_cursor_location.constrain(physical_rect());
}

void Screen::set_buffer(int index)
{
    VERIFY(m_can_set_buffer);
    int rc = fb_set_buffer(m_framebuffer_fd, index);
    VERIFY(rc == 0);
}

void Screen::set_acceleration_factor(double factor)
{
    VERIFY(factor >= mouse_accel_min && factor <= mouse_accel_max);
    m_acceleration_factor = factor;
}

void Screen::set_scroll_step_size(unsigned step_size)
{
    VERIFY(step_size >= scroll_step_size_min);
    m_scroll_step_size = step_size;
}

void Screen::on_receive_mouse_data(const MousePacket& packet)
{
    auto prev_location = m_physical_cursor_location / m_scale_factor;
    if (packet.is_relative) {
        m_physical_cursor_location.move_by(packet.x * m_acceleration_factor, packet.y * m_acceleration_factor);
#if WSSCREEN_DEBUG
        dbgln("Screen: New Relative mouse point @ {}", m_physical_cursor_location);
#endif
    } else {
        m_physical_cursor_location = { packet.x * physical_width() / 0xffff, packet.y * physical_height() / 0xffff };
#if WSSCREEN_DEBUG
        dbgln("Screen: New Absolute mouse point @ {}", m_physical_cursor_location);
#endif
    }

    m_physical_cursor_location.constrain(physical_rect());
    auto new_location = m_physical_cursor_location / m_scale_factor;

    unsigned buttons = packet.buttons;
    unsigned prev_buttons = m_mouse_button_state;
    m_mouse_button_state = buttons;
    unsigned changed_buttons = prev_buttons ^ buttons;
    auto post_mousedown_or_mouseup_if_needed = [&](MouseButton button) {
        if (!(changed_buttons & (unsigned)button))
            return;
        auto message = make<MouseEvent>(buttons & (unsigned)button ? Event::MouseDown : Event::MouseUp, new_location, buttons, button, m_modifiers);
        Core::EventLoop::current().post_event(WindowManager::the(), move(message));
    };
    post_mousedown_or_mouseup_if_needed(MouseButton::Left);
    post_mousedown_or_mouseup_if_needed(MouseButton::Right);
    post_mousedown_or_mouseup_if_needed(MouseButton::Middle);
    post_mousedown_or_mouseup_if_needed(MouseButton::Back);
    post_mousedown_or_mouseup_if_needed(MouseButton::Forward);
    if (new_location != prev_location) {
        auto message = make<MouseEvent>(Event::MouseMove, new_location, buttons, MouseButton::None, m_modifiers);
        if (WindowManager::the().dnd_client())
            message->set_mime_data(WindowManager::the().dnd_mime_data());
        Core::EventLoop::current().post_event(WindowManager::the(), move(message));
    }

    if (packet.z) {
        auto message = make<MouseEvent>(Event::MouseWheel, new_location, buttons, MouseButton::None, m_modifiers, packet.z * m_scroll_step_size);
        Core::EventLoop::current().post_event(WindowManager::the(), move(message));
    }

    if (new_location != prev_location)
        Compositor::the().invalidate_cursor();
}

void Screen::on_receive_keyboard_data(::KeyEvent kernel_event)
{
    m_modifiers = kernel_event.modifiers();
    auto message = make<KeyEvent>(kernel_event.is_press() ? Event::KeyDown : Event::KeyUp, kernel_event.key, kernel_event.code_point, kernel_event.modifiers(), kernel_event.scancode);
    Core::EventLoop::current().post_event(WindowManager::the(), move(message));
}

}
