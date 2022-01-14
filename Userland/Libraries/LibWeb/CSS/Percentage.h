/*
 * Copyright (c) 2022, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <AK/Variant.h>
#include <LibWeb/CSS/Length.h>

namespace Web::CSS {

class Percentage {
public:
    explicit Percentage(int value)
        : m_value(value)
    {
    }

    explicit Percentage(float value)
        : m_value(value)
    {
    }

    float value() const { return m_value; }
    float as_fraction() const { return m_value * 0.01f; }

    String to_string() const
    {
        return String::formatted("{}%", m_value);
    }

    bool operator==(Percentage const& other) const { return m_value == other.m_value; }
    bool operator!=(Percentage const& other) const { return !(*this == other); }

private:
    float m_value;
};

template<typename T>
class PercentageOr {
public:
    PercentageOr(T t)
        : m_value(move(t))
    {
    }

    PercentageOr(Percentage percentage)
        : m_value(move(percentage))
    {
    }

    PercentageOr<T>& operator=(T t)
    {
        m_value = move(t);
        return *this;
    }

    PercentageOr<T>& operator=(Percentage percentage)
    {
        m_value = move(percentage);
        return *this;
    }

    bool is_percentage() const { return m_value.template has<Percentage>(); }

    Percentage const& percentage() const
    {
        VERIFY(is_percentage());
        return m_value.template get<Percentage>();
    }

    T resolved(T const& reference_value) const
    {
        if (is_percentage())
            return reference_value.percentage_of(m_value.template get<Percentage>());

        return m_value.template get<T>();
    }

    String to_string() const
    {
        if (is_percentage())
            return m_value.template get<Percentage>().to_string();

        return m_value.template get<T>().to_string();
    }

    bool operator==(PercentageOr<T> const& other) const
    {
        if (is_percentage() != other.is_percentage())
            return false;
        if (is_percentage())
            return (m_value.template get<Percentage>() == other.m_value.template get<Percentage>());
        return (m_value.template get<T>() == other.m_value.template get<T>());
    }
    bool operator!=(PercentageOr<T> const& other) const { return !(*this == other); }

protected:
    bool is_non_percentage_value() const { return m_value.template has<T>(); }
    T const& non_percentage_value() const { return m_value.template get<T>(); }

private:
    Variant<T, Percentage> m_value;
};

template<typename T>
bool operator==(PercentageOr<T> const& percentage_or, T const& t)
{
    return percentage_or == PercentageOr<T> { t };
}

template<typename T>
bool operator==(T const& t, PercentageOr<T> const& percentage_or)
{
    return t == percentage_or;
}

template<typename T>
bool operator==(PercentageOr<T> const& percentage_or, Percentage const& percentage)
{
    return percentage_or == PercentageOr<T> { percentage };
}

template<typename T>
bool operator==(Percentage const& percentage, PercentageOr<T> const& percentage_or)
{
    return percentage == percentage_or;
}

class LengthPercentage : public PercentageOr<Length> {
public:
    using PercentageOr<Length>::PercentageOr;

    bool is_length() const { return is_non_percentage_value(); }
    Length const& length() const { return non_percentage_value(); }
};

}
