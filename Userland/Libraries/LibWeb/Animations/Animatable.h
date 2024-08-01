/*
 * Copyright (c) 2024, Matthew Olsson <mattco@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FlyString.h>
#include <LibWeb/Animations/KeyframeEffect.h>

namespace Web::Animations {

// https://www.w3.org/TR/web-animations-1/#dictdef-keyframeanimationoptions
struct KeyframeAnimationOptions : public KeyframeEffectOptions {
    FlyString id { ""_fly_string };
    Optional<JS::GCPtr<AnimationTimeline>> timeline;
};

// https://www.w3.org/TR/web-animations-1/#dictdef-getanimationsoptions
struct GetAnimationsOptions {
    bool subtree { false };
};

// https://www.w3.org/TR/web-animations-1/#animatable
class Animatable {
public:
    virtual ~Animatable() = default;

    WebIDL::ExceptionOr<JS::NonnullGCPtr<Animation>> animate(Optional<JS::Handle<JS::Object>> keyframes, Variant<Empty, double, KeyframeAnimationOptions> options = {});
    Vector<JS::NonnullGCPtr<Animation>> get_animations(GetAnimationsOptions options = {});

    void associate_with_animation(JS::NonnullGCPtr<Animation>);
    void disassociate_with_animation(JS::NonnullGCPtr<Animation>);

    JS::GCPtr<CSS::CSSStyleDeclaration const> cached_animation_name_source(Optional<CSS::Selector::PseudoElement::Type>) const;
    void set_cached_animation_name_source(JS::GCPtr<CSS::CSSStyleDeclaration const> value, Optional<CSS::Selector::PseudoElement::Type>);

    JS::GCPtr<Animations::Animation> cached_animation_name_animation(Optional<CSS::Selector::PseudoElement::Type>) const;
    void set_cached_animation_name_animation(JS::GCPtr<Animations::Animation> value, Optional<CSS::Selector::PseudoElement::Type>);

protected:
    void visit_edges(JS::Cell::Visitor&);

private:
    Vector<JS::NonnullGCPtr<Animation>> m_associated_animations;
    bool m_is_sorted_by_composite_order { true };

    Array<JS::GCPtr<CSS::CSSStyleDeclaration const>, to_underlying(CSS::Selector::PseudoElement::Type::KnownPseudoElementCount) + 1> m_cached_animation_name_source;
    Array<JS::GCPtr<Animations::Animation>, to_underlying(CSS::Selector::PseudoElement::Type::KnownPseudoElementCount) + 1> m_cached_animation_name_animation;
};

}
