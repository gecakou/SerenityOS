/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Function.h>
#include <LibJS/AST.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ErrorPrototype.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Value.h>

namespace JS {

ErrorPrototype::ErrorPrototype(GlobalObject& global_object)
    : Object(*global_object.object_prototype())
{
}

void ErrorPrototype::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    Object::initialize(global_object);
    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_direct_property(vm.names.name, js_string(vm, "Error"), attr);
    define_direct_property(vm.names.message, js_string(vm, ""), attr);
    define_native_function(vm.names.toString, to_string, 0, attr);
    // Non standard property "stack"
    // Every other engine seems to have this in some way or another, and the spec
    // proposal for this is only Stage 1
    define_native_accessor(vm.names.stack, stack, nullptr, attr);
}

// 20.5.3.4 Error.prototype.toString ( ), https://tc39.es/ecma262/#sec-error.prototype.tostring
JS_DEFINE_NATIVE_FUNCTION(ErrorPrototype::to_string)
{
    auto this_value = vm.this_value(global_object);
    if (!this_value.is_object())
        return vm.throw_completion<TypeError>(global_object, ErrorType::NotAnObject, this_value.to_string_without_side_effects());
    auto& this_object = this_value.as_object();

    String name = "Error";
    auto name_property = TRY(this_object.get(vm.names.name));
    if (!name_property.is_undefined())
        name = TRY(name_property.to_string(global_object));

    String message = "";
    auto message_property = TRY(this_object.get(vm.names.message));
    if (!message_property.is_undefined())
        message = TRY(message_property.to_string(global_object));

    if (name.is_empty())
        return js_string(vm, message);
    if (message.is_empty())
        return js_string(vm, name);
    return js_string(vm, String::formatted("{}: {}", name, message));
}

JS_DEFINE_NATIVE_FUNCTION(ErrorPrototype::stack)
{
    auto this_value = vm.this_value(global_object);
    if (!this_value.is_object())
        return vm.throw_completion<TypeError>(global_object, ErrorType::NotAnObject, this_value.to_string_without_side_effects());
    auto& this_object = this_value.as_object();

    if (!is<Error>(this_object))
        return vm.throw_completion<TypeError>(global_object, ErrorType::NotAnObjectOfType, "Error");

    String name = "Error";
    auto name_property = TRY(this_object.get(vm.names.name));
    if (!name_property.is_undefined())
        name = TRY(name_property.to_string(global_object));

    String message = "";
    auto message_property = TRY(this_object.get(vm.names.message));
    if (!message_property.is_undefined())
        message = TRY(message_property.to_string(global_object));

    String header = name;
    if (!message.is_empty())
        header = String::formatted("{}: {}", name, message);

    return js_string(vm,
        String::formatted("{}\n{}", header, static_cast<Error&>(this_object).stack_string()));
}

#define __JS_ENUMERATE(ClassName, snake_name, PrototypeName, ConstructorName, ArrayType) \
    PrototypeName::PrototypeName(GlobalObject& global_object)                            \
        : Object(*global_object.error_prototype())                                       \
    {                                                                                    \
    }                                                                                    \
                                                                                         \
    void PrototypeName::initialize(GlobalObject& global_object)                          \
    {                                                                                    \
        auto& vm = this->vm();                                                           \
        Object::initialize(global_object);                                               \
        u8 attr = Attribute::Writable | Attribute::Configurable;                         \
        define_direct_property(vm.names.name, js_string(vm, #ClassName), attr);          \
        define_direct_property(vm.names.message, js_string(vm, ""), attr);               \
    }

JS_ENUMERATE_NATIVE_ERRORS
#undef __JS_ENUMERATE

}
