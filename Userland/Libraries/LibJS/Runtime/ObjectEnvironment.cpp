/*
 * Copyright (c) 2020-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/AST.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/ObjectEnvironment.h>

namespace JS {

ObjectEnvironment::ObjectEnvironment(Object& binding_object, IsWithEnvironment is_with_environment, Environment* outer_environment)
    : Environment(outer_environment)
    , m_binding_object(binding_object)
    , m_with_environment(is_with_environment == IsWithEnvironment::Yes)
{
}

void ObjectEnvironment::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(&m_binding_object);
}

// 9.1.1.2.1 HasBinding ( N ), https://tc39.es/ecma262/#sec-object-environment-records-hasbinding-n
ThrowCompletionOr<bool> ObjectEnvironment::has_binding(FlyString const& name, Optional<size_t>*) const
{
    auto& vm = this->vm();

    // 1. Let bindingObject be envRec.[[BindingObject]].

    // 2. Let foundBinding be ? HasProperty(bindingObject, N).
    bool found_binding = TRY(m_binding_object.has_property(name));

    // 3. If foundBinding is false, return false.
    if (!found_binding)
        return false;

    // 4. If envRec.[[IsWithEnvironment]] is false, return true.
    if (!m_with_environment)
        return true;

    // 5. Let unscopables be ? Get(bindingObject, @@unscopables).
    auto unscopables = TRY(m_binding_object.get(*vm.well_known_symbol_unscopables()));

    // 6. If Type(unscopables) is Object, then
    if (unscopables.is_object()) {
        // a. Let blocked be ! ToBoolean(? Get(unscopables, N)).
        auto blocked = TRY(unscopables.as_object().get(name)).to_boolean();

        // b. If blocked is true, return false.
        if (blocked)
            return false;
    }

    // 7. Return true.
    return true;
}

// 9.1.1.2.2 CreateMutableBinding ( N, D ), https://tc39.es/ecma262/#sec-object-environment-records-createmutablebinding-n-d
ThrowCompletionOr<void> ObjectEnvironment::create_mutable_binding(GlobalObject&, FlyString const& name, bool can_be_deleted)
{
    // 1. Let bindingObject be envRec.[[BindingObject]].
    // 2. Return ? DefinePropertyOrThrow(bindingObject, N, PropertyDescriptor { [[Value]]: undefined, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: D }).
    TRY(m_binding_object.define_property_or_throw(name, { .value = js_undefined(), .writable = true, .enumerable = true, .configurable = can_be_deleted }));
    return {};
}

// 9.1.1.2.3 CreateImmutableBinding ( N, S ), https://tc39.es/ecma262/#sec-object-environment-records-createimmutablebinding-n-s
ThrowCompletionOr<void> ObjectEnvironment::create_immutable_binding(GlobalObject&, FlyString const&, bool)
{
    // "The CreateImmutableBinding concrete method of an object Environment Record is never used within this specification."
    VERIFY_NOT_REACHED();
}

// 9.1.1.2.4 InitializeBinding ( N, V ), https://tc39.es/ecma262/#sec-object-environment-records-initializebinding-n-v
void ObjectEnvironment::initialize_binding(GlobalObject& global_object, FlyString const& name, Value value)
{
    set_mutable_binding(global_object, name, value, false);
}

// 9.1.1.2.5 SetMutableBinding ( N, V, S ), https://tc39.es/ecma262/#sec-object-environment-records-setmutablebinding-n-v-s
void ObjectEnvironment::set_mutable_binding(GlobalObject& global_object, FlyString const& name, Value value, bool strict)
{
    auto& vm = this->vm();
    auto still_exists_or_error = m_binding_object.has_property(name);
    if (still_exists_or_error.is_error())
        return;
    auto still_exists = still_exists_or_error.release_value();
    if (!still_exists && strict) {
        global_object.vm().throw_exception<ReferenceError>(global_object, ErrorType::UnknownIdentifier, name);
        return;
    }

    auto result_or_error = m_binding_object.set(name, value, strict ? Object::ShouldThrowExceptions::Yes : Object::ShouldThrowExceptions::No);

    // Note: Nothing like this in the spec, this is here to produce nicer errors instead of the generic one thrown by Object::set().
    if (result_or_error.is_error() && strict) {
        auto property_or_error = m_binding_object.internal_get_own_property(name);
        if (property_or_error.is_error())
            return;
        auto property = property_or_error.release_value();
        if (property.has_value() && !property->writable.value_or(true)) {
            vm.clear_exception();
            vm.throw_exception<TypeError>(global_object, ErrorType::DescWriteNonWritable, name);
        }
    }
}

// 9.1.1.2.6 GetBindingValue ( N, S ), https://tc39.es/ecma262/#sec-object-environment-records-getbindingvalue-n-s
Value ObjectEnvironment::get_binding_value(GlobalObject& global_object, FlyString const& name, bool strict)
{
    auto& vm = this->vm();
    auto value = TRY_OR_DISCARD(m_binding_object.has_property(name));
    if (!value) {
        if (!strict)
            return js_undefined();

        vm.throw_exception<ReferenceError>(global_object, ErrorType::UnknownIdentifier, name);
        return {};
    }
    return TRY_OR_DISCARD(m_binding_object.get(name));
}

// 9.1.1.2.7 DeleteBinding ( N ), https://tc39.es/ecma262/#sec-object-environment-records-deletebinding-n
bool ObjectEnvironment::delete_binding(GlobalObject&, FlyString const& name)
{
    return TRY_OR_DISCARD(m_binding_object.internal_delete(name));
}

}
