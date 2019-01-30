#pragma once

#include <AK/Badge.h>
#include <Kernel/CharacterDevice.h>
#include <Kernel/DoubleBuffer.h>

class SlavePTY;

class MasterPTY final : public CharacterDevice {
public:
    explicit MasterPTY(unsigned index);
    virtual ~MasterPTY() override;

    // ^CharacterDevice
    virtual ssize_t read(Process&, byte*, size_t) override;
    virtual ssize_t write(Process&, const byte*, size_t) override;
    virtual bool can_read(Process&) const override;
    virtual bool can_write(Process&) const override;
    virtual bool is_master_pty() const override { return true; }

    unsigned index() const { return m_index; }
    String pts_name() const;
    void on_slave_write(const byte*, size_t);
    bool can_write_from_slave() const;
    void notify_slave_closed(Badge<SlavePTY>);

private:
    // ^CharacterDevice
    virtual const char* class_name() const override { return "MasterPTY"; }

    RetainPtr<SlavePTY> m_slave;
    unsigned m_index;
    DoubleBuffer m_buffer;
};
