/*
 * Copyright (c) 2018-2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeWeakable.h>
#else
#    include <AK/Assertions.h>
#    include <AK/Atomic.h>
#    include <AK/RefCounted.h>
#    include <AK/RefPtr.h>
#    include <AK/StdLibExtras.h>
#    include <sched.h>

namespace AK {

template<typename T>
class Weakable;
template<typename T>
class WeakPtr;

class WeakLink : public RefCounted<WeakLink> {
    template<typename T>
    friend class Weakable;
    template<typename T>
    friend class WeakPtr;

public:
    template<typename T>
    RefPtr<T> strong_ref() const
        requires(IsBaseOf<RefCountedBase, T>)
    {
        RefPtr<T> ref;

        {
            if (!(m_consumers.fetch_add(1u << 1, AK::MemoryOrder::memory_order_acquire) & 1u)) {
                T* ptr = (T*)m_ptr.load(AK::MemoryOrder::memory_order_acquire);
                if (ptr && ptr->try_ref())
                    ref = adopt_ref(*ptr);
            }
            m_consumers.fetch_sub(1u << 1, AK::MemoryOrder::memory_order_release);
        }

        return ref;
    }

    template<typename T>
    T* unsafe_ptr() const
    {
        if (m_consumers.load(AK::MemoryOrder::memory_order_relaxed) & 1u)
            return nullptr;
        // NOTE: This may return a non-null pointer even if revocation
        // has been triggered as there is a possible race! But it's "unsafe"
        // anyway because we return a raw pointer without ensuring a
        // reference...
        return (T*)m_ptr.load(AK::MemoryOrder::memory_order_acquire);
    }

    bool is_null() const
    {
        return unsafe_ptr<void>() == nullptr;
    }

    void revoke()
    {
        auto current_consumers = m_consumers.fetch_or(1u, AK::MemoryOrder::memory_order_relaxed);
        VERIFY(!(current_consumers & 1u));
        // We flagged revocation, now wait until everyone trying to obtain
        // a strong reference is done
        while (current_consumers > 0) {
            sched_yield();
            current_consumers = m_consumers.load(AK::MemoryOrder::memory_order_acquire) & ~1u;
        }
        // No one is trying to use it (anymore)
        m_ptr.store(nullptr, AK::MemoryOrder::memory_order_release);
    }

private:
    template<typename T>
    explicit WeakLink(T& weakable)
        : m_ptr(&weakable)
    {
    }
    mutable Atomic<void*> m_ptr;
    mutable Atomic<unsigned> m_consumers; // LSB indicates revocation in progress
};

template<typename T>
class Weakable {
private:
    class Link;

public:
    template<typename U = T>
    WeakPtr<U> make_weak_ptr() const
    {
        return MUST(try_make_weak_ptr<U>());
    }

    template<typename U = T>
    ErrorOr<WeakPtr<U>> try_make_weak_ptr() const;

protected:
    Weakable() = default;

    ~Weakable()
    {
        revoke_weak_ptrs();
    }

    void revoke_weak_ptrs()
    {
        if (auto link = move(m_link))
            link->revoke();
    }

private:
    mutable RefPtr<WeakLink> m_link;
};

}

using AK::Weakable;

#endif
