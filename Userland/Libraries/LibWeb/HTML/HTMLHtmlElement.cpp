/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/HTMLHtmlElement.h>

namespace Web::HTML {

HTMLHtmlElement::HTMLHtmlElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : HTMLElement(document, move(qualified_name))
{
    set_prototype(&Bindings::cached_web_prototype(realm(), "HTMLHtmlElement"));
}

HTMLHtmlElement::~HTMLHtmlElement() = default;

bool HTMLHtmlElement::should_use_body_background_properties() const
{
    auto background_color = layout_node()->computed_values().background_color();
    auto const& background_layers = layout_node()->background_layers();

    for (auto& layer : background_layers) {
        if (layer.background_image)
            return false;
    }

    return (background_color == Color::Transparent);
}

}
