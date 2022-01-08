/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/ArgsParser.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
#ifdef __serenity__
    TRY(Core::System::pledge("stdio settime"));
#endif

    Core::ArgsParser args_parser;
    double delta = __builtin_nan("");
    args_parser.add_option(delta, "Adjust system time by this many seconds", "set", 's', "delta_seconds");
    args_parser.parse(arguments);

    if (!__builtin_isnan(delta)) {
        long delta_us = static_cast<long>(round(delta * 1'000'000));
        timeval delta_timeval;
        delta_timeval.tv_sec = delta_us / 1'000'000;
        delta_timeval.tv_usec = delta_us % 1'000'000;
        if (delta_timeval.tv_usec < 0) {
            delta_timeval.tv_sec--;
            delta_timeval.tv_usec += 1'000'000;
        }
        if (adjtime(&delta_timeval, nullptr) < 0) {
            perror("adjtime set");
            return 1;
        }
    }

#ifdef __serenity__
    TRY(Core::System::pledge("stdio"));
#endif

    timeval remaining_delta_timeval;
    if (adjtime(nullptr, &remaining_delta_timeval) < 0) {
        perror("adjtime get");
        return 1;
    }
    double remaining_delta = remaining_delta_timeval.tv_sec + remaining_delta_timeval.tv_usec / 1'000'000.0;
    outln("{}", remaining_delta);

    return 0;
}
