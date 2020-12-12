/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
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

#include <LibWeb/CSS/Length.h>
#include <LibWeb/DOM/Node.h>
#include <LibWeb/Layout/BlockBox.h>
#include <LibWeb/Layout/BlockFormattingContext.h>
#include <LibWeb/Layout/Box.h>
#include <LibWeb/Layout/InitialContainingBlockBox.h>
#include <LibWeb/Layout/InlineFormattingContext.h>
#include <LibWeb/Layout/ListItemBox.h>
#include <LibWeb/Layout/WidgetBox.h>
#include <LibWeb/Page/Frame.h>

namespace Web::Layout {

BlockFormattingContext::BlockFormattingContext(Box& context_box, FormattingContext* parent)
    : FormattingContext(context_box, parent)
{
}

BlockFormattingContext::~BlockFormattingContext()
{
}

bool BlockFormattingContext::is_initial() const
{
    return context_box().is_initial_containing_block();
}

void BlockFormattingContext::run(Box& box, LayoutMode layout_mode)
{
    if (is_initial()) {
        layout_initial_containing_block(layout_mode);
        return;
    }

    // FIXME: BFC currently computes the width+height of the target box.
    //        This is necessary to be able to place absolutely positioned descendants.
    //        The same work is also done by the parent BFC for each of its blocks..

    if (layout_mode == LayoutMode::Default)
        compute_width(box);

    if (box.children_are_inline()) {
        layout_inline_children(box, layout_mode);
    } else {
        layout_block_level_children(box, layout_mode);
    }

    if (layout_mode == LayoutMode::Default)
        compute_height(box);
}

void BlockFormattingContext::compute_width(Box& box)
{
    if (box.is_replaced()) {
        // FIXME: This should not be done *by* ReplacedBox
        auto& replaced = downcast<ReplacedBox>(box);
        replaced.prepare_for_replaced_layout();
        compute_width_for_block_level_replaced_element_in_normal_flow(replaced);
        return;
    }

    if (box.is_absolutely_positioned()) {
        compute_width_for_absolutely_positioned_block(box);
        return;
    }

    if (box.is_floating()) {
        compute_width_for_floating_box(box);
        return;
    }

    auto& style = box.style();
    float width_of_containing_block = box.width_of_logical_containing_block();

    auto zero_value = CSS::Length::make_px(0);

    auto margin_left = CSS::Length::make_auto();
    auto margin_right = CSS::Length::make_auto();
    const auto padding_left = style.padding().left.resolved_or_zero(box, width_of_containing_block);
    const auto padding_right = style.padding().right.resolved_or_zero(box, width_of_containing_block);

    auto try_compute_width = [&](const auto& a_width) {
        CSS::Length width = a_width;
        margin_left = style.margin().left.resolved_or_zero(box, width_of_containing_block);
        margin_right = style.margin().right.resolved_or_zero(box, width_of_containing_block);

        float total_px = style.border_left().width + style.border_right().width;
        for (auto& value : { margin_left, padding_left, width, padding_right, margin_right }) {
            total_px += value.to_px(box);
        }

        if (!box.is_inline()) {
            // 10.3.3 Block-level, non-replaced elements in normal flow
            // If 'width' is not 'auto' and 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' (plus any of 'margin-left' or 'margin-right' that are not 'auto') is larger than the width of the containing block, then any 'auto' values for 'margin-left' or 'margin-right' are, for the following rules, treated as zero.
            if (width.is_auto() && total_px > width_of_containing_block) {
                if (margin_left.is_auto())
                    margin_left = zero_value;
                if (margin_right.is_auto())
                    margin_right = zero_value;
            }

            // 10.3.3 cont'd.
            auto underflow_px = width_of_containing_block - total_px;

            if (width.is_auto()) {
                if (margin_left.is_auto())
                    margin_left = zero_value;
                if (margin_right.is_auto())
                    margin_right = zero_value;
                if (underflow_px >= 0) {
                    width = CSS::Length(underflow_px, CSS::Length::Type::Px);
                } else {
                    width = zero_value;
                    margin_right = CSS::Length(margin_right.to_px(box) + underflow_px, CSS::Length::Type::Px);
                }
            } else {
                if (!margin_left.is_auto() && !margin_right.is_auto()) {
                    margin_right = CSS::Length(margin_right.to_px(box) + underflow_px, CSS::Length::Type::Px);
                } else if (!margin_left.is_auto() && margin_right.is_auto()) {
                    margin_right = CSS::Length(underflow_px, CSS::Length::Type::Px);
                } else if (margin_left.is_auto() && !margin_right.is_auto()) {
                    margin_left = CSS::Length(underflow_px, CSS::Length::Type::Px);
                } else { // margin_left.is_auto() && margin_right.is_auto()
                    auto half_of_the_underflow = CSS::Length(underflow_px / 2, CSS::Length::Type::Px);
                    margin_left = half_of_the_underflow;
                    margin_right = half_of_the_underflow;
                }
            }
        } else if (box.is_inline_block()) {

            // 10.3.9 'Inline-block', non-replaced elements in normal flow

            // A computed value of 'auto' for 'margin-left' or 'margin-right' becomes a used value of '0'.
            if (margin_left.is_auto())
                margin_left = zero_value;
            if (margin_right.is_auto())
                margin_right = zero_value;

            // If 'width' is 'auto', the used value is the shrink-to-fit width as for floating elements.
            if (width.is_auto()) {

                // Find the available width: in this case, this is the width of the containing
                // block minus the used values of 'margin-left', 'border-left-width', 'padding-left',
                // 'padding-right', 'border-right-width', 'margin-right', and the widths of any relevant scroll bars.
                float available_width = width_of_containing_block
                    - margin_left.to_px(box) - style.border_left().width - padding_left.to_px(box)
                    - padding_right.to_px(box) - style.border_right().width - margin_right.to_px(box);

                auto result = calculate_shrink_to_fit_widths(box);

                // Then the shrink-to-fit width is: min(max(preferred minimum width, available width), preferred width).
                width = CSS::Length(min(max(result.preferred_minimum_width, available_width), result.preferred_width), CSS::Length::Type::Px);
            }
        }

        return width;
    };

    auto specified_width = style.width().resolved_or_auto(box, width_of_containing_block);

    // 1. The tentative used width is calculated (without 'min-width' and 'max-width')
    auto used_width = try_compute_width(specified_width);

    // 2. The tentative used width is greater than 'max-width', the rules above are applied again,
    //    but this time using the computed value of 'max-width' as the computed value for 'width'.
    auto specified_max_width = style.max_width().resolved_or_auto(box, width_of_containing_block);
    if (!specified_max_width.is_auto()) {
        if (used_width.to_px(box) > specified_max_width.to_px(box)) {
            used_width = try_compute_width(specified_max_width);
        }
    }

    // 3. If the resulting width is smaller than 'min-width', the rules above are applied again,
    //    but this time using the value of 'min-width' as the computed value for 'width'.
    auto specified_min_width = style.min_width().resolved_or_auto(box, width_of_containing_block);
    if (!specified_min_width.is_auto()) {
        if (used_width.to_px(box) < specified_min_width.to_px(box)) {
            used_width = try_compute_width(specified_min_width);
        }
    }

    box.set_width(used_width.to_px(box));
    box.box_model().margin.left = margin_left;
    box.box_model().margin.right = margin_right;
    box.box_model().border.left = CSS::Length::make_px(style.border_left().width);
    box.box_model().border.right = CSS::Length::make_px(style.border_right().width);
    box.box_model().padding.left = padding_left;
    box.box_model().padding.right = padding_right;
}

void BlockFormattingContext::compute_width_for_floating_box(Box& box)
{
    // 10.3.5 Floating, non-replaced elements
    auto& style = box.style();
    float width_of_containing_block = box.width_of_logical_containing_block();
    auto zero_value = CSS::Length::make_px(0);

    auto margin_left = CSS::Length::make_auto();
    auto margin_right = CSS::Length::make_auto();
    const auto padding_left = style.padding().left.resolved_or_zero(box, width_of_containing_block);
    const auto padding_right = style.padding().right.resolved_or_zero(box, width_of_containing_block);

    // If 'margin-left', or 'margin-right' are computed as 'auto', their used value is '0'.
    if (margin_left.is_auto())
        margin_left = zero_value;
    if (margin_right.is_auto())
        margin_right = zero_value;

    auto width = style.width().resolved_or_auto(box, width_of_containing_block);

    // If 'width' is computed as 'auto', the used value is the "shrink-to-fit" width.
    if (width.is_auto()) {

        // Find the available width: in this case, this is the width of the containing
        // block minus the used values of 'margin-left', 'border-left-width', 'padding-left',
        // 'padding-right', 'border-right-width', 'margin-right', and the widths of any relevant scroll bars.
        float available_width = width_of_containing_block
            - margin_left.to_px(box) - style.border_left().width - padding_left.to_px(box)
            - padding_right.to_px(box) - style.border_right().width - margin_right.to_px(box);

        auto result = calculate_shrink_to_fit_widths(box);

        // Then the shrink-to-fit width is: min(max(preferred minimum width, available width), preferred width).
        width = CSS::Length(min(max(result.preferred_minimum_width, available_width), result.preferred_width), CSS::Length::Type::Px);
    }

    float final_width = width.resolved_or_zero(box, width_of_containing_block).to_px(box);
    box.set_width(final_width);
}

void BlockFormattingContext::compute_width_for_block_level_replaced_element_in_normal_flow(ReplacedBox& box)
{
    box.set_width(compute_width_for_replaced_element(box));
}

void BlockFormattingContext::compute_height_for_block_level_replaced_element_in_normal_flow(ReplacedBox& box)
{
    box.set_height(compute_height_for_replaced_element(box));
}

void BlockFormattingContext::compute_width_for_absolutely_positioned_block(Box& box)
{
    auto& containing_block = context_box();
    auto& style = box.style();
    auto zero_value = CSS::Length::make_px(0);

    auto margin_left = CSS::Length::make_auto();
    auto margin_right = CSS::Length::make_auto();
    const auto border_left = style.border_left().width;
    const auto border_right = style.border_right().width;
    const auto padding_left = style.padding().left.resolved_or_zero(box, containing_block.width());
    const auto padding_right = style.padding().right.resolved_or_zero(box, containing_block.width());

    auto try_compute_width = [&](const auto& a_width) {
        margin_left = style.margin().left.resolved_or_zero(box, containing_block.width());
        margin_right = style.margin().right.resolved_or_zero(box, containing_block.width());

        auto left = style.offset().left.resolved_or_auto(box, containing_block.width());
        auto right = style.offset().right.resolved_or_auto(box, containing_block.width());
        auto width = a_width;

        auto solve_for_left = [&] {
            return CSS::Length(containing_block.width() - margin_left.to_px(box) - border_left - padding_left.to_px(box) - width.to_px(box) - padding_right.to_px(box) - border_right - margin_right.to_px(box) - right.to_px(box), CSS::Length::Type::Px);
        };

        auto solve_for_width = [&] {
            return CSS::Length(containing_block.width() - left.to_px(box) - margin_left.to_px(box) - border_left - padding_left.to_px(box) - padding_right.to_px(box) - border_right - margin_right.to_px(box) - right.to_px(box), CSS::Length::Type::Px);
        };

        auto solve_for_right = [&] {
            return CSS::Length(containing_block.width() - left.to_px(box) - margin_left.to_px(box) - border_left - padding_left.to_px(box) - width.to_px(box) - padding_right.to_px(box) - border_right - margin_right.to_px(box), CSS::Length::Type::Px);
        };

        // If all three of 'left', 'width', and 'right' are 'auto':
        if (left.is_auto() && width.is_auto() && right.is_auto()) {
            // First set any 'auto' values for 'margin-left' and 'margin-right' to 0.
            if (margin_left.is_auto())
                margin_left = CSS::Length::make_px(0);
            if (margin_right.is_auto())
                margin_right = CSS::Length::make_px(0);
            // Then, if the 'direction' property of the element establishing the static-position containing block
            // is 'ltr' set 'left' to the static position and apply rule number three below;
            // otherwise, set 'right' to the static position and apply rule number one below.
            // FIXME: This is very hackish.
            left = CSS::Length::make_px(0);
            goto Rule3;
        }

        if (!left.is_auto() && !width.is_auto() && !right.is_auto()) {
            // FIXME: This should be solved in a more complicated way.
            return width;
        }

        if (margin_left.is_auto())
            margin_left = CSS::Length::make_px(0);
        if (margin_right.is_auto())
            margin_right = CSS::Length::make_px(0);

        // 1. 'left' and 'width' are 'auto' and 'right' is not 'auto',
        //    then the width is shrink-to-fit. Then solve for 'left'
        if (left.is_auto() && width.is_auto() && !right.is_auto()) {
            auto result = calculate_shrink_to_fit_widths(box);
            solve_for_left();
            auto available_width = solve_for_width();
            width = CSS::Length(min(max(result.preferred_minimum_width, available_width.to_px(box)), result.preferred_width), CSS::Length::Type::Px);
        }

        // 2. 'left' and 'right' are 'auto' and 'width' is not 'auto',
        //    then if the 'direction' property of the element establishing
        //    the static-position containing block is 'ltr' set 'left'
        //    to the static position, otherwise set 'right' to the static position.
        //    Then solve for 'left' (if 'direction is 'rtl') or 'right' (if 'direction' is 'ltr').
        else if (left.is_auto() && right.is_auto() && !width.is_auto()) {
            // FIXME: Check direction
            // FIXME: Use the static-position containing block
            left = zero_value;
            right = solve_for_right();
        }

        // 3. 'width' and 'right' are 'auto' and 'left' is not 'auto',
        //    then the width is shrink-to-fit. Then solve for 'right'
        else if (width.is_auto() && right.is_auto() && !left.is_auto()) {
        Rule3:
            auto result = calculate_shrink_to_fit_widths(box);
            right = solve_for_right();
            auto available_width = solve_for_width();
            width = CSS::Length(min(max(result.preferred_minimum_width, available_width.to_px(box)), result.preferred_width), CSS::Length::Type::Px);
        }

        // 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve for 'left'
        else if (left.is_auto() && !width.is_auto() && !right.is_auto()) {
            left = solve_for_left();
        }

        // 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve for 'width'
        else if (width.is_auto() && !left.is_auto() && !right.is_auto()) {
            width = solve_for_width();
        }

        // 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve for 'right'
        else if (right.is_auto() && !left.is_auto() && !width.is_auto()) {
            right = solve_for_right();
        }

        return width;
    };

    auto specified_width = style.width().resolved_or_auto(box, containing_block.width());

    // 1. The tentative used width is calculated (without 'min-width' and 'max-width')
    auto used_width = try_compute_width(specified_width);

    // 2. The tentative used width is greater than 'max-width', the rules above are applied again,
    //    but this time using the computed value of 'max-width' as the computed value for 'width'.
    auto specified_max_width = style.max_width().resolved_or_auto(box, containing_block.width());
    if (!specified_max_width.is_auto()) {
        if (used_width.to_px(box) > specified_max_width.to_px(box)) {
            used_width = try_compute_width(specified_max_width);
        }
    }

    // 3. If the resulting width is smaller than 'min-width', the rules above are applied again,
    //    but this time using the value of 'min-width' as the computed value for 'width'.
    auto specified_min_width = style.min_width().resolved_or_auto(box, containing_block.width());
    if (!specified_min_width.is_auto()) {
        if (used_width.to_px(box) < specified_min_width.to_px(box)) {
            used_width = try_compute_width(specified_min_width);
        }
    }

    box.set_width(used_width.to_px(box));

    box.box_model().margin.left = margin_left;
    box.box_model().margin.right = margin_right;
    box.box_model().border.left = CSS::Length::make_px(border_left);
    box.box_model().border.right = CSS::Length::make_px(border_right);
    box.box_model().padding.left = padding_left;
    box.box_model().padding.right = padding_right;
}

void BlockFormattingContext::compute_height(Box& box)
{
    if (box.is_replaced()) {
        compute_height_for_block_level_replaced_element_in_normal_flow(downcast<ReplacedBox>(box));
        return;
    }

    auto& style = box.style();
    auto& containing_block = *box.containing_block();

    CSS::Length specified_height;

    if (style.height().is_percentage() && !containing_block.style().height().is_absolute()) {
        specified_height = CSS::Length::make_auto();
    } else {
        specified_height = style.height().resolved_or_auto(box, containing_block.height());
    }

    auto specified_max_height = style.max_height().resolved_or_auto(box, containing_block.height());

    box.box_model().margin.top = style.margin().top.resolved_or_zero(box, containing_block.width());
    box.box_model().margin.bottom = style.margin().bottom.resolved_or_zero(box, containing_block.width());
    box.box_model().border.top = CSS::Length::make_px(style.border_top().width);
    box.box_model().border.bottom = CSS::Length::make_px(style.border_bottom().width);
    box.box_model().padding.top = style.padding().top.resolved_or_zero(box, containing_block.width());
    box.box_model().padding.bottom = style.padding().bottom.resolved_or_zero(box, containing_block.width());

    if (!specified_height.is_auto()) {
        float used_height = specified_height.to_px(box);
        if (!specified_max_height.is_auto())
            used_height = min(used_height, specified_max_height.to_px(box));
        box.set_height(used_height);
    }
}

void BlockFormattingContext::layout_inline_children(Box& box, LayoutMode layout_mode)
{
    InlineFormattingContext context(box, this);
    context.run(box, layout_mode);
}

void BlockFormattingContext::layout_block_level_children(Box& box, LayoutMode layout_mode)
{
    float content_height = 0;
    float content_width = 0;

    box.for_each_child_of_type<Box>([&](auto& child_box) {
        if (child_box.is_absolutely_positioned()) {
            layout_absolutely_positioned_child(child_box);
            return IterationDecision::Continue;
        }

        if (child_box.is_floating()) {
            layout_floating_child(child_box, box);
            return IterationDecision::Continue;
        }

        compute_width(child_box);
        layout_inside(child_box, layout_mode);
        compute_height(child_box);

        if (child_box.is_replaced())
            place_block_level_replaced_element_in_normal_flow(child_box, box);
        else if (child_box.is_block())
            place_block_level_non_replaced_element_in_normal_flow(child_box, box);
        else
            dbgln("FIXME: BlockFormattingContext::layout_block_level_children() doesn't know how to place a {}", child_box.class_name());

        // FIXME: This should be factored differently. It's uncool that we mutate the tree *during* layout!
        //        Instead, we should generate the marker box during the tree build.
        if (is<ListItemBox>(child_box))
            downcast<ListItemBox>(child_box).layout_marker();

        content_height = max(content_height, child_box.effective_offset().y() + child_box.height() + child_box.box_model().margin_box(child_box).bottom);
        content_width = max(content_width, downcast<Box>(child_box).width());
        return IterationDecision::Continue;
    });

    if (layout_mode != LayoutMode::Default) {
        if (box.style().width().is_undefined() || box.style().width().is_auto())
            box.set_width(content_width);
    }

    // FIXME: It's not right to always shrink-wrap the box to the content here.
    box.set_height(content_height);
}

void BlockFormattingContext::place_block_level_replaced_element_in_normal_flow(Box& child_box, Box& containing_block)
{
    ASSERT(!containing_block.is_absolutely_positioned());
    auto& replaced_element_box_model = child_box.box_model();

    replaced_element_box_model.margin.top = child_box.style().margin().top.resolved_or_zero(containing_block, containing_block.width());
    replaced_element_box_model.margin.bottom = child_box.style().margin().bottom.resolved_or_zero(containing_block, containing_block.width());
    replaced_element_box_model.border.top = CSS::Length::make_px(child_box.style().border_top().width);
    replaced_element_box_model.border.bottom = CSS::Length::make_px(child_box.style().border_bottom().width);
    replaced_element_box_model.padding.top = child_box.style().padding().top.resolved_or_zero(containing_block, containing_block.width());
    replaced_element_box_model.padding.bottom = child_box.style().padding().bottom.resolved_or_zero(containing_block, containing_block.width());

    float x = replaced_element_box_model.margin.left.to_px(child_box)
        + replaced_element_box_model.border.left.to_px(child_box)
        + replaced_element_box_model.padding.left.to_px(child_box)
        + replaced_element_box_model.offset.left.to_px(child_box);

    float y = replaced_element_box_model.margin_box(child_box).top + containing_block.box_model().offset.top.to_px(child_box);

    child_box.set_offset(x, y);
}

void BlockFormattingContext::place_block_level_non_replaced_element_in_normal_flow(Box& child_box, Box& containing_block)
{
    auto& box_model = child_box.box_model();
    auto& style = child_box.style();

    box_model.margin.top = style.margin().top.resolved_or_zero(containing_block, containing_block.width());
    box_model.margin.bottom = style.margin().bottom.resolved_or_zero(containing_block, containing_block.width());
    box_model.border.top = CSS::Length::make_px(style.border_top().width);
    box_model.border.bottom = CSS::Length::make_px(style.border_bottom().width);
    box_model.padding.top = style.padding().top.resolved_or_zero(containing_block, containing_block.width());
    box_model.padding.bottom = style.padding().bottom.resolved_or_zero(containing_block, containing_block.width());

    float x = box_model.margin.left.to_px(child_box)
        + box_model.border.left.to_px(child_box)
        + box_model.padding.left.to_px(child_box)
        + box_model.offset.left.to_px(child_box);

    if (containing_block.style().text_align() == CSS::TextAlign::VendorSpecificCenter) {
        x = (containing_block.width() / 2) - child_box.width() / 2;
    }

    float y = box_model.margin_box(child_box).top
        + box_model.offset.top.to_px(child_box);

    // NOTE: Empty (0-height) preceding siblings have their margins collapsed with *their* preceding sibling, etc.
    float collapsed_bottom_margin_of_preceding_siblings = 0;

    auto* relevant_sibling = child_box.previous_sibling_of_type<Layout::BlockBox>();
    while (relevant_sibling != nullptr) {
        if (!relevant_sibling->is_absolutely_positioned() && !relevant_sibling->is_floating()) {
            collapsed_bottom_margin_of_preceding_siblings = max(collapsed_bottom_margin_of_preceding_siblings, relevant_sibling->box_model().margin.bottom.to_px(*relevant_sibling));
            if (relevant_sibling->height() > 0)
                break;
        }
        relevant_sibling = relevant_sibling->previous_sibling();
    }

    if (relevant_sibling) {
        y += relevant_sibling->effective_offset().y()
            + relevant_sibling->height()
            + relevant_sibling->box_model().border_box(*relevant_sibling).bottom;

        // Collapse top margin with bottom margin of preceding siblings if needed
        float my_margin_top = box_model.margin.top.to_px(child_box);

        if (my_margin_top < 0 || collapsed_bottom_margin_of_preceding_siblings < 0) {
            // Negative margins present.
            float largest_negative_margin = -min(my_margin_top, collapsed_bottom_margin_of_preceding_siblings);
            float largest_positive_margin = (my_margin_top < 0 && collapsed_bottom_margin_of_preceding_siblings < 0) ? 0 : max(my_margin_top, collapsed_bottom_margin_of_preceding_siblings);
            float final_margin = largest_positive_margin - largest_negative_margin;
            y += final_margin - my_margin_top;
        } else if (collapsed_bottom_margin_of_preceding_siblings > my_margin_top) {
            // Sibling's margin is larger than mine, adjust so we use sibling's.
            y += collapsed_bottom_margin_of_preceding_siblings - my_margin_top;
        }
    }

    if (child_box.style().clear() == CSS::Clear::Left || child_box.style().clear() == CSS::Clear::Both) {
        if (!m_left_floating_boxes.is_empty()) {
            float clearance_y = 0;
            for (auto* floating_box : m_left_floating_boxes) {
                clearance_y = max(clearance_y, floating_box->effective_offset().y() + floating_box->box_model().margin_box(*floating_box).bottom);
            }
            y = max(y, clearance_y);
            m_left_floating_boxes.clear();
        }
    }

    if (child_box.style().clear() == CSS::Clear::Right || child_box.style().clear() == CSS::Clear::Both) {
        if (!m_right_floating_boxes.is_empty()) {
            float clearance_y = 0;
            for (auto* floating_box : m_right_floating_boxes) {
                clearance_y = max(clearance_y, floating_box->effective_offset().y() + floating_box->box_model().margin_box(*floating_box).bottom);
            }
            y = max(y, clearance_y);
            m_right_floating_boxes.clear();
        }
    }

    child_box.set_offset(x, y);
}

void BlockFormattingContext::layout_initial_containing_block(LayoutMode layout_mode)
{
    auto viewport_rect = context_box().frame().viewport_rect();

    auto& icb = downcast<Layout::InitialContainingBlockBox>(context_box());
    icb.build_stacking_context_tree();

    icb.set_width(viewport_rect.width());

    layout_block_level_children(context_box(), layout_mode);

    ASSERT(!icb.children_are_inline());

    // FIXME: The ICB should have the height of the viewport.
    //        Instead of auto-sizing the ICB, we should spill into overflow.
    float lowest_bottom = 0;
    icb.for_each_child_of_type<Box>([&](auto& child) {
        lowest_bottom = max(lowest_bottom, child.absolute_rect().bottom());
    });
    icb.set_height(lowest_bottom);

    // FIXME: This is a total hack. Make sure any GUI::Widgets are moved into place after layout.
    //        We should stop embedding GUI::Widgets entirely, since that won't work out-of-process.
    icb.for_each_in_subtree_of_type<Layout::WidgetBox>([&](auto& widget) {
        widget.update_widget();
        return IterationDecision::Continue;
    });
}

static Gfx::FloatRect rect_in_coordinate_space(const Box& box, const Box& context_box)
{
    Gfx::FloatRect rect { box.effective_offset(), box.size() };
    for (auto* ancestor = box.parent(); ancestor; ancestor = ancestor->parent()) {
        if (is<Box>(*ancestor)) {
            auto offset = downcast<Box>(*ancestor).effective_offset();
            rect.move_by(offset);
        }
        if (ancestor == &context_box)
            break;
    }
    return rect;
}

void BlockFormattingContext::layout_floating_child(Box& box, Box& containing_block)
{
    ASSERT(box.is_floating());

    compute_width(box);
    layout_inside(box, LayoutMode::Default);
    compute_height(box);

    // First we place the box normally (to get the right y coordinate.)
    place_block_level_non_replaced_element_in_normal_flow(box, containing_block);

    // Then we float it to the left or right.
    float x = box.effective_offset().x();

    auto box_in_context_rect = rect_in_coordinate_space(box, context_box());
    float y_in_context_box = box_in_context_rect.y();

    // Next, float to the left and/or right
    if (box.style().float_() == CSS::Float::Left) {
        if (!m_left_floating_boxes.is_empty()) {
            auto& previous_floating_box = *m_left_floating_boxes.last();
            auto previous_rect = rect_in_coordinate_space(previous_floating_box, context_box());
            if (previous_rect.contains_vertically(y_in_context_box)) {
                // This box touches another already floating box. Stack to the right.
                x = previous_floating_box.effective_offset().x() + previous_floating_box.width();
            } else {
                // This box does not touch another floating box, go all the way to the left.
                x = 0;
                // Also, forget all previous left-floating boxes while we're here since they're no longer relevant.
                m_left_floating_boxes.clear();
            }
        } else {
            // This is the first left-floating box. Go all the way to the left.
            x = 0;
        }
        m_left_floating_boxes.append(&box);
    } else if (box.style().float_() == CSS::Float::Right) {
        if (!m_right_floating_boxes.is_empty()) {
            auto& previous_floating_box = *m_right_floating_boxes.last();
            auto previous_rect = rect_in_coordinate_space(previous_floating_box, context_box());
            if (previous_rect.contains_vertically(y_in_context_box)) {
                // This box touches another already floating box. Stack to the left.
                x = previous_floating_box.effective_offset().x() - box.width();
            } else {
                // This box does not touch another floating box, go all the way to the right.
                x = containing_block.width() - box.width();
                // Also, forget all previous right-floating boxes while we're here since they're no longer relevant.
                m_right_floating_boxes.clear();
            }
        } else {
            // This is the first right-floating box. Go all the way to the right.
            x = containing_block.width() - box.width();
        }
        m_right_floating_boxes.append(&box);
    }

    box.set_offset(x, box.effective_offset().y());
}

void BlockFormattingContext::layout_absolutely_positioned_child(Box& box)
{
    auto& containing_block = context_box();
    auto& box_model = box.box_model();
    auto zero_value = CSS::Length::make_px(0);

    auto specified_width = box.style().width().resolved_or_auto(box, containing_block.width());

    compute_width(box);
    layout_inside(box, LayoutMode::Default);
    compute_height(box);

    box_model.margin.left = box.style().margin().left.resolved_or_auto(box, containing_block.width());
    box_model.margin.top = box.style().margin().top.resolved_or_auto(box, containing_block.height());
    box_model.margin.right = box.style().margin().right.resolved_or_auto(box, containing_block.width());
    box_model.margin.bottom = box.style().margin().bottom.resolved_or_auto(box, containing_block.height());

    box_model.border.left = CSS::Length::make_px(box.style().border_left().width);
    box_model.border.right = CSS::Length::make_px(box.style().border_right().width);
    box_model.border.top = CSS::Length::make_px(box.style().border_top().width);
    box_model.border.bottom = CSS::Length::make_px(box.style().border_bottom().width);

    box_model.offset.left = box.style().offset().left.resolved_or_auto(box, containing_block.width());
    box_model.offset.top = box.style().offset().top.resolved_or_auto(box, containing_block.height());
    box_model.offset.right = box.style().offset().right.resolved_or_auto(box, containing_block.width());
    box_model.offset.bottom = box.style().offset().bottom.resolved_or_auto(box, containing_block.height());

    if (box_model.offset.left.is_auto() && specified_width.is_auto() && box_model.offset.right.is_auto()) {
        if (box_model.margin.left.is_auto())
            box_model.margin.left = zero_value;
        if (box_model.margin.right.is_auto())
            box_model.margin.right = zero_value;
    }

    Gfx::FloatPoint used_offset;

    if (!box_model.offset.left.is_auto()) {
        float x_offset = box_model.offset.left.to_px(box)
            + box_model.border_box(box).left;
        used_offset.set_x(x_offset + box_model.margin.left.to_px(box));
    } else if (!box_model.offset.right.is_auto()) {
        float x_offset = 0
            - box_model.offset.right.to_px(box)
            - box_model.border_box(box).right;
        used_offset.set_x(containing_block.width() + x_offset - box.width() - box_model.margin.right.to_px(box));
    } else {
        float x_offset = box_model.margin_box(box).left;
        used_offset.set_x(x_offset);
    }

    if (!box_model.offset.top.is_auto()) {
        float y_offset = box_model.offset.top.to_px(box)
            + box_model.border_box(box).top;
        used_offset.set_y(y_offset + box_model.margin.top.to_px(box));
    } else if (!box_model.offset.bottom.is_auto()) {
        float y_offset = 0
            - box_model.offset.bottom.to_px(box)
            - box_model.border_box(box).bottom;
        used_offset.set_y(containing_block.height() + y_offset - box.height() - box_model.margin.bottom.to_px(box));
    } else {
        float y_offset = box_model.margin_box(box).top;
        used_offset.set_y(y_offset);
    }

    box.set_offset(used_offset);
}

}
