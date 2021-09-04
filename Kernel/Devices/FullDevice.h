/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Devices/CharacterDevice.h>

namespace Kernel {

class FullDevice final : public CharacterDevice {
    AK_MAKE_ETERNAL
public:
    static NonnullRefPtr<FullDevice> must_create();
    virtual ~FullDevice() override;

    // ^Device
    virtual mode_t required_mode() const override { return 0666; }
    virtual String device_name() const override { return "full"; }

private:
    FullDevice();

    // ^CharacterDevice
    virtual KResultOr<size_t> read(FileDescription&, u64, UserOrKernelBuffer&, size_t) override;
    virtual KResultOr<size_t> write(FileDescription&, u64, UserOrKernelBuffer const&, size_t) override;
    virtual bool can_read(FileDescription const&, size_t) const override;
    virtual bool can_write(FileDescription const&, size_t) const override { return true; }
    virtual StringView class_name() const override { return "FullDevice"; }
};

}
