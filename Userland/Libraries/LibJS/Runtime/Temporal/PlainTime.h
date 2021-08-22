/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 * Copyright (c) 2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <LibJS/Runtime/Object.h>

namespace JS::Temporal {

class PlainTime final : public Object {
    JS_OBJECT(PlainDateTime, Object);

public:
    PlainTime(u8 iso_hour, u8 iso_minute, u8 iso_second, u16 iso_millisecond, u16 iso_microsecond, u16 iso_nanosecond, Calendar& calendar, Object& prototype);
    virtual ~PlainTime() override = default;

    [[nodiscard]] u8 iso_hour() const { return m_iso_hour; }
    [[nodiscard]] u8 iso_minute() const { return m_iso_minute; }
    [[nodiscard]] u8 iso_second() const { return m_iso_second; }
    [[nodiscard]] u16 iso_millisecond() const { return m_iso_millisecond; }
    [[nodiscard]] u16 iso_microsecond() const { return m_iso_microsecond; }
    [[nodiscard]] u16 iso_nanosecond() const { return m_iso_nanosecond; }
    [[nodiscard]] Calendar const& calendar() const { return m_calendar; }
    [[nodiscard]] Calendar& calendar() { return m_calendar; }

private:
    virtual void visit_edges(Visitor&) override;

    // 4.4 Properties of Temporal.PlainTime Instances, https://tc39.es/proposal-temporal/#sec-properties-of-temporal-plaintime-instances
    u8 m_iso_hour { 0 };         // [[ISOHour]]
    u8 m_iso_minute { 0 };       // [[ISOMinute]]
    u8 m_iso_second { 0 };       // [[ISOSecond]]
    u16 m_iso_millisecond { 0 }; // [[ISOMillisecond]]
    u16 m_iso_microsecond { 0 }; // [[ISOMicrosecond]]
    u16 m_iso_nanosecond { 0 };  // [[ISONanosecond]]
    Calendar& m_calendar;        // [[Calendar]] (always the built-in ISO 8601 calendar)
};

struct DaysAndTime {
    i32 days;
    u8 hour;
    u8 minute;
    u8 second;
    u16 millisecond;
    u16 microsecond;
    u16 nanosecond;
};

struct TemporalTime {
    double hour;
    double minute;
    double second;
    double millisecond;
    double microsecond;
    double nanosecond;
};

// Table 3: Properties of a TemporalTimeLike, https://tc39.es/proposal-temporal/#table-temporal-temporaltimelike-properties

template<typename StructT, typename ValueT>
struct TemporalTimeLikeProperty {
    ValueT StructT::*internal_slot { nullptr };
    PropertyName property;
};

template<typename StructT, typename ValueT>
auto temporal_time_like_properties = [](VM& vm) {
    using PropertyT = TemporalTimeLikeProperty<StructT, ValueT>;
    return AK::Array {
        PropertyT { &StructT::hour, vm.names.hour },
        PropertyT { &StructT::microsecond, vm.names.microsecond },
        PropertyT { &StructT::millisecond, vm.names.millisecond },
        PropertyT { &StructT::minute, vm.names.minute },
        PropertyT { &StructT::nanosecond, vm.names.nanosecond },
        PropertyT { &StructT::second, vm.names.second },
    };
};

Optional<TemporalTime> regulate_time(GlobalObject&, double hour, double minute, double second, double millisecond, double microsecond, double nanosecond, StringView overflow);
bool is_valid_time(double hour, double minute, double second, double millisecond, double microsecond, double nanosecond);
DaysAndTime balance_time(i64 hour, i64 minute, i64 second, i64 millisecond, i64 microsecond, i64 nanosecond);
TemporalTime constrain_time(double hour, double minute, double second, double millisecond, double microsecond, double nanosecond);
PlainTime* create_temporal_time(GlobalObject&, u8 hour, u8 minute, u8 second, u16 millisecond, u16 microsecond, u16 nanosecond, FunctionObject* new_target = nullptr);
Optional<TemporalTime> to_temporal_time_record(GlobalObject&, Object& temporal_time_like);

}
