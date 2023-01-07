/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/DOM/ARIARoleNames.h>
#include <LibWeb/HTML/HTMLElement.h>

namespace Web::HTML {

class HTMLHtmlElement final : public HTMLElement {
    WEB_PLATFORM_OBJECT(HTMLHtmlElement, HTMLElement);

public:
    virtual ~HTMLHtmlElement() override;

    bool should_use_body_background_properties() const;

    // https://www.w3.org/TR/html-aria/#el-html
    virtual FlyString default_role() const override { return DOM::ARIARoleNames::document; }

private:
    HTMLHtmlElement(DOM::Document&, DOM::QualifiedName);

    virtual bool is_html_html_element() const override { return true; }
};

}

namespace Web::DOM {
template<>
inline bool Node::fast_is<HTML::HTMLHtmlElement>() const { return is_html_html_element(); }
}
