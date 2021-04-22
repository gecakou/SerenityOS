/*
 * Copyright (c) 2020, The SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/HTML/HTMLElement.h>

namespace Web::HTML {

class HTMLDataListElement final : public HTMLElement {
public:
    using WrapperType = Bindings::HTMLDataListElementWrapper;

    HTMLDataListElement(DOM::Document&, QualifiedName);
    virtual ~HTMLDataListElement() override;
};

}
