/*
 * Copyright (c) 2022, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Concepts.h>
#include <AK/DistinctNumeric.h>
#include <AK/Traits.h>
#include <LibGfx/Forward.h>
#include <math.h>

namespace Web {

/// DevicePixels: A position or length on the physical display.
AK_TYPEDEF_DISTINCT_NUMERIC_GENERAL(int, DevicePixels, Arithmetic, CastToUnderlying, Comparison, Increment);

template<Arithmetic T>
constexpr bool operator==(DevicePixels const& left, T const& right) { return left.value() == right; }

template<Arithmetic T>
constexpr bool operator!=(DevicePixels const& left, T const& right) { return left.value() != right; }

template<Arithmetic T>
constexpr bool operator>(DevicePixels const& left, T const& right) { return left.value() > right; }

template<Arithmetic T>
constexpr bool operator<(DevicePixels const& left, T const& right) { return left.value() < right; }

template<Arithmetic T>
constexpr bool operator>=(DevicePixels const& left, T const& right) { return left.value() >= right; }

template<Arithmetic T>
constexpr bool operator<=(DevicePixels const& left, T const& right) { return left.value() <= right; }

template<Arithmetic T>
constexpr DevicePixels operator*(DevicePixels const& left, T const& right) { return left.value() * right; }
template<Arithmetic T>
constexpr DevicePixels operator*(T const& left, DevicePixels const& right) { return right * left; }

template<Arithmetic T>
constexpr DevicePixels operator/(DevicePixels const& left, T const& right) { return left.value() / right; }

template<Arithmetic T>
constexpr DevicePixels operator%(DevicePixels const& left, T const& right) { return left.value() % right; }

/// CSSPixels: A position or length in CSS "reference pixels", independent of zoom or screen DPI.
/// See https://www.w3.org/TR/css-values-3/#reference-pixel
AK_TYPEDEF_DISTINCT_NUMERIC_GENERAL(float, CSSPixels, Arithmetic, CastToUnderlying, Comparison, Increment);

template<Arithmetic T>
constexpr bool operator==(CSSPixels const& left, T const& right) { return left.value() == right; }

template<Arithmetic T>
constexpr bool operator!=(CSSPixels const& left, T const& right) { return left.value() != right; }

template<Arithmetic T>
constexpr bool operator>(CSSPixels const& left, T const& right) { return left.value() > right; }

template<Arithmetic T>
constexpr bool operator<(CSSPixels const& left, T const& right) { return left.value() < right; }

template<Arithmetic T>
constexpr bool operator>=(CSSPixels const& left, T const& right) { return left.value() >= right; }

template<Arithmetic T>
constexpr bool operator<=(CSSPixels const& left, T const& right) { return left.value() <= right; }

template<Arithmetic T>
constexpr CSSPixels operator*(CSSPixels const& left, T const& right) { return left.value() * right; }
template<Arithmetic T>
constexpr CSSPixels operator*(T const& left, CSSPixels const& right) { return right * left; }

template<Arithmetic T>
constexpr CSSPixels operator/(CSSPixels const& left, T const& right) { return left.value() / right; }

template<Arithmetic T>
constexpr CSSPixels operator%(CSSPixels const& left, T const& right) { return left.value() % right; }

using CSSPixelLine = Gfx::Line<CSSPixels>;
using CSSPixelPoint = Gfx::Point<CSSPixels>;
using CSSPixelRect = Gfx::Rect<CSSPixels>;
using CSSPixelSize = Gfx::Size<CSSPixels>;

using DevicePixelLine = Gfx::Line<DevicePixels>;
using DevicePixelPoint = Gfx::Point<DevicePixels>;
using DevicePixelRect = Gfx::Rect<DevicePixels>;
using DevicePixelSize = Gfx::Size<DevicePixels>;

}

constexpr Web::CSSPixels floor(Web::CSSPixels const& value)
{
    return ::floorf(value.value());
}

constexpr Web::CSSPixels ceil(Web::CSSPixels const& value)
{
    return ::ceilf(value.value());
}

constexpr Web::CSSPixels round(Web::CSSPixels const& value)
{
    return ::roundf(value.value());
}

constexpr Web::CSSPixels fmod(Web::CSSPixels const& x, Web::CSSPixels const& y)
{
    return ::fmodf(x.value(), y.value());
}

constexpr Web::CSSPixels abs(Web::CSSPixels const& value)
{
    return AK::abs(value.value());
}

constexpr Web::DevicePixels abs(Web::DevicePixels const& value)
{
    return AK::abs(value.value());
}

namespace AK {

template<>
struct Traits<Web::CSSPixels> : public GenericTraits<Web::CSSPixels> {
    static unsigned hash(Web::CSSPixels const& key)
    {
        return double_hash(key.value());
    }

    static bool equals(Web::CSSPixels const& a, Web::CSSPixels const& b)
    {
        return a == b;
    }
};

template<>
struct Traits<Web::DevicePixels> : public GenericTraits<Web::DevicePixels> {
    static unsigned hash(Web::DevicePixels const& key)
    {
        return double_hash(key.value());
    }

    static bool equals(Web::DevicePixels const& a, Web::DevicePixels const& b)
    {
        return a == b;
    }
};

template<>
struct Formatter<Web::CSSPixels> : Formatter<float> {
    ErrorOr<void> format(FormatBuilder& builder, Web::CSSPixels const& value)
    {
        return Formatter<float>::format(builder, value.value());
    }
};

template<>
struct Formatter<Web::DevicePixels> : Formatter<float> {
    ErrorOr<void> format(FormatBuilder& builder, Web::DevicePixels const& value)
    {
        return Formatter<float>::format(builder, value.value());
    }
};

}
