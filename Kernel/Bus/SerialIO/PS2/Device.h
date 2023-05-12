/*
 * Copyright (c) 2023, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DistinctNumeric.h>
#include <AK/Types.h>
#include <Kernel/Bus/SerialIO/PS2/Controller.h>
#include <Kernel/Bus/SerialIO/PS2/Definitions.h>

namespace Kernel {

class PS2Device {
public:
    virtual ~PS2Device() = default;

    virtual void handle_byte_read_from_serial_input(u8 byte) = 0;

    PS2PortIndex attached_port_index() const { return m_attached_port_index; }

protected:
    PS2Device(PS2Controller const& ps2_controller, PS2PortIndex attached_port_index)
        : m_ps2_controller(ps2_controller)
        , m_attached_port_index(attached_port_index)
    {
    }

    NonnullRefPtr<PS2Controller> const m_ps2_controller;
    PS2PortIndex m_attached_port_index { 0 };
};

}
