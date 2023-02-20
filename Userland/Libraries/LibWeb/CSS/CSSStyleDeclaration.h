/*
 * Copyright (c) 2018-2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/Vector.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/CSS/StyleValue.h>

namespace Web::CSS {

enum class Important {
    No,
    Yes,
};

struct StyleProperty {
    Important important { Important::No };
    CSS::PropertyID property_id;
    NonnullRefPtr<StyleValue const> value;
    DeprecatedString custom_name {};
};

class CSSStyleDeclaration : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(CSSStyleDeclaration, Bindings::PlatformObject);

public:
    virtual ~CSSStyleDeclaration() = default;

    virtual size_t length() const = 0;
    virtual DeprecatedString item(size_t index) const = 0;

    virtual Optional<StyleProperty> property(PropertyID) const = 0;

    virtual WebIDL::ExceptionOr<void> set_property(PropertyID, StringView css_text, StringView priority = ""sv) = 0;
    virtual WebIDL::ExceptionOr<DeprecatedString> remove_property(PropertyID) = 0;

    WebIDL::ExceptionOr<void> set_property(StringView property_name, StringView css_text, StringView priority);
    WebIDL::ExceptionOr<DeprecatedString> remove_property(StringView property_name);

    DeprecatedString get_property_value(StringView property) const;
    DeprecatedString get_property_priority(StringView property) const;

    DeprecatedString css_text() const;
    virtual WebIDL::ExceptionOr<void> set_css_text(StringView) = 0;

    virtual DeprecatedString serialized() const = 0;

    virtual JS::ThrowCompletionOr<bool> internal_has_property(JS::PropertyKey const& name) const override;
    virtual JS::ThrowCompletionOr<JS::Value> internal_get(JS::PropertyKey const&, JS::Value receiver) const override;
    virtual JS::ThrowCompletionOr<bool> internal_set(JS::PropertyKey const&, JS::Value value, JS::Value receiver) override;

protected:
    explicit CSSStyleDeclaration(JS::Realm&);
};

class PropertyOwningCSSStyleDeclaration : public CSSStyleDeclaration {
    WEB_PLATFORM_OBJECT(PropertyOwningCSSStyleDeclaration, CSSStyleDeclaration);
    friend class ElementInlineCSSStyleDeclaration;

public:
    static WebIDL::ExceptionOr<JS::NonnullGCPtr<PropertyOwningCSSStyleDeclaration>> create(JS::Realm&, Vector<StyleProperty>, HashMap<DeprecatedString, StyleProperty> custom_properties);

    virtual ~PropertyOwningCSSStyleDeclaration() override = default;

    virtual size_t length() const override;
    virtual DeprecatedString item(size_t index) const override;

    virtual Optional<StyleProperty> property(PropertyID) const override;

    virtual WebIDL::ExceptionOr<void> set_property(PropertyID, StringView css_text, StringView priority) override;
    virtual WebIDL::ExceptionOr<DeprecatedString> remove_property(PropertyID) override;

    Vector<StyleProperty> const& properties() const { return m_properties; }
    HashMap<DeprecatedString, StyleProperty> const& custom_properties() const { return m_custom_properties; }
    Optional<StyleProperty> custom_property(DeprecatedString const& custom_property_name) const { return m_custom_properties.get(custom_property_name); }
    size_t custom_property_count() const { return m_custom_properties.size(); }

    virtual DeprecatedString serialized() const final override;
    virtual WebIDL::ExceptionOr<void> set_css_text(StringView) override;

protected:
    PropertyOwningCSSStyleDeclaration(JS::Realm&, Vector<StyleProperty>, HashMap<DeprecatedString, StyleProperty>);

    virtual void update_style_attribute() { }

    void empty_the_declarations();
    void set_the_declarations(Vector<StyleProperty> properties, HashMap<DeprecatedString, StyleProperty> custom_properties);

private:
    bool set_a_css_declaration(PropertyID, NonnullRefPtr<StyleValue const>, Important);

    Vector<StyleProperty> m_properties;
    HashMap<DeprecatedString, StyleProperty> m_custom_properties;
};

class ElementInlineCSSStyleDeclaration final : public PropertyOwningCSSStyleDeclaration {
    WEB_PLATFORM_OBJECT(ElementInlineCSSStyleDeclaration, PropertyOwningCSSStyleDeclaration);

public:
    static WebIDL::ExceptionOr<JS::NonnullGCPtr<ElementInlineCSSStyleDeclaration>> create(DOM::Element&, Vector<StyleProperty> properties, HashMap<DeprecatedString, StyleProperty> custom_properties);

    virtual ~ElementInlineCSSStyleDeclaration() override = default;

    DOM::Element* element() { return m_element.ptr(); }
    const DOM::Element* element() const { return m_element.ptr(); }

    bool is_updating() const { return m_updating; }

    virtual WebIDL::ExceptionOr<void> set_css_text(StringView) override;

private:
    ElementInlineCSSStyleDeclaration(DOM::Element&, Vector<StyleProperty> properties, HashMap<DeprecatedString, StyleProperty> custom_properties);

    virtual void visit_edges(Cell::Visitor&) override;

    virtual void update_style_attribute() override;

    JS::GCPtr<DOM::Element> m_element;

    // https://drafts.csswg.org/cssom/#cssstyledeclaration-updating-flag
    bool m_updating { false };
};

}
