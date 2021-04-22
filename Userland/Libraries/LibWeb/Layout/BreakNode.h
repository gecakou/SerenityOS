/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/HTML/HTMLBRElement.h>
#include <LibWeb/Layout/Node.h>

namespace Web::Layout {

class BreakNode final : public NodeWithStyleAndBoxModelMetrics {
public:
    BreakNode(DOM::Document&, HTML::HTMLBRElement&);
    virtual ~BreakNode() override;

    const HTML::HTMLBRElement& dom_node() const { return downcast<HTML::HTMLBRElement>(*Node::dom_node()); }

private:
    virtual void split_into_lines(InlineFormattingContext&, LayoutMode) override;
};

}
