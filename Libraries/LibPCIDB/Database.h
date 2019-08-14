#pragma once

#include <AK/HashMap.h>
#include <AK/MappedFile.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/RefCounted.h>
#include <AK/RefPtr.h>
#include <AK/StringView.h>

namespace PCIDB {

struct Subsystem {
    u16 vendor_id;
    u16 device_id;
    StringView name;
};

struct Device {
    u16 id;
    StringView name;
    HashMap<int, NonnullOwnPtr<Subsystem>> subsystems;
};

struct Vendor {
    u16 id;
    StringView name;
    HashMap<int, NonnullOwnPtr<Device>> devices;
};

struct ProgrammingInterface {
    u8 id { 0 };
    StringView name {};
};

struct Subclass {
    u8 id { 0 };
    StringView name {};
    HashMap<int, NonnullOwnPtr<ProgrammingInterface>> programming_interfaces;
};

struct Class {
    u8 id { 0 };
    StringView name {};
    HashMap<int, NonnullOwnPtr<Subclass>> subclasses;
};

class Database : public RefCounted<Database> {
public:
    static RefPtr<Database> open(const StringView& file_name);
    static RefPtr<Database> open() { return open("/res/pci.ids"); };

    const StringView get_vendor(u16 vendor_id) const;
    const StringView get_device(u16 vendor_id, u16 device_id) const;
    const StringView get_subsystem(u16 vendor_id, u16 device_id, u16 subvendor_id, u16 subdevice_id) const;
    const StringView get_class(u8 class_id) const;
    const StringView get_subclass(u8 class_id, u8 subclass_id) const;
    const StringView get_programming_interface(u8 class_id, u8 subclass_id, u8 programming_interface_id) const;

private:
    Database(const StringView& file_name)
        : m_file(file_name) {};

    int init();

    enum ParseMode {
        UnknownMode,
        VendorMode,
        ClassMode,
    };

    MappedFile m_file {};
    StringView m_view {};
    HashMap<int, NonnullOwnPtr<Vendor>> m_vendors;
    HashMap<int, NonnullOwnPtr<Class>> m_classes;
    bool m_ready { false };
};

}
