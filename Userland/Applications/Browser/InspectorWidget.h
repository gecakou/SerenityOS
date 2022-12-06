/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021-2022, Sam Atkins <atkinssj@serenityos.org>
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "ElementSizePreviewWidget.h"
#include <LibGUI/Widget.h>
#include <LibWeb/CSS/Selector.h>
#include <LibWeb/Forward.h>
#include <LibWeb/Layout/BoxModelMetrics.h>
#include <LibWebView/Forward.h>

namespace Browser {

class InspectorWidget final : public GUI::Widget {
    C_OBJECT(InspectorWidget)
public:
    struct Selection {
        i32 dom_node_id { 0 };
        Optional<Web::CSS::Selector::PseudoElement> pseudo_element {};

        bool operator==(Selection const& other) const
        {
            return dom_node_id == other.dom_node_id && pseudo_element == other.pseudo_element;
        }

        DeprecatedString to_deprecated_string() const
        {
            if (pseudo_element.has_value())
                return DeprecatedString::formatted("id: {}, pseudo: {}", dom_node_id, Web::CSS::pseudo_element_name(pseudo_element.value()));
            return DeprecatedString::formatted("id: {}", dom_node_id);
        }
    };

    virtual ~InspectorWidget() = default;

    void set_web_view(NonnullRefPtr<WebView::OutOfProcessWebView> web_view) { m_web_view = web_view; }
    void set_dom_json(DeprecatedString);
    void clear_dom_json();
    void set_dom_node_properties_json(Selection, DeprecatedString specified_values_json, DeprecatedString computed_values_json, DeprecatedString custom_properties_json, DeprecatedString node_box_sizing_json);

    void set_selection(Selection);
    void select_default_node();

private:
    InspectorWidget();

    void set_selection(GUI::ModelIndex);
    void load_style_json(DeprecatedString specified_values_json, DeprecatedString computed_values_json, DeprecatedString custom_properties_json);
    void update_node_box_model(Optional<DeprecatedString> node_box_sizing_json);
    void clear_style_json();
    void clear_node_box_model();

    RefPtr<WebView::OutOfProcessWebView> m_web_view;

    RefPtr<GUI::TreeView> m_dom_tree_view;
    RefPtr<GUI::TableView> m_computed_style_table_view;
    RefPtr<GUI::TableView> m_resolved_style_table_view;
    RefPtr<GUI::TableView> m_custom_properties_table_view;
    RefPtr<ElementSizePreviewWidget> m_element_size_view;

    Web::Layout::BoxModelMetrics m_node_box_sizing;

    Optional<DeprecatedString> m_dom_json;
    Optional<Selection> m_pending_selection;
    Selection m_selection;
    Optional<DeprecatedString> m_selection_specified_values_json;
    Optional<DeprecatedString> m_selection_computed_values_json;
    Optional<DeprecatedString> m_selection_custom_properties_json;
};

}
