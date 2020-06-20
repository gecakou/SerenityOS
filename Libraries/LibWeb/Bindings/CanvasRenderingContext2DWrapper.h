/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
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

#include <LibWeb/Bindings/Wrapper.h>

namespace Web {
namespace Bindings {

class CanvasRenderingContext2DWrapper final : public Wrapper {
public:
    CanvasRenderingContext2DWrapper(JS::GlobalObject&, CanvasRenderingContext2D&);
    virtual void initialize(JS::Interpreter&, JS::GlobalObject&) override;
    virtual ~CanvasRenderingContext2DWrapper() override;

    CanvasRenderingContext2D& impl() { return m_impl; }
    const CanvasRenderingContext2D& impl() const { return m_impl; }

private:
    virtual const char* class_name() const override { return "CanvasRenderingContext2DWrapper"; }

    JS_DECLARE_NATIVE_FUNCTION(fill_rect);
    JS_DECLARE_NATIVE_FUNCTION(stroke_rect);
    JS_DECLARE_NATIVE_FUNCTION(draw_image);
    JS_DECLARE_NATIVE_FUNCTION(scale);
    JS_DECLARE_NATIVE_FUNCTION(translate);
    JS_DECLARE_NATIVE_FUNCTION(begin_path);
    JS_DECLARE_NATIVE_FUNCTION(close_path);
    JS_DECLARE_NATIVE_FUNCTION(stroke);
    JS_DECLARE_NATIVE_FUNCTION(fill);
    JS_DECLARE_NATIVE_FUNCTION(move_to);
    JS_DECLARE_NATIVE_FUNCTION(line_to);
    JS_DECLARE_NATIVE_FUNCTION(quadratic_curve_to);
    JS_DECLARE_NATIVE_FUNCTION(create_image_data);
    JS_DECLARE_NATIVE_FUNCTION(put_image_data);

    JS_DECLARE_NATIVE_GETTER(fill_style_getter);
    JS_DECLARE_NATIVE_SETTER(fill_style_setter);

    JS_DECLARE_NATIVE_GETTER(stroke_style_getter);
    JS_DECLARE_NATIVE_SETTER(stroke_style_setter);

    JS_DECLARE_NATIVE_GETTER(line_width_getter);
    JS_DECLARE_NATIVE_SETTER(line_width_setter);

    JS_DECLARE_NATIVE_GETTER(canvas_getter);

    NonnullRefPtr<CanvasRenderingContext2D> m_impl;
};

CanvasRenderingContext2DWrapper* wrap(JS::Heap&, CanvasRenderingContext2D&);

}
}
