#pragma once

#include <AK/AKString.h>
#include <AK/Function.h>
#include <LibGUI/GVariant.h>

class GWidget;

class VBProperty {
public:
    VBProperty(const String& name, const GVariant& value);
    VBProperty(const String& name, Function<GVariant(const GWidget&)>&& getter, Function<void(GWidget&, const GVariant&)>&& setter);
    ~VBProperty();

    String name() const { return m_name; }
    const GVariant& value() const { return m_value; }
    void set_value(const GVariant& value) { m_value = value; }

    bool is_readonly() const { return m_readonly; }
    void set_readonly(bool b) { m_readonly = b; }

    void sync();

private:
    String m_name;
    GVariant m_value;
    Function<GVariant(const GWidget&)> m_getter;
    Function<void(GWidget&, const GVariant&)> m_setter;
    bool m_readonly { false };
};
