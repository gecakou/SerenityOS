/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/CircularQueue.h>
#include <AK/Types.h>
#include <Kernel/API/KeyCode.h>
#include <Kernel/Bus/SerialIO/PS2/Controller.h>
#include <Kernel/Bus/SerialIO/PS2/Device.h>
#include <Kernel/Devices/HID/KeyboardDevice.h>
#include <Kernel/Random.h>

namespace Kernel {

class PS2KeyboardDevice final : public PS2Device {
    friend class DeviceManagement;

public:
    static ErrorOr<NonnullOwnPtr<PS2KeyboardDevice>> try_to_initialize(PS2Controller const&, PS2PortIndex port_index, KeyboardDevice const&);
    virtual ~PS2KeyboardDevice() override;
    ErrorOr<void> initialize();

    // ^PS2Device
    virtual void handle_byte_read_from_serial_input(u8 byte) override;

private:
    PS2KeyboardDevice(PS2Controller const&, PS2PortIndex port_index, KeyboardDevice const&);

    bool m_has_e0_prefix { false };

    NonnullRefPtr<KeyboardDevice> const m_keyboard_device;
};

}
