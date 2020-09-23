/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <AK/LogStream.h>
#include <AK/NonnullRefPtr.h>
#include <AK/StdLibExtras.h>
#include <AK/Traits.h>
#include <AK/Types.h>

namespace AK {

template<typename T>
class OwnPtr;

template<typename T>
struct RefPtrTraits {
    static T* as_ptr(FlatPtr bits)
    {
        return (T*)bits;
    }

    static FlatPtr as_bits(T* ptr)
    {
        return (FlatPtr)ptr;
    }

    static bool is_null(FlatPtr bits)
    {
        return !bits;
    }

    static constexpr FlatPtr default_null_value = 0;

    typedef std::nullptr_t NullType;
};

template<typename T, typename PtrTraits>
class RefPtr {
    template<typename U, typename P>
    friend class RefPtr;
public:
    enum AdoptTag {
        Adopt
    };

    RefPtr() { }
    RefPtr(const T* ptr)
        : m_bits(PtrTraits::as_bits(const_cast<T*>(ptr)))
    {
        ref_if_not_null(const_cast<T*>(ptr));
    }
    RefPtr(const T& object)
        : m_bits(PtrTraits::as_bits(const_cast<T*>(&object)))
    {
        T* ptr = const_cast<T*>(&object);
        ASSERT(ptr);
        ASSERT(!ptr == PtrTraits::is_null(m_bits));
        ptr->ref();
    }
    RefPtr(AdoptTag, T& object)
        : m_bits(PtrTraits::as_bits(&object))
    {
        ASSERT(&object);
        ASSERT(!PtrTraits::is_null(m_bits));
    }
    RefPtr(RefPtr&& other)
        : m_bits(other.leak_ref_raw())
    {
    }
    ALWAYS_INLINE RefPtr(const NonnullRefPtr<T>& other)
        : m_bits(PtrTraits::as_bits(const_cast<T*>(other.ptr())))
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        PtrTraits::as_ptr(m_bits)->ref();
    }
    template<typename U>
    ALWAYS_INLINE RefPtr(const NonnullRefPtr<U>& other)
        : m_bits(PtrTraits::as_bits(const_cast<U*>(other.ptr())))
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        PtrTraits::as_ptr(m_bits)->ref();
    }
    template<typename U>
    ALWAYS_INLINE RefPtr(NonnullRefPtr<U>&& other)
        : m_bits(PtrTraits::as_bits(&other.leak_ref()))
    {
        ASSERT(!PtrTraits::is_null(m_bits));
    }
    template<typename U, typename P = RefPtrTraits<U>>
    RefPtr(RefPtr<U, P>&& other)
        : m_bits(other.leak_ref_raw())
    {
    }
    RefPtr(const RefPtr& other)
        : m_bits(PtrTraits::as_bits(const_cast<T*>(other.ptr())))
    {
        ref_if_not_null(const_cast<T*>(other.ptr()));
    }
    template<typename U, typename P = RefPtrTraits<U>>
    RefPtr(const RefPtr<U, P>& other)
        : m_bits(PtrTraits::as_bits(const_cast<U*>(other.ptr())))
    {
        ref_if_not_null(const_cast<U*>(other.ptr()));
    }
    ALWAYS_INLINE ~RefPtr()
    {
        clear();
#ifdef SANITIZE_PTRS
        if constexpr (sizeof(T*) == 8)
            m_bits = 0xe0e0e0e0e0e0e0e0;
        else
            m_bits = 0xe0e0e0e0;
#endif
    }
    RefPtr(std::nullptr_t) { }

    template<typename U>
    RefPtr(const OwnPtr<U>&) = delete;
    template<typename U>
    RefPtr& operator=(const OwnPtr<U>&) = delete;

    template<typename U>
    void swap(RefPtr<U, PtrTraits>& other)
    {
        ::swap(m_bits, other.m_bits);
    }

    ALWAYS_INLINE RefPtr& operator=(RefPtr&& other)
    {
        RefPtr tmp = move(other);
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(RefPtr<U, PtrTraits>&& other)
    {
        RefPtr tmp = move(other);
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(NonnullRefPtr<U>&& other)
    {
        RefPtr tmp = move(other);
        swap(tmp);
        ASSERT(!PtrTraits::is_null(m_bits));
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(const NonnullRefPtr<T>& other)
    {
        RefPtr tmp = other;
        swap(tmp);
        ASSERT(!PtrTraits::is_null(m_bits));
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(const NonnullRefPtr<U>& other)
    {
        RefPtr tmp = other;
        swap(tmp);
        ASSERT(!PtrTraits::is_null(m_bits));
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(const RefPtr& other)
    {
        RefPtr tmp = other;
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(const RefPtr<U>& other)
    {
        RefPtr tmp = other;
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(const T* ptr)
    {
        RefPtr tmp = ptr;
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(const T& object)
    {
        RefPtr tmp = object;
        swap(tmp);
        return *this;
    }

    RefPtr& operator=(std::nullptr_t)
    {
        clear();
        return *this;
    }

    ALWAYS_INLINE void clear()
    {
        unref_if_not_null(PtrTraits::as_ptr(m_bits));
        m_bits = PtrTraits::default_null_value;
    }

    bool operator!() const { return PtrTraits::is_null(m_bits); }

    [[nodiscard]] T* leak_ref()
    {
        FlatPtr bits = exchange(m_bits, PtrTraits::default_null_value);
        return !PtrTraits::is_null(bits) ? PtrTraits::as_ptr(bits) : nullptr;
    }

    NonnullRefPtr<T> release_nonnull()
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        return NonnullRefPtr<T>(NonnullRefPtr<T>::Adopt, *leak_ref());
    }

    ALWAYS_INLINE T* ptr() { return !PtrTraits::is_null(m_bits) ? PtrTraits::as_ptr(m_bits) : nullptr; }
    ALWAYS_INLINE const T* ptr() const { return !PtrTraits::is_null(m_bits) ? PtrTraits::as_ptr(m_bits) : nullptr; }

    ALWAYS_INLINE T* operator->()
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        return PtrTraits::as_ptr(m_bits);
    }

    ALWAYS_INLINE const T* operator->() const
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        return PtrTraits::as_ptr(m_bits);
    }

    ALWAYS_INLINE T& operator*()
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        return *PtrTraits::as_ptr(m_bits);
    }

    ALWAYS_INLINE const T& operator*() const
    {
        ASSERT(!PtrTraits::is_null(m_bits));
        return *PtrTraits::as_ptr(m_bits);
    }

    ALWAYS_INLINE operator const T*() const { return PtrTraits::as_ptr(m_bits); }
    ALWAYS_INLINE operator T*() { return PtrTraits::as_ptr(m_bits); }

    operator bool() { return !PtrTraits::is_null(m_bits); }

    bool operator==(std::nullptr_t) const { return PtrTraits::is_null(m_bits); }
    bool operator!=(std::nullptr_t) const { return !PtrTraits::is_null(m_bits); }

    bool operator==(const RefPtr& other) const { return m_bits == other.m_bits; }
    bool operator!=(const RefPtr& other) const { return m_bits != other.m_bits; }

    bool operator==(RefPtr& other) { return m_bits == other.m_bits; }
    bool operator!=(RefPtr& other) { return m_bits != other.m_bits; }

    bool operator==(const T* other) const { return PtrTraits::as_ptr(m_bits) == other; }
    bool operator!=(const T* other) const { return PtrTraits::as_ptr(m_bits) != other; }

    bool operator==(T* other) { return PtrTraits::as_ptr(m_bits) == other; }
    bool operator!=(T* other) { return PtrTraits::as_ptr(m_bits) != other; }

    bool is_null() const { return PtrTraits::is_null(m_bits); }
    
    template<typename U = T, typename EnableIf<IsSame<U, T>::value && !IsNullPointer<typename PtrTraits::NullType>::value>::Type* = nullptr>
    typename PtrTraits::NullType null_value() const
    {
        // make sure we are holding a null value
        ASSERT(PtrTraits::is_null(m_bits));
        return PtrTraits::to_null_value(m_bits);
    }
    template<typename U = T, typename EnableIf<IsSame<U, T>::value && !IsNullPointer<typename PtrTraits::NullType>::value>::Type* = nullptr>
    void set_null_value(typename PtrTraits::NullType value)
    {
         // make sure that new null value would be interpreted as a null value
         FlatPtr bits = PtrTraits::from_null_value(value);
         ASSERT(PtrTraits::is_null(bits));
         clear();
         m_bits = bits;
    }

private:
    [[nodiscard]] FlatPtr leak_ref_raw()
    {
        return exchange(m_bits, PtrTraits::default_null_value);
    }

    FlatPtr m_bits { PtrTraits::default_null_value };
};

template<typename T, typename PtrTraits = RefPtrTraits<T>>
inline const LogStream& operator<<(const LogStream& stream, const RefPtr<T, PtrTraits>& value)
{
    return stream << value.ptr();
}

template<typename T>
struct Traits<RefPtr<T>> : public GenericTraits<RefPtr<T>> {
    using PeekType = const T*;
    static unsigned hash(const RefPtr<T>& p) { return ptr_hash(p.ptr()); }
    static bool equals(const RefPtr<T>& a, const RefPtr<T>& b) { return a.ptr() == b.ptr(); }
};

template<typename T, typename U>
inline NonnullRefPtr<T> static_ptr_cast(const NonnullRefPtr<U>& ptr)
{
    return NonnullRefPtr<T>(static_cast<const T&>(*ptr));
}

template<typename T, typename U, typename PtrTraits = RefPtrTraits<T>>
inline RefPtr<T> static_ptr_cast(const RefPtr<U>& ptr)
{
    return RefPtr<T, PtrTraits>(static_cast<const T*>(ptr.ptr()));
}

}

using AK::RefPtr;
using AK::static_ptr_cast;
