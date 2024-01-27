/*
 * Copyright (c) 2020, Matthew Olsson <matthewcolsson@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGfx/Bitmap.h>
#include <LibWeb/SVG/AttributeParser.h>
#include <LibWeb/SVG/SVGGraphicsElement.h>
#include <LibWeb/SVG/SVGViewport.h>
#include <LibWeb/SVG/ViewBox.h>

namespace Web::SVG {

class SVGSVGElement final : public SVGGraphicsElement
    , public SVGViewport {
    WEB_PLATFORM_OBJECT(SVGSVGElement, SVGGraphicsElement);
    JS_DECLARE_ALLOCATOR(SVGSVGElement);

public:
    virtual JS::GCPtr<Layout::Node> create_layout_node(NonnullRefPtr<CSS::StyleProperties>) override;

    virtual void apply_presentational_hints(CSS::StyleProperties&) const override;

    virtual bool requires_svg_container() const override { return false; }
    virtual bool is_svg_container() const override { return true; }

    virtual Optional<ViewBox> view_box() const override;
    virtual Optional<PreserveAspectRatio> preserve_aspect_ratio() const override { return m_preserve_aspect_ratio; }

    void set_fallback_view_box_for_svg_as_image(Optional<ViewBox>);

    JS::NonnullGCPtr<SVGAnimatedRect> view_box_for_bindings() { return *m_view_box_for_bindings; }

private:
    SVGSVGElement(DOM::Document&, DOM::QualifiedName);

    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Visitor&) override;

    virtual bool is_svg_svg_element() const override { return true; }

    virtual void attribute_changed(FlyString const& name, Optional<String> const& value) override;

    void update_fallback_view_box_for_svg_as_image();

    Optional<ViewBox> m_view_box;
    Optional<PreserveAspectRatio> m_preserve_aspect_ratio;

    Optional<ViewBox> m_fallback_view_box_for_svg_as_image;

    JS::GCPtr<SVGAnimatedRect> m_view_box_for_bindings;
};

}

namespace Web::DOM {

template<>
inline bool Node::fast_is<SVG::SVGSVGElement>() const { return is_svg_svg_element(); }

}
