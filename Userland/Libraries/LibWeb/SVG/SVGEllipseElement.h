/*
 * Copyright (c) 2022, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/SVG/SVGAnimatedLength.h>
#include <LibWeb/SVG/SVGGeometryElement.h>

namespace Web::SVG {

class SVGEllipseElement final : public SVGGeometryElement {
    WEB_PLATFORM_OBJECT(SVGEllipseElement, SVGGraphicsElement);

public:
    virtual ~SVGEllipseElement() override = default;

    virtual void parse_attribute(FlyString const& name, String const& value) override;

    virtual Gfx::Path& get_path() override;

    JS::NonnullGCPtr<SVGAnimatedLength> cx() const;
    JS::NonnullGCPtr<SVGAnimatedLength> cy() const;
    JS::NonnullGCPtr<SVGAnimatedLength> rx() const;
    JS::NonnullGCPtr<SVGAnimatedLength> ry() const;

private:
    SVGEllipseElement(DOM::Document&, DOM::QualifiedName);

    Optional<Gfx::Path> m_path;

    Optional<float> m_center_x;
    Optional<float> m_center_y;
    Optional<float> m_radius_x;
    Optional<float> m_radius_y;
};

}

WRAPPER_HACK(SVGEllipseElement, Web::SVG)
