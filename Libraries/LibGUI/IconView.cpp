/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/StringBuilder.h>
#include <LibCore/Timer.h>
#include <LibGUI/DragOperation.h>
#include <LibGUI/IconView.h>
#include <LibGUI/Model.h>
#include <LibGUI/Painter.h>
#include <LibGUI/ScrollBar.h>
#include <LibGfx/Palette.h>

//#define DRAGDROP_DEBUG

namespace GUI {

IconView::IconView()
{
    set_fill_with_background_color(true);
    set_background_role(ColorRole::Base);
    set_foreground_role(ColorRole::BaseText);
    horizontal_scrollbar().set_visible(false);
}

IconView::~IconView()
{
}

void IconView::select_all()
{
    selection().clear();
    for (int item_index = 0; item_index < item_count(); ++item_index) {
        auto index = model()->index(item_index, model_column());
        selection().add(index);
    }
}

void IconView::scroll_into_view(const ModelIndex& index, Orientation orientation)
{
    ScrollableWidget::scroll_into_view(item_rect(index.row()), orientation);
}

void IconView::resize_event(ResizeEvent& event)
{
    AbstractView::resize_event(event);
    update_content_size();
}

void IconView::did_update_model(unsigned flags)
{
    AbstractView::did_update_model(flags);
    update_content_size();
    update();
}

void IconView::update_content_size()
{
    if (!model())
        return set_content_size({});

    m_visual_column_count = available_size().width() / effective_item_size().width();
    if (m_visual_column_count)
        m_visual_row_count = ceil_div(model()->row_count(), m_visual_column_count);
    else
        m_visual_row_count = 0;

    int content_width = available_size().width();
    int content_height = m_visual_row_count * effective_item_size().height();

    set_content_size({ content_width, content_height });
}

Gfx::IntRect IconView::item_rect(int item_index) const
{
    if (!m_visual_row_count || !m_visual_column_count)
        return {};
    int visual_row_index = item_index / m_visual_column_count;
    int visual_column_index = item_index % m_visual_column_count;
    return {
        visual_column_index * effective_item_size().width(),
        visual_row_index * effective_item_size().height(),
        effective_item_size().width(),
        effective_item_size().height()
    };
}

Vector<int> IconView::items_intersecting_rect(const Gfx::IntRect& rect) const
{
    ASSERT(model());
    Vector<int> item_indexes;
    for (int item_index = 0; item_index < item_count(); ++item_index) {
        Gfx::IntRect item_rect;
        Gfx::IntRect icon_rect;
        Gfx::IntRect text_rect;
        auto index = model()->index(item_index, model_column());
        auto item_text = model()->data(index);
        get_item_rects(item_index, font_for_index(index), item_text, item_rect, icon_rect, text_rect);
        if (icon_rect.intersects(rect) || text_rect.intersects(rect))
            item_indexes.append(item_index);
    }
    return item_indexes;
}

ModelIndex IconView::index_at_event_position(const Gfx::IntPoint& position) const
{
    ASSERT(model());
    // FIXME: Since all items are the same size, just compute the clicked item index
    //        instead of iterating over everything.
    auto adjusted_position = to_content_position(position);
    for (int item_index = 0; item_index < item_count(); ++item_index) {
        Gfx::IntRect item_rect;
        Gfx::IntRect icon_rect;
        Gfx::IntRect text_rect;
        auto index = model()->index(item_index, model_column());
        auto item_text = model()->data(index);
        get_item_rects(item_index, font_for_index(index), item_text, item_rect, icon_rect, text_rect);
        if (icon_rect.contains(adjusted_position) || text_rect.contains(adjusted_position))
            return index;
    }
    return {};
}

void IconView::mousedown_event(MouseEvent& event)
{
    if (!model())
        return AbstractView::mousedown_event(event);

    if (event.button() != MouseButton::Left)
        return AbstractView::mousedown_event(event);

    auto index = index_at_event_position(event.position());
    if (index.is_valid()) {
        // We might start dragging this item, but not rubber-banding.
        return AbstractView::mousedown_event(event);
    }

    ASSERT(m_rubber_band_remembered_selection.is_empty());

    if (event.modifiers() & Mod_Ctrl) {
        selection().for_each_index([&](auto& index) {
            m_rubber_band_remembered_selection.append(index);
        });
    } else {
        selection().clear();
    }

    auto adjusted_position = to_content_position(event.position());

    m_might_drag = false;
    m_rubber_banding = true;
    m_rubber_band_origin = adjusted_position;
    m_rubber_band_current = adjusted_position;
}

void IconView::mouseup_event(MouseEvent& event)
{
    if (m_rubber_banding && event.button() == MouseButton::Left) {
        m_rubber_banding = false;
        m_rubber_band_remembered_selection.clear();
        if (m_out_of_view_timer)
            m_out_of_view_timer->stop();
        update();
    }
    AbstractView::mouseup_event(event);
}

void IconView::drag_move_event(DragEvent& event)
{
    auto index = index_at_event_position(event.position());
    ModelIndex new_drop_candidate_index;
    if (index.is_valid()) {
        bool acceptable = model()->accepts_drag(index, event.data_type());
#ifdef DRAGDROP_DEBUG
        dbg() << "Drag of type '" << event.data_type() << "' moving over " << index << ", acceptable: " << acceptable;
#endif
        if (acceptable)
            new_drop_candidate_index = index;
    }
    if (m_drop_candidate_index != new_drop_candidate_index) {
        m_drop_candidate_index = new_drop_candidate_index;
        update();
    }
    event.accept();
}

bool IconView::update_rubber_banding(const Gfx::IntPoint& position)
{
    auto adjusted_position = to_content_position(position);
    if (m_rubber_band_current != adjusted_position) {
        m_rubber_band_current = adjusted_position;
        auto rubber_band_rect = Gfx::IntRect::from_two_points(m_rubber_band_origin, m_rubber_band_current);
        selection().clear();
        for (auto item_index : items_intersecting_rect(rubber_band_rect)) {
            selection().add(model()->index(item_index, model_column()));
        }
        if (m_rubber_banding_store_selection) {
            for (auto stored_item : m_rubber_band_remembered_selection) {
                selection().add(stored_item);
            }
        }
        update();
        return true;
    }
    return false;
}

#define SCROLL_OUT_OF_VIEW_HOT_MARGIN 20

void IconView::mousemove_event(MouseEvent& event)
{
    if (!model())
        return AbstractView::mousemove_event(event);

    if (m_rubber_banding) {
        m_rubber_banding_store_selection = (event.modifiers() & Mod_Ctrl);

        auto in_view_rect = widget_inner_rect();
        in_view_rect.shrink(SCROLL_OUT_OF_VIEW_HOT_MARGIN, SCROLL_OUT_OF_VIEW_HOT_MARGIN);
        if (!in_view_rect.contains(event.position())) {
            if (!m_out_of_view_timer) {
                m_out_of_view_timer = add<Core::Timer>();
                m_out_of_view_timer->set_interval(100);
                m_out_of_view_timer->on_timeout = [this] {
                    scroll_out_of_view_timer_fired();
                };
            }
            
            m_out_of_view_position = event.position();
            if (!m_out_of_view_timer->is_active())
                m_out_of_view_timer->start();
        } else {
            if (m_out_of_view_timer)
                m_out_of_view_timer->stop();
        }
        if (update_rubber_banding(event.position()))
            return;
    }

    AbstractView::mousemove_event(event);
}

void IconView::scroll_out_of_view_timer_fired()
{
    auto scroll_to = to_content_position(m_out_of_view_position);
    // Adjust the scroll-to position by SCROLL_OUT_OF_VIEW_HOT_MARGIN / 2
    // depending on which direction we're scrolling. This allows us to
    // start scrolling before we actually leave the visible area, which
    // is important when there is no space to further move the mouse. The
    // speed of scrolling is determined by the distance between the mouse
    // pointer and the widget's inner rect shrunken by the hot margin
    auto in_view_rect = widget_inner_rect().shrunken(SCROLL_OUT_OF_VIEW_HOT_MARGIN, SCROLL_OUT_OF_VIEW_HOT_MARGIN);
    int adjust_x = 0, adjust_y = 0;
    if (m_out_of_view_position.y() > in_view_rect.bottom())
        adjust_y = (SCROLL_OUT_OF_VIEW_HOT_MARGIN / 2) + min(SCROLL_OUT_OF_VIEW_HOT_MARGIN, m_out_of_view_position.y() - in_view_rect.bottom());
    else if (m_out_of_view_position.y() < in_view_rect.top())
        adjust_y = -(SCROLL_OUT_OF_VIEW_HOT_MARGIN / 2) + max(-SCROLL_OUT_OF_VIEW_HOT_MARGIN, m_out_of_view_position.y() - in_view_rect.top());
    if (m_out_of_view_position.x() > in_view_rect.right())
        adjust_x = (SCROLL_OUT_OF_VIEW_HOT_MARGIN / 2) + min(SCROLL_OUT_OF_VIEW_HOT_MARGIN, m_out_of_view_position.x() - in_view_rect.right());
    else if (m_out_of_view_position.x() < in_view_rect.left())
        adjust_x = -(SCROLL_OUT_OF_VIEW_HOT_MARGIN / 2) + max(-SCROLL_OUT_OF_VIEW_HOT_MARGIN, m_out_of_view_position.x() - in_view_rect.left());
    
    ScrollableWidget::scroll_into_view({scroll_to.translated(adjust_x, adjust_y), {1, 1}}, true, true);
    update_rubber_banding(m_out_of_view_position);
}

void IconView::get_item_rects(int item_index, const Gfx::Font& font, const Variant& item_text, Gfx::IntRect& item_rect, Gfx::IntRect& icon_rect, Gfx::IntRect& text_rect) const
{
    item_rect = this->item_rect(item_index);
    icon_rect = { 0, 0, 32, 32 };
    icon_rect.center_within(item_rect);
    icon_rect.move_by(0, -font.glyph_height() - 6);
    text_rect = { 0, icon_rect.bottom() + 6 + 1, font.width(item_text.to_string()), font.glyph_height() };
    text_rect.center_horizontally_within(item_rect);
    text_rect.inflate(6, 4);
    text_rect.intersect(item_rect);
}

void IconView::second_paint_event(PaintEvent& event)
{
    if (!m_rubber_banding)
        return;

    Painter painter(*this);
    painter.add_clip_rect(event.rect());
    painter.translate(frame_thickness(), frame_thickness());
    painter.translate(-horizontal_scrollbar().value(), -vertical_scrollbar().value());

    auto rubber_band_rect = Gfx::IntRect::from_two_points(m_rubber_band_origin, m_rubber_band_current);
    painter.fill_rect(rubber_band_rect, palette().rubber_band_fill());
    painter.draw_rect(rubber_band_rect, palette().rubber_band_border());
}

void IconView::paint_event(PaintEvent& event)
{
    Color widget_background_color = palette().color(background_role());
    Frame::paint_event(event);

    Painter painter(*this);
    painter.add_clip_rect(widget_inner_rect());
    painter.add_clip_rect(event.rect());
    if (fill_with_background_color())
        painter.fill_rect(event.rect(), widget_background_color);
    painter.translate(frame_thickness(), frame_thickness());
    painter.translate(-horizontal_scrollbar().value(), -vertical_scrollbar().value());

    for (int item_index = 0; item_index < model()->row_count(); ++item_index) {
        auto model_index = model()->index(item_index, m_model_column);
        bool is_selected_item = selection().contains(model_index);
        Color background_color;
        if (is_selected_item) {
            background_color = is_focused() ? palette().selection() : palette().inactive_selection();
        } else {
            background_color = widget_background_color;
        }

        auto icon = model()->data(model_index, Model::Role::Icon);
        auto item_text = model()->data(model_index, Model::Role::Display);

        Gfx::IntRect item_rect;
        Gfx::IntRect icon_rect;
        Gfx::IntRect text_rect;
        get_item_rects(item_index, font_for_index(model_index), item_text, item_rect, icon_rect, text_rect);

        if (icon.is_icon()) {
            if (auto bitmap = icon.as_icon().bitmap_for_size(icon_rect.width())) {
                Gfx::IntRect destination = bitmap->rect();
                destination.center_within(icon_rect);

                if (m_hovered_index.is_valid() && m_hovered_index == model_index) {
                    painter.blit_brightened(destination.location(), *bitmap, bitmap->rect());
                } else {
                    painter.blit(destination.location(), *bitmap, bitmap->rect());
                }
            }
        }

        Color text_color;
        if (is_selected_item)
            text_color = is_focused() ? palette().selection_text() : palette().inactive_selection_text();
        else
            text_color = model()->data(model_index, Model::Role::ForegroundColor).to_color(palette().color(foreground_role()));
        painter.fill_rect(text_rect, background_color);
        painter.draw_text(text_rect, item_text.to_string(), font_for_index(model_index), Gfx::TextAlignment::Center, text_color, Gfx::TextElision::Right);

        if (model_index == m_drop_candidate_index) {
            // FIXME: This visualization is not great, as it's also possible to drop things on the text label..
            painter.draw_rect(icon_rect.inflated(8, 8), palette().selection(), true);
        }
    };
}

int IconView::item_count() const
{
    if (!model())
        return 0;
    return model()->row_count();
}

void IconView::keydown_event(KeyEvent& event)
{
    if (!model())
        return;
    if (!m_visual_row_count || !m_visual_column_count)
        return;

    auto& model = *this->model();
    if (event.key() == KeyCode::Key_Return) {
        activate_selected();
        return;
    }
    if (event.key() == KeyCode::Key_Home) {
        auto new_index = model.index(0, 0);
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_End) {
        auto new_index = model.index(model.row_count() - 1, 0);
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_Up) {
        ModelIndex new_index;
        if (!selection().is_empty()) {
            auto old_index = selection().first();
            new_index = model.index(old_index.row() - m_visual_column_count, old_index.column());
        } else {
            new_index = model.index(0, 0);
        }
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_Down) {
        ModelIndex new_index;
        if (!selection().is_empty()) {
            auto old_index = selection().first();
            new_index = model.index(old_index.row() + m_visual_column_count, old_index.column());
        } else {
            new_index = model.index(0, 0);
        }
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_Left) {
        ModelIndex new_index;
        if (!selection().is_empty()) {
            auto old_index = selection().first();
            new_index = model.index(old_index.row() - 1, old_index.column());
        } else {
            new_index = model.index(0, 0);
        }
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_Right) {
        ModelIndex new_index;
        if (!selection().is_empty()) {
            auto old_index = selection().first();
            new_index = model.index(old_index.row() + 1, old_index.column());
        } else {
            new_index = model.index(0, 0);
        }
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_PageUp) {
        int items_per_page = (visible_content_rect().height() / effective_item_size().height()) * m_visual_column_count;
        auto old_index = selection().first();
        auto new_index = model.index(max(0, old_index.row() - items_per_page), old_index.column());
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    if (event.key() == KeyCode::Key_PageDown) {
        int items_per_page = (visible_content_rect().height() / effective_item_size().height()) * m_visual_column_count;
        auto old_index = selection().first();
        auto new_index = model.index(min(model.row_count() - 1, old_index.row() + items_per_page), old_index.column());
        if (model.is_valid(new_index)) {
            selection().set(new_index);
            scroll_into_view(new_index, Orientation::Vertical);
            update();
        }
        return;
    }
    return Widget::keydown_event(event);
}

}
