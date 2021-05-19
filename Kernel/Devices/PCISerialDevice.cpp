/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/PCISerialDevice.h>
#include <Kernel/UnixTypes.h>
#include <Kernel/kstdio.h>
#include <LibC/sys/ttydefaults.h>

namespace Kernel {

static SerialDevice* s_the = nullptr;

void PCISerialDevice::detect()
{
    size_t current_device_minor = 68;
    const auto default_baud = SerialDevice::serial_baud_from_termios(TTYDEF_SPEED).value();
    PCI::enumerate([&](const PCI::Address& address, PCI::ID id) {
        if (address.is_null())
            return;

        for (auto& board_definition : board_definitions) {
            if (board_definition.device_id != id)
                continue;

            auto maybe_new_baud = SerialDevice::termios_baud_from_serial(board_definition.baud_rate);
            if (!maybe_new_baud.has_value()) {
                dbgln("FIXME: PCISerialDevice's speed is missing from termios");
                break;
            }

            auto bar_base = PCI::get_BAR(address, board_definition.pci_bar) & ~1;
            auto port_base = IOAddress(bar_base + board_definition.first_offset);
            for (size_t i = 0; i < board_definition.port_count; i++) {
                auto serial_device = new SerialDevice(port_base.offset(board_definition.port_size * i), current_device_minor++, PCI::get_interrupt_line(address));
                if (board_definition.baud_rate != default_baud) {
                    auto termios = serial_device->get_termios();
                    termios.c_ispeed = termios.c_ospeed = maybe_new_baud.value();
                    serial_device->set_termios(termios);
                }

                // If this is the first port of the first pci serial device, store it as the debug PCI serial port (TODO: Make this configurable somehow?)
                if (!is_available() && get_serial_debug())
                    s_the = serial_device;
                // NOTE: We intentionally leak the reference to serial_device here, as it is eternal
            }

            dmesgln("PCISerialDevice: Found {} @ {}", board_definition.name, address);
            return;
        }
    });
}

SerialDevice& PCISerialDevice::the()
{
    VERIFY(s_the);
    return *s_the;
}

bool PCISerialDevice::is_available()
{
    return s_the;
}
}
