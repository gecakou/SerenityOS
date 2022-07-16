/*
 * Copyright (c) 2021, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Bus/PCI/API.h>
#include <Kernel/Bus/PCI/Access.h>
#include <Kernel/Bus/PCI/Definitions.h>
#include <Kernel/Debug.h>
#include <Kernel/FileSystem/SysFS/Registry.h>
#include <Kernel/FileSystem/SysFS/Subsystems/Bus/PCI/BusDirectory.h>
#include <Kernel/FileSystem/SysFS/Subsystems/Bus/PCI/DeviceDirectory.h>
#include <Kernel/Sections.h>

namespace Kernel {

UNMAP_AFTER_INIT void PCIBusSysFSDirectory::initialize()
{
    auto pci_directory = adopt_ref(*new (nothrow) PCIBusSysFSDirectory());
    pci_directory->enumerate_all_devices_and_add_pci_device_directories();
    SysFSComponentRegistry::the().register_new_bus_directory(pci_directory);
}

UNMAP_AFTER_INIT PCIBusSysFSDirectory::PCIBusSysFSDirectory()
    : SysFSDirectory(SysFSComponentRegistry::the().buses_directory())
{
}

UNMAP_AFTER_INIT void PCIBusSysFSDirectory::enumerate_all_devices_and_add_pci_device_directories()
{
    MUST(m_child_components.with([&](auto& list) -> ErrorOr<void> {
        MUST(PCI::enumerate_locked([&](PCI::DeviceIdentifier& device_identifier) {
            auto pci_device_directory = PCIDeviceSysFSDirectory::create(*this, device_identifier.address());
            list.append(pci_device_directory);
            device_identifier.set_sysfs_pci_device_directory({}, *pci_device_directory);
        }));
        return {};
    }));
}

}
