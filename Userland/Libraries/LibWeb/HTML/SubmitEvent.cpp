/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/SubmitEvent.h>

namespace Web::HTML {

WebIDL::ExceptionOr<JS::NonnullGCPtr<SubmitEvent>> SubmitEvent::create(JS::Realm& realm, FlyString const& event_name, SubmitEventInit const& event_init)
{
    return MUST_OR_THROW_OOM(realm.heap().allocate<SubmitEvent>(realm, realm, event_name, event_init));
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<SubmitEvent>> SubmitEvent::construct_impl(JS::Realm& realm, FlyString const& event_name, SubmitEventInit const& event_init)
{
    return create(realm, event_name, event_init);
}

SubmitEvent::SubmitEvent(JS::Realm& realm, FlyString const& event_name, SubmitEventInit const& event_init)
    : DOM::Event(realm, event_name.to_deprecated_fly_string(), event_init)
    , m_submitter(event_init.submitter)
{
}

SubmitEvent::~SubmitEvent() = default;

JS::ThrowCompletionOr<void> SubmitEvent::initialize(JS::Realm& realm)
{
    MUST_OR_THROW_OOM(Base::initialize(realm));
    set_prototype(&Bindings::ensure_web_prototype<Bindings::SubmitEventPrototype>(realm, "SubmitEvent"));

    return {};
}

void SubmitEvent::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_submitter.ptr());
}

}
