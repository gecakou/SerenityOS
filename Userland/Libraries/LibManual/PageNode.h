/*
 * Copyright (c) 2019-2020, Sergey Bugaev <bugaevc@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <LibManual/Node.h>

namespace Manual {

class SectionNode;

class PageNode : public Node {
public:
    virtual ~PageNode() override = default;

    PageNode(NonnullRefPtr<SectionNode> section, StringView page)
        : m_section(move(section))
        , m_page(page)
    {
    }

    virtual NonnullRefPtrVector<Node>& children() const override;
    virtual Node const* parent() const override;
    virtual String name() const override { return m_page; };
    virtual bool is_page() const override { return true; }

    String path() const;

private:
    NonnullRefPtr<SectionNode> m_section;
    String m_page;
};

}
