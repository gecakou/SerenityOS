/*
 * Copyright (c) 2020, The SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/HTML/HTMLElement.h>

namespace Web::HTML {

class HTMLSpanElement final : public HTMLElement {
public:
    using WrapperType = Bindings::HTMLSpanElementWrapper;

    HTMLSpanElement(DOM::Document&, QualifiedName);
    virtual ~HTMLSpanElement() override;
};

}
