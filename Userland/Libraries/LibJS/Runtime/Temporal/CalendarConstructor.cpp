/*
 * Copyright (c) 2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Temporal/Calendar.h>
#include <LibJS/Runtime/Temporal/CalendarConstructor.h>

namespace JS::Temporal {

// 12.2 The Temporal.Calendar Constructor, https://tc39.es/proposal-temporal/#sec-temporal-calendar-constructor
CalendarConstructor::CalendarConstructor(GlobalObject& global_object)
    : NativeFunction(vm().names.Calendar.as_string(), *global_object.function_prototype())
{
}

void CalendarConstructor::initialize(GlobalObject& global_object)
{
    NativeFunction::initialize(global_object);

    auto& vm = this->vm();

    // 12.3.1 Temporal.Calendar.prototype, https://tc39.es/proposal-temporal/#sec-temporal-calendar-prototype
    define_direct_property(vm.names.prototype, global_object.temporal_calendar_prototype(), 0);

    define_direct_property(vm.names.length, Value(1), Attribute::Configurable);
}

// 12.2.1 Temporal.Calendar ( id ), https://tc39.es/proposal-temporal/#sec-temporal.calendar
Value CalendarConstructor::call()
{
    auto& vm = this->vm();

    // 1. If NewTarget is undefined, then
    // a. Throw a TypeError exception.
    vm.throw_exception<TypeError>(global_object(), ErrorType::ConstructorWithoutNew, "Temporal.Calendar");
    return {};
}

// 12.2.1 Temporal.Calendar ( id ), https://tc39.es/proposal-temporal/#sec-temporal.calendar
Value CalendarConstructor::construct(FunctionObject& new_target)
{
    auto& vm = this->vm();
    auto& global_object = this->global_object();

    // 2. Set id to ? ToString(id).
    auto identifier = vm.argument(0).to_string(global_object);
    if (vm.exception())
        return {};

    // 3. If ! IsBuiltinCalendar(id) is false, then
    if (!is_builtin_calendar(identifier)) {
        // a. Throw a RangeError exception.
        vm.throw_exception<RangeError>(global_object, ErrorType::TemporalInvalidCalendarIdentifier, identifier);
        return {};
    }

    // 4. Return ? CreateTemporalCalendar(id, NewTarget).
    return create_temporal_calendar(global_object, identifier, &new_target);
}

}
