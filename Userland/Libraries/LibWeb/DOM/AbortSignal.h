/*
 * Copyright (c) 2021, Luke Wilde <lukew@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/RefCounted.h>
#include <AK/Weakable.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/DOM/Window.h>
#include <LibWeb/Forward.h>

namespace Web::DOM {

// https://dom.spec.whatwg.org/#abortsignal
class AbortSignal final
    : public RefCounted<AbortSignal>
    , public Weakable<AbortSignal>
    , public EventTarget
    , public Bindings::Wrappable {
public:
    using WrapperType = Bindings::AbortSignalWrapper;

    using RefCounted::ref;
    using RefCounted::unref;

    static NonnullRefPtr<AbortSignal> create()
    {
        return adopt_ref(*new AbortSignal());
    }

    static NonnullRefPtr<AbortSignal> create_with_global_object(Bindings::WindowObject&)
    {
        return AbortSignal::create();
    }

    static NonnullRefPtr<AbortSignal> abort();

    virtual ~AbortSignal() override;

    void add_abort_algorithm(Function<void()>);

    // https://dom.spec.whatwg.org/#dom-abortsignal-aborted
    bool aborted() const { return m_aborted; }

    void signal_abort();

    void set_onabort(Optional<Bindings::CallbackType>);
    Bindings::CallbackType* onabort();

    // ^EventTarget
    virtual void ref_event_target() override { ref(); }
    virtual void unref_event_target() override { unref(); }
    virtual JS::Object* create_wrapper(JS::GlobalObject&) override;

private:
    AbortSignal();

    // https://dom.spec.whatwg.org/#abortsignal-aborted-flag
    bool m_aborted { false };

    // https://dom.spec.whatwg.org/#abortsignal-abort-algorithms
    // FIXME: This should be a set.
    Vector<Function<void()>> m_abort_algorithms;
};

}
