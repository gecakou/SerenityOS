#pragma once

#include <LibGUI/GDialog.h>

class GButton;
class GTextEditor;

class GInputBox : public GDialog {
    C_OBJECT(GInputBox)
public:
    explicit GInputBox(const StringView& prompt, const StringView& title, CObject* parent = nullptr);
    virtual ~GInputBox() override;

    String text_value() const { return m_text_value; }

private:
    void build();
    String m_prompt;
    String m_text_value;

    ObjectPtr<GButton> m_ok_button;
    ObjectPtr<GButton> m_cancel_button;
    ObjectPtr<GTextEditor> m_text_editor;
};
