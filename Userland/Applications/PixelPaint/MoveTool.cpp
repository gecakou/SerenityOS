/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "MoveTool.h"
#include "Image.h"
#include "ImageEditor.h"
#include "Layer.h"
#include <LibGUI/Action.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Window.h>
#include <LibGfx/Bitmap.h>

namespace PixelPaint {

MoveTool::MoveTool()
{
}

MoveTool::~MoveTool()
{
}

void MoveTool::on_mousedown(Layer& layer, GUI::MouseEvent& event, GUI::MouseEvent& image_event)
{
    if (event.button() != GUI::MouseButton::Left)
        return;
    if (!layer.rect().contains(event.position()))
        return;
    m_layer_being_moved = layer;
    m_event_origin = image_event.position();
    m_layer_origin = layer.location();
    m_editor->window()->set_cursor(Gfx::StandardCursor::Move);
}

void MoveTool::on_mousemove(Layer&, GUI::MouseEvent&, GUI::MouseEvent& image_event)
{
    if (!m_layer_being_moved)
        return;
    auto delta = image_event.position() - m_event_origin;
    m_layer_being_moved->set_location(m_layer_origin.translated(delta));
    m_editor->layers_did_change();
}

void MoveTool::on_mouseup(Layer&, GUI::MouseEvent& event, GUI::MouseEvent&)
{
    if (event.button() != GUI::MouseButton::Left)
        return;
    m_layer_being_moved = nullptr;
    m_editor->window()->set_cursor(Gfx::StandardCursor::None);
    m_editor->did_complete_action();
}

void MoveTool::on_keydown(GUI::KeyEvent& event)
{
    if (event.modifiers() != 0)
        return;

    auto* layer = m_editor->active_layer();
    if (!layer)
        return;

    auto new_location = layer->location();

    switch (event.key()) {
    case Key_Up:
        new_location.move_by(0, -1);
        break;
    case Key_Down:
        new_location.move_by(0, 1);
        break;
    case Key_Left:
        new_location.move_by(-1, 0);
        break;
    case Key_Right:
        new_location.move_by(1, 0);
        break;
    default:
        return;
    }

    layer->set_location(new_location);
    m_editor->layers_did_change();
}

void MoveTool::on_context_menu(Layer& layer, GUI::ContextMenuEvent& event)
{
    if (!m_context_menu) {
        m_context_menu = GUI::Menu::construct();
        m_context_menu->add_action(GUI::CommonActions::make_move_to_front_action(
            [this](auto&) {
                m_editor->image()->move_layer_to_front(*m_context_menu_layer);
                m_editor->layers_did_change();
            },
            m_editor));
        m_context_menu->add_action(GUI::CommonActions::make_move_to_back_action(
            [this](auto&) {
                m_editor->image()->move_layer_to_back(*m_context_menu_layer);
                m_editor->layers_did_change();
            },
            m_editor));
        m_context_menu->add_separator();
        m_context_menu->add_action(GUI::Action::create(
            "&Delete Layer", Gfx::Bitmap::load_from_file("/res/icons/16x16/delete.png"), [this](auto&) {
                m_editor->image()->remove_layer(*m_context_menu_layer);
                // FIXME: This should not be done imperatively here. Perhaps a Image::Client interface that ImageEditor can implement?
                if (m_editor->active_layer() == m_context_menu_layer)
                    m_editor->set_active_layer(nullptr);
                m_editor->layers_did_change();
            },
            m_editor));
    }
    m_context_menu_layer = layer;
    m_context_menu->popup(event.screen_position());
}

}
