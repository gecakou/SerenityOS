/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/Object.h>
#include <LibWeb/DOM/NodeFilter.h>

namespace Web::DOM {

// https://dom.spec.whatwg.org/#nodeiterator
class NodeIterator final : public Bindings::PlatformObject {
    JS_OBJECT(NodeIterator, Bindings::PlatformObject);

public:
    static JS::NonnullGCPtr<NodeIterator> create(Node& root, unsigned what_to_show, JS::GCPtr<NodeFilter>);

    NodeIterator(Node& root);
    virtual ~NodeIterator() override;

    NodeIterator& impl() { return *this; }

    NonnullRefPtr<Node> root() { return m_root; }
    NonnullRefPtr<Node> reference_node() { return m_reference.node; }
    bool pointer_before_reference_node() const { return m_reference.is_before_node; }
    unsigned what_to_show() const { return m_what_to_show; }

    NodeFilter* filter() { return m_filter.ptr(); }

    JS::ThrowCompletionOr<RefPtr<Node>> next_node();
    JS::ThrowCompletionOr<RefPtr<Node>> previous_node();

    void detach();

    void run_pre_removing_steps(Node&);

private:
    virtual void visit_edges(Cell::Visitor&) override;

    enum class Direction {
        Next,
        Previous,
    };

    JS::ThrowCompletionOr<RefPtr<Node>> traverse(Direction);

    JS::ThrowCompletionOr<NodeFilter::Result> filter(Node&);

    // https://dom.spec.whatwg.org/#concept-traversal-root
    NonnullRefPtr<DOM::Node> m_root;

    struct NodePointer {
        NonnullRefPtr<DOM::Node> node;

        // https://dom.spec.whatwg.org/#nodeiterator-pointer-before-reference
        bool is_before_node { true };
    };

    void run_pre_removing_steps_with_node_pointer(Node&, NodePointer&);

    // https://dom.spec.whatwg.org/#nodeiterator-reference
    NodePointer m_reference;

    // While traversal is ongoing, we keep track of the current node pointer.
    // This allows us to adjust it during traversal if calling the filter ends up removing the node from the DOM.
    Optional<NodePointer> m_traversal_pointer;

    // https://dom.spec.whatwg.org/#concept-traversal-whattoshow
    unsigned m_what_to_show { 0 };

    // https://dom.spec.whatwg.org/#concept-traversal-filter
    JS::GCPtr<DOM::NodeFilter> m_filter;

    // https://dom.spec.whatwg.org/#concept-traversal-active
    bool m_active { false };
};

}

namespace Web::Bindings {
inline JS::Object* wrap(JS::Realm&, Web::DOM::NodeIterator& object) { return &object; }
using NodeIteratorWrapper = Web::DOM::NodeIterator;
}
