/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FlyString.h>
#include <AK/Vector.h>

namespace Web::CSS {

class Selector {
public:
    struct SimpleSelector {
        enum class Type {
            Invalid,
            Universal,
            TagName,
            Id,
            Class,
        };
        Type type { Type::Invalid };

        enum class PseudoClass {
            None,
            Link,
            Visited,
            Hover,
            Focus,
            FirstChild,
            LastChild,
            OnlyChild,
            Empty,
            Root,
            FirstOfType,
            LastOfType,
        };
        PseudoClass pseudo_class { PseudoClass::None };

        enum class PseudoElement {
            None,
            Before,
            After,
        };
        PseudoElement pseudo_element { PseudoElement::None };

        FlyString value;

        enum class AttributeMatchType {
            None,
            HasAttribute,
            ExactValueMatch,
            Contains,
        };

        AttributeMatchType attribute_match_type { AttributeMatchType::None };
        FlyString attribute_name;
        String attribute_value;
    };

    struct ComplexSelector {
        enum class Relation {
            None,
            ImmediateChild,
            Descendant,
            AdjacentSibling,
            GeneralSibling,
        };
        Relation relation { Relation::None };

        using CompoundSelector = Vector<SimpleSelector>;
        CompoundSelector compound_selector;
    };

    explicit Selector(Vector<ComplexSelector>&&);
    ~Selector();

    const Vector<ComplexSelector>& complex_selectors() const { return m_complex_selectors; }

    u32 specificity() const;

private:
    Vector<ComplexSelector> m_complex_selectors;
};

}
