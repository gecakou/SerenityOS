/*
 * Copyright (c) 2020, The SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/HTML/HTMLParagraphElement.h>

namespace Web::HTML {

HTMLParagraphElement::HTMLParagraphElement(DOM::Document& document, QualifiedName qualified_name)
    : HTMLElement(document, move(qualified_name))
{
}

HTMLParagraphElement::~HTMLParagraphElement()
{
}

}
