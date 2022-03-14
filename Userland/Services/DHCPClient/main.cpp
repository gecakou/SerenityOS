/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "DHCPv4Client.h"
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>

ErrorOr<int> serenity_main(Main::Arguments)
{
    using enum Kernel::Pledge;
    TRY((Core::System::Promise<stdio, unix, inet, cpath, rpath>::pledge()));
    Core::EventLoop event_loop;

    TRY(Core::System::unveil("/proc/net/", "r"));
    TRY(Core::System::unveil(nullptr, nullptr));

    auto client = TRY(DHCPv4Client::try_create());

    TRY((Core::System::Promise<stdio, inet, cpath, rpath>::pledge()));
    return event_loop.exec();
}
