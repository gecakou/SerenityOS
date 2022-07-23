/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/RefPtr.h>
#include <AK/WeakPtr.h>
#include <Kernel/Devices/BlockDevice.h>
#include <LibPartition/DiskPartitionMetadata.h>

namespace Kernel {

class DiskPartition final : public BlockDevice {
    friend class DeviceManagement;

public:
    static NonnullRefPtr<DiskPartition> create(BlockDevice&, MinorNumber, Partition::DiskPartitionMetadata);
    virtual ~DiskPartition();

    virtual void start_request(AsyncBlockDeviceRequest&) override;

    // ^BlockDevice
    virtual ErrorOr<size_t> read(OpenFileDescription&, u64, UserOrKernelBuffer&, size_t) override;
    virtual bool can_read(OpenFileDescription const&, u64) const override;
    virtual ErrorOr<size_t> write(OpenFileDescription&, u64, UserOrKernelBuffer const&, size_t) override;
    virtual bool can_write(OpenFileDescription const&, u64) const override;

    Partition::DiskPartitionMetadata const& metadata() const;

private:
    DiskPartition(BlockDevice&, MinorNumber, Partition::DiskPartitionMetadata);
    virtual StringView class_name() const override;

    virtual void after_inserting() override;
    virtual void will_be_destroyed() override;

    WeakPtr<BlockDevice> m_device;
    Partition::DiskPartitionMetadata m_metadata;
};

}
