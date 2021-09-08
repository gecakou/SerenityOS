/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 * Copyright (c) 2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Variant.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Temporal/AbstractOperations.h>

namespace JS::Temporal {

class PlainDateTime final : public Object {
    JS_OBJECT(PlainDateTime, Object);

public:
    PlainDateTime(i32 iso_year, u8 iso_month, u8 iso_day, u8 iso_hour, u8 iso_minute, u8 iso_second, u16 iso_millisecond, u16 iso_microsecond, u16 iso_nanosecond, Object& calendar, Object& prototype);
    virtual ~PlainDateTime() override = default;

    [[nodiscard]] i32 iso_year() const { return m_iso_year; }
    [[nodiscard]] u8 iso_month() const { return m_iso_month; }
    [[nodiscard]] u8 iso_day() const { return m_iso_day; }
    [[nodiscard]] u8 iso_hour() const { return m_iso_hour; }
    [[nodiscard]] u8 iso_minute() const { return m_iso_minute; }
    [[nodiscard]] u8 iso_second() const { return m_iso_second; }
    [[nodiscard]] u16 iso_millisecond() const { return m_iso_millisecond; }
    [[nodiscard]] u16 iso_microsecond() const { return m_iso_microsecond; }
    [[nodiscard]] u16 iso_nanosecond() const { return m_iso_nanosecond; }
    [[nodiscard]] Object const& calendar() const { return m_calendar; }
    [[nodiscard]] Object& calendar() { return m_calendar; }

private:
    virtual void visit_edges(Visitor&) override;

    // 5.4 Properties of Temporal.PlainDateTime Instances, https://tc39.es/proposal-temporal/#sec-properties-of-temporal-plaindatetime-instances
    i32 m_iso_year { 0 };        // [[ISOYear]]
    u8 m_iso_month { 0 };        // [[ISOMonth]]
    u8 m_iso_day { 0 };          // [[ISODay]]
    u8 m_iso_hour { 0 };         // [[ISOHour]]
    u8 m_iso_minute { 0 };       // [[ISOMinute]]
    u8 m_iso_second { 0 };       // [[ISOSecond]]
    u16 m_iso_millisecond { 0 }; // [[ISOMillisecond]]
    u16 m_iso_microsecond { 0 }; // [[ISOMicrosecond]]
    u16 m_iso_nanosecond { 0 };  // [[ISONanosecond]]
    Object& m_calendar;          // [[Calendar]]
};

BigInt* get_epoch_from_iso_parts(GlobalObject&, i32 year, u8 month, u8 day, u8 hour, u8 minute, u8 second, u16 millisecond, u16 microsecond, u16 nanosecond);
bool iso_date_time_within_limits(GlobalObject&, i32 year, u8 month, u8 day, u8 hour, u8 minute, u8 second, u16 millisecond, u16 microsecond, u16 nanosecond);
Optional<ISODateTime> interpret_temporal_date_time_fields(GlobalObject&, Object& calendar, Object& fields, Object& options);
PlainDateTime* to_temporal_date_time(GlobalObject&, Value item, Object* options = nullptr);
ISODateTime balance_iso_date_time(i32 year, u8 month, u8 day, u8 hour, u8 minute, u8 second, u16 millisecond, u16 microsecond, i64 nanosecond);
PlainDateTime* create_temporal_date_time(GlobalObject&, i32 iso_year, u8 iso_month, u8 iso_day, u8 hour, u8 minute, u8 second, u16 millisecond, u16 microsecond, u16 nanosecond, Object& calendar, FunctionObject* new_target = nullptr);
Optional<String> temporal_date_time_to_string(GlobalObject&, i32 iso_year, u8 iso_month, u8 iso_day, u8 hour, u8 minute, u8 second, u16 millisecond, u16 microsecond, u16 nanosecond, Value calendar, Variant<StringView, u8> const& precision, StringView show_calendar);
i8 compare_iso_date_time(i32 year1, u8 month1, u8 day1, u8 hour1, u8 minute1, u8 second1, u16 millisecond1, u16 microsecond1, u16 nanosecond1, i32 year2, u8 month2, u8 day2, u8 hour2, u8 minute2, u8 second2, u16 millisecond2, u16 microsecond2, u16 nanosecond2);

}
