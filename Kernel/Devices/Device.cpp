/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Singleton.h>
#include <Kernel/Devices/Device.h>
#include <Kernel/Devices/DeviceManagement.h>
#include <Kernel/FileSystem/InodeMetadata.h>
#include <Kernel/FileSystem/SysFS.h>
#include <Kernel/FileSystem/SysFS/Subsystems/DeviceIdentifiers/BlockDevicesDirectory.h>
#include <Kernel/FileSystem/SysFS/Subsystems/DeviceIdentifiers/CharacterDevicesDirectory.h>
#include <Kernel/Sections.h>

namespace Kernel {

Device::Device(MajorNumber major, MinorNumber minor)
    : m_major(major)
    , m_minor(minor)
{
}

void Device::before_will_be_destroyed_remove_from_device_management()
{
    DeviceManagement::the().before_device_removal({}, *this);
    m_state = State::BeingRemoved;
}

void Device::after_inserting_add_to_device_management()
{
    DeviceManagement::the().after_inserting_device({}, *this);
}

void Device::after_inserting()
{
    after_inserting_add_to_device_management();
    VERIFY(!m_sysfs_component);
    auto sys_fs_component = SysFSDeviceComponent::must_create(*this);
    m_sysfs_component = sys_fs_component;
    after_inserting_add_to_device_identifier_directory();
}

void Device::will_be_destroyed()
{
    VERIFY(m_sysfs_component);
    before_will_be_destroyed_remove_from_device_identifier_directory();
    before_will_be_destroyed_remove_from_device_management();
}

NonnullRefPtr<SysFSComponent> Device::sysfs_device_identifier_component() const
{
    // Note: Verify that we are not using both pointers by accident.
    VERIFY(!(m_sysfs_component && m_symlink_sysfs_component));
    RefPtr<SysFSComponent> device_identifier_component;
    if (m_sysfs_component)
        device_identifier_component = m_sysfs_component;
    else if (m_symlink_sysfs_component)
        device_identifier_component = m_symlink_sysfs_component;
    return device_identifier_component.release_nonnull();
}

Device::~Device()
{
    VERIFY(m_state == State::BeingRemoved);
}

ErrorOr<NonnullOwnPtr<KString>> Device::pseudo_path(OpenFileDescription const&) const
{
    return KString::formatted("device:{},{}", major(), minor());
}

void Device::process_next_queued_request(Badge<AsyncDeviceRequest>, AsyncDeviceRequest const& completed_request)
{
    SpinlockLocker lock(m_requests_lock);
    VERIFY(!m_requests.is_empty());
    VERIFY(m_requests.first().ptr() == &completed_request);
    m_requests.remove(m_requests.begin());
    if (!m_requests.is_empty()) {
        auto* next_request = m_requests.first().ptr();
        next_request->do_start(move(lock));
    }

    evaluate_block_conditions();
}

}
