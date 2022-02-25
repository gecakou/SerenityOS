/*
 * Copyright (c) 2018-2022, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Optional.h>
#include <AK/TemporaryChange.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibWeb/DOM/ParentNode.h>
#include <LibWeb/DOM/ShadowRoot.h>
#include <LibWeb/Dump.h>
#include <LibWeb/Layout/InitialContainingBlock.h>
#include <LibWeb/Layout/ListItemBox.h>
#include <LibWeb/Layout/ListItemMarkerBox.h>
#include <LibWeb/Layout/Node.h>
#include <LibWeb/Layout/TableBox.h>
#include <LibWeb/Layout/TableCellBox.h>
#include <LibWeb/Layout/TableRowBox.h>
#include <LibWeb/Layout/TextNode.h>
#include <LibWeb/Layout/TreeBuilder.h>

namespace Web::Layout {

TreeBuilder::TreeBuilder()
{
}

// The insertion_parent_for_*() functions maintain the invariant that block-level boxes must have either
// only block-level children or only inline-level children.

static Layout::Node& insertion_parent_for_inline_node(Layout::NodeWithStyle& layout_parent)
{
    if (layout_parent.is_inline() && !layout_parent.is_inline_block())
        return layout_parent;

    if (layout_parent.computed_values().display().is_flex_inside()) {
        layout_parent.append_child(layout_parent.create_anonymous_wrapper());
    }

    if (!layout_parent.has_children() || layout_parent.children_are_inline())
        return layout_parent;

    // Parent has block-level children, insert into an anonymous wrapper block (and create it first if needed)
    if (!layout_parent.last_child()->is_anonymous() || !layout_parent.last_child()->children_are_inline()) {
        layout_parent.append_child(layout_parent.create_anonymous_wrapper());
    }
    return *layout_parent.last_child();
}

static Layout::Node& insertion_parent_for_block_node(Layout::Node& layout_parent, Layout::Node& layout_node)
{
    if (!layout_parent.has_children()) {
        // Parent block has no children, insert this block into parent.
        return layout_parent;
    }

    if (!layout_parent.children_are_inline()) {
        // Parent block has block-level children, insert this block into parent.
        return layout_parent;
    }

    // Parent block has inline-level children (our siblings).
    // First move these siblings into an anonymous wrapper block.
    NonnullRefPtrVector<Layout::Node> children;
    while (RefPtr<Layout::Node> child = layout_parent.first_child()) {
        layout_parent.remove_child(*child);
        children.append(child.release_nonnull());
    }
    layout_parent.append_child(adopt_ref(*new BlockContainer(layout_node.document(), nullptr, layout_parent.computed_values().clone_inherited_values())));
    layout_parent.set_children_are_inline(false);
    for (auto& child : children) {
        layout_parent.last_child()->append_child(child);
    }
    layout_parent.last_child()->set_children_are_inline(true);
    // Then it's safe to insert this block into parent.
    return layout_parent;
}

void TreeBuilder::create_layout_tree(DOM::Node& dom_node, TreeBuilder::Context& context)
{
    // If the parent doesn't have a layout node, we don't need one either.
    if (dom_node.parent_or_shadow_host() && !dom_node.parent_or_shadow_host()->layout_node())
        return;

    Optional<TemporaryChange<bool>> has_svg_root_change;

    if (dom_node.is_svg_container()) {
        has_svg_root_change.emplace(context.has_svg_root, true);
    } else if (dom_node.requires_svg_container() && !context.has_svg_root) {
        return;
    }

    auto& document = dom_node.document();
    auto& style_computer = document.style_computer();
    RefPtr<Layout::Node> layout_node;

    if (is<DOM::Element>(dom_node)) {
        auto& element = static_cast<DOM::Element&>(dom_node);
        auto style = style_computer.compute_style(element);
        if (style->display().is_none())
            return;
        element.set_specified_css_values(style);
        layout_node = element.create_layout_node(move(style));
    } else if (is<DOM::Document>(dom_node)) {
        auto style = style_computer.create_document_style();
        layout_node = adopt_ref(*new Layout::InitialContainingBlock(static_cast<DOM::Document&>(dom_node), move(style)));
    } else if (is<DOM::Text>(dom_node)) {
        layout_node = adopt_ref(*new Layout::TextNode(document, static_cast<DOM::Text&>(dom_node)));
    } else if (is<DOM::ShadowRoot>(dom_node)) {
        layout_node = adopt_ref(*new Layout::BlockContainer(document, &static_cast<DOM::ShadowRoot&>(dom_node), CSS::ComputedValues {}));
    }

    if (!layout_node)
        return;

    auto insert_node_into_inline_or_block_ancestor = [this](auto& node, bool prepend = false) {
        if (node->is_inline() && !(node->is_inline_block() && m_parent_stack.last()->computed_values().display().is_flex_inside())) {
            // Inlines can be inserted into the nearest ancestor.
            auto& insertion_point = insertion_parent_for_inline_node(*m_parent_stack.last());
            if (prepend)
                insertion_point.prepend_child(*node);
            else
                insertion_point.append_child(*node);
            insertion_point.set_children_are_inline(true);
        } else {
            // Non-inlines can't be inserted into an inline parent, so find the nearest non-inline ancestor.
            auto& nearest_non_inline_ancestor = [&]() -> Layout::Node& {
                for (ssize_t i = m_parent_stack.size() - 1; i >= 0; --i) {
                    if (!m_parent_stack[i]->is_inline() || m_parent_stack[i]->is_inline_block())
                        return *m_parent_stack[i];
                }
                VERIFY_NOT_REACHED();
            }();
            auto& insertion_point = insertion_parent_for_block_node(nearest_non_inline_ancestor, *node);
            if (prepend)
                insertion_point.prepend_child(*node);
            else
                insertion_point.append_child(*node);
            insertion_point.set_children_are_inline(false);
        }
    };

    if (!dom_node.parent_or_shadow_host()) {
        m_layout_root = layout_node;
    } else {
        insert_node_into_inline_or_block_ancestor(layout_node);
    }

    auto* shadow_root = is<DOM::Element>(dom_node) ? verify_cast<DOM::Element>(dom_node).shadow_root() : nullptr;

    if ((dom_node.has_children() || shadow_root) && layout_node->can_have_children()) {
        push_parent(verify_cast<NodeWithStyle>(*layout_node));
        if (shadow_root)
            create_layout_tree(*shadow_root, context);
        verify_cast<DOM::ParentNode>(dom_node).for_each_child([&](auto& dom_child) {
            create_layout_tree(dom_child, context);
        });
        pop_parent();
    }

    // Add nodes for the ::before and ::after pseudo-elements.
    if (is<DOM::Element>(dom_node)) {
        auto& element = static_cast<DOM::Element&>(dom_node);
        auto create_pseudo_element_if_needed = [&](CSS::Selector::PseudoElement pseudo_element) -> RefPtr<Node> {
            auto pseudo_element_style = style_computer.compute_style(element, pseudo_element);
            auto pseudo_element_content = pseudo_element_style->content();
            auto pseudo_element_display = pseudo_element_style->display();
            // ::before and ::after only exist if they have content. `content: normal` computes to `none` for them.
            // We also don't create them if they are `display: none`.
            if (pseudo_element_display.is_none()
                || pseudo_element_content.type == CSS::ContentData::Type::Normal
                || pseudo_element_content.type == CSS::ContentData::Type::None)
                return nullptr;

            if (auto pseudo_element_node = DOM::Element::create_layout_node_for_display_type(document, pseudo_element_display, move(pseudo_element_style), nullptr)) {
                // FIXME: Handle images, and multiple values
                if (pseudo_element_content.type == CSS::ContentData::Type::String) {
                    auto text = adopt_ref(*new DOM::Text(document, pseudo_element_content.data));
                    auto text_node = adopt_ref(*new TextNode(document, text));
                    push_parent(verify_cast<NodeWithStyle>(*pseudo_element_node));
                    insert_node_into_inline_or_block_ancestor(text_node);
                    pop_parent();
                } else {
                    TODO();
                }
                return pseudo_element_node;
            }

            return nullptr;
        };

        push_parent(verify_cast<NodeWithStyle>(*layout_node));
        if (auto before_node = create_pseudo_element_if_needed(CSS::Selector::PseudoElement::Before))
            insert_node_into_inline_or_block_ancestor(before_node, true);
        if (auto after_node = create_pseudo_element_if_needed(CSS::Selector::PseudoElement::After))
            insert_node_into_inline_or_block_ancestor(after_node);
        pop_parent();
    }

    if (is<ListItemBox>(*layout_node)) {
        int child_index = layout_node->parent()->index_of_child<ListItemBox>(*layout_node).value();
        auto marker_style = static_cast<DOM::Element const&>(dom_node).specified_css_values();
        auto list_item_marker = adopt_ref(*new ListItemMarkerBox(document, layout_node->computed_values().list_style_type(), child_index + 1, *marker_style));
        if (layout_node->first_child())
            list_item_marker->set_inline(layout_node->first_child()->is_inline());
        static_cast<ListItemBox&>(*layout_node).set_marker(list_item_marker);
        layout_node->append_child(move(list_item_marker));
    }
}

RefPtr<Node> TreeBuilder::build(DOM::Node& dom_node)
{
    if (dom_node.parent()) {
        // We're building a partial layout tree, so start by building up the stack of parent layout nodes.
        for (auto* ancestor = dom_node.parent()->layout_node(); ancestor; ancestor = ancestor->parent())
            m_parent_stack.prepend(verify_cast<NodeWithStyle>(ancestor));
    }

    Context context;
    create_layout_tree(dom_node, context);

    if (auto* root = dom_node.document().layout_node())
        fixup_tables(*root);

    return move(m_layout_root);
}

template<CSS::Display::Internal internal, typename Callback>
void TreeBuilder::for_each_in_tree_with_internal_display(NodeWithStyle& root, Callback callback)
{
    root.for_each_in_inclusive_subtree_of_type<Box>([&](auto& box) {
        auto const& display = box.computed_values().display();
        if (display.is_internal() && display.internal() == internal)
            callback(box);
        return IterationDecision::Continue;
    });
}

template<CSS::Display::Inside inside, typename Callback>
void TreeBuilder::for_each_in_tree_with_inside_display(NodeWithStyle& root, Callback callback)
{
    root.for_each_in_inclusive_subtree_of_type<Box>([&](auto& box) {
        auto const& display = box.computed_values().display();
        if (display.it_outside_and_inside() && display.inside() == inside)
            callback(box);
        return IterationDecision::Continue;
    });
}

void TreeBuilder::fixup_tables(NodeWithStyle& root)
{
    // NOTE: Even if we only do a partial build, we always do fixup from the root.

    remove_irrelevant_boxes(root);
    generate_missing_child_wrappers(root);
    generate_missing_parents(root);
}

void TreeBuilder::remove_irrelevant_boxes(NodeWithStyle& root)
{
    // The following boxes are discarded as if they were display:none:

    NonnullRefPtrVector<Node> to_remove;

    // Children of a table-column.
    for_each_in_tree_with_internal_display<CSS::Display::Internal::TableColumn>(root, [&](Box& table_column) {
        table_column.for_each_child([&](auto& child) {
            to_remove.append(child);
        });
    });

    // Children of a table-column-group which are not a table-column.
    for_each_in_tree_with_internal_display<CSS::Display::Internal::TableColumnGroup>(root, [&](Box& table_column_group) {
        table_column_group.for_each_child([&](auto& child) {
            if (child.computed_values().display().is_table_column())
                to_remove.append(child);
        });
    });

    // FIXME:
    // Anonymous inline boxes which contain only white space and are between two immediate siblings each of which is a table-non-root box.
    // Anonymous inline boxes which meet all of the following criteria:
    // - they contain only white space
    // - they are the first and/or last child of a tabular container
    // - whose immediate sibling, if any, is a table-non-root box

    for (auto& box : to_remove)
        box.parent()->remove_child(box);
}

static bool is_table_track(CSS::Display display)
{
    return display.is_table_row() || display.is_table_column();
}

static bool is_table_track_group(CSS::Display display)
{
    // Unless explicitly mentioned otherwise, mentions of table-row-groups in this spec also encompass the specialized
    // table-header-groups and table-footer-groups.
    return display.is_table_row_group()
        || display.is_table_header_group()
        || display.is_table_footer_group()
        || display.is_table_column_group();
}

static bool is_not_proper_table_child(const Node& node)
{
    if (!node.has_style())
        return true;
    auto display = node.computed_values().display();
    return !is_table_track_group(display) && !is_table_track(display) && !display.is_table_caption();
}

static bool is_not_table_row(const Node& node)
{
    if (!node.has_style())
        return true;
    auto display = node.computed_values().display();
    return !display.is_table_row();
}

static bool is_not_table_cell(const Node& node)
{
    if (!node.has_style())
        return true;
    auto display = node.computed_values().display();
    return !display.is_table_cell();
}

static bool is_ignorable_whitespace(Layout::Node const& node)
{
    if (node.is_text_node() && static_cast<TextNode const&>(node).text_for_rendering().is_whitespace())
        return true;

    if (node.is_anonymous() && node.is_block_container() && static_cast<BlockContainer const&>(node).children_are_inline()) {
        bool contains_only_white_space = true;
        node.for_each_in_inclusive_subtree_of_type<TextNode>([&contains_only_white_space](auto& text_node) {
            if (!text_node.text_for_rendering().is_whitespace()) {
                contains_only_white_space = false;
                return IterationDecision::Break;
            }
            return IterationDecision::Continue;
        });
        if (contains_only_white_space)
            return true;
    }

    return false;
}

template<typename Matcher, typename Callback>
static void for_each_sequence_of_consecutive_children_matching(NodeWithStyle& parent, Matcher matcher, Callback callback)
{
    NonnullRefPtrVector<Node> sequence;

    auto sequence_is_all_ignorable_whitespace = [&]() -> bool {
        for (auto& node : sequence) {
            if (!is_ignorable_whitespace(node))
                return false;
        }
        return true;
    };

    Node* next_sibling = nullptr;
    for (auto* child = parent.first_child(); child; child = next_sibling) {
        next_sibling = child->next_sibling();
        if (matcher(*child)) {
            sequence.append(*child);
        } else {
            if (!sequence.is_empty()) {
                if (!sequence_is_all_ignorable_whitespace())
                    callback(sequence, next_sibling);
                sequence.clear();
            }
        }
    }
    if (sequence.is_empty() && !sequence_is_all_ignorable_whitespace())
        callback(sequence, nullptr);
}

template<typename WrapperBoxType>
static void wrap_in_anonymous(NonnullRefPtrVector<Node>& sequence, Node* nearest_sibling)
{
    VERIFY(!sequence.is_empty());
    auto& parent = *sequence.first().parent();
    auto computed_values = parent.computed_values().clone_inherited_values();
    static_cast<CSS::MutableComputedValues&>(computed_values).set_display(WrapperBoxType::static_display());
    auto wrapper = adopt_ref(*new WrapperBoxType(parent.document(), nullptr, move(computed_values)));
    for (auto& child : sequence) {
        parent.remove_child(child);
        wrapper->append_child(child);
    }
    if (nearest_sibling)
        parent.insert_before(move(wrapper), *nearest_sibling);
    else
        parent.append_child(move(wrapper));
}

void TreeBuilder::generate_missing_child_wrappers(NodeWithStyle& root)
{
    // An anonymous table-row box must be generated around each sequence of consecutive children of a table-root box which are not proper table child boxes.
    for_each_in_tree_with_inside_display<CSS::Display::Inside::Table>(root, [&](auto& parent) {
        for_each_sequence_of_consecutive_children_matching(parent, is_not_proper_table_child, [&](auto sequence, auto nearest_sibling) {
            wrap_in_anonymous<TableRowBox>(sequence, nearest_sibling);
        });
    });

    // An anonymous table-row box must be generated around each sequence of consecutive children of a table-row-group box which are not table-row boxes.
    for_each_in_tree_with_internal_display<CSS::Display::Internal::TableRowGroup>(root, [&](auto& parent) {
        for_each_sequence_of_consecutive_children_matching(parent, is_not_table_row, [&](auto& sequence, auto nearest_sibling) {
            wrap_in_anonymous<TableRowBox>(sequence, nearest_sibling);
        });
    });
    // Unless explicitly mentioned otherwise, mentions of table-row-groups in this spec also encompass the specialized
    // table-header-groups and table-footer-groups.
    for_each_in_tree_with_internal_display<CSS::Display::Internal::TableHeaderGroup>(root, [&](auto& parent) {
        for_each_sequence_of_consecutive_children_matching(parent, is_not_table_row, [&](auto& sequence, auto nearest_sibling) {
            wrap_in_anonymous<TableRowBox>(sequence, nearest_sibling);
        });
    });
    for_each_in_tree_with_internal_display<CSS::Display::Internal::TableFooterGroup>(root, [&](auto& parent) {
        for_each_sequence_of_consecutive_children_matching(parent, is_not_table_row, [&](auto& sequence, auto nearest_sibling) {
            wrap_in_anonymous<TableRowBox>(sequence, nearest_sibling);
        });
    });

    // An anonymous table-cell box must be generated around each sequence of consecutive children of a table-row box which are not table-cell boxes. !Testcase
    for_each_in_tree_with_internal_display<CSS::Display::Internal::TableRow>(root, [&](auto& parent) {
        for_each_sequence_of_consecutive_children_matching(parent, is_not_table_cell, [&](auto& sequence, auto nearest_sibling) {
            wrap_in_anonymous<TableCellBox>(sequence, nearest_sibling);
        });
    });
}

void TreeBuilder::generate_missing_parents(NodeWithStyle&)
{
    // FIXME: Implement.
}

}
