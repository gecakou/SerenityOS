/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2020, Linus Groh <mail@linusgroh.de>
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

#include <AK/Function.h>
#include <LibJS/Heap/Heap.h>
#include <LibJS/Interpreter.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/ArrayConstructor.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Shape.h>

namespace JS {

ArrayConstructor::ArrayConstructor()
    : NativeFunction("Array", *interpreter().global_object().function_prototype())
{
    define_property("prototype", interpreter().global_object().array_prototype(), 0);
    define_property("length", Value(1), Attribute::Configurable);

    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function("isArray", is_array, 1, attr);
    define_native_function("of", of, 0, attr);
}

ArrayConstructor::~ArrayConstructor()
{
}

Value ArrayConstructor::call(Interpreter& interpreter)
{
    if (interpreter.argument_count() <= 0)
        return Array::create(interpreter.global_object());

    if (interpreter.argument_count() == 1 && interpreter.argument(0).is_number()) {
        auto array_length_value = interpreter.argument(0);
        if (!array_length_value.is_integer() || array_length_value.as_i32() < 0) {
            interpreter.throw_exception<TypeError>("Invalid array length");
            return {};
        }
        auto* array = Array::create(interpreter.global_object());
        array->elements().resize(array_length_value.as_i32());
        return array;
    }

    auto* array = Array::create(interpreter.global_object());
    for (size_t i = 0; i < interpreter.argument_count(); ++i)
        array->elements().append(interpreter.argument(i));
    return array;
}

Value ArrayConstructor::construct(Interpreter& interpreter)
{
    return call(interpreter);
}

Value ArrayConstructor::is_array(Interpreter& interpreter)
{
    auto value = interpreter.argument(0);
    if (!value.is_array())
        return Value(false);
    // Exclude TypedArray and similar
    return Value(StringView(value.as_object().class_name()) == "Array");
}

Value ArrayConstructor::of(Interpreter& interpreter)
{
    auto* array = Array::create(interpreter.global_object());
    for (size_t i = 0; i < interpreter.argument_count(); ++i)
        array->elements().append(interpreter.argument(i));
    return array;
}

}
