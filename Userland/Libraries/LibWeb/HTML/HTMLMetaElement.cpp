/*
 * Copyright (c) 2020, The SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/HTML/HTMLMetaElement.h>

namespace Web::HTML {

HTMLMetaElement::HTMLMetaElement(DOM::Document& document, QualifiedName qualified_name)
    : HTMLElement(document, move(qualified_name))
{
}

HTMLMetaElement::~HTMLMetaElement()
{
}

}
