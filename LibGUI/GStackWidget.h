#pragma once

#include <LibGUI/GWidget.h>

class GStackWidget : public GWidget {
public:
    explicit GStackWidget(GWidget* parent);
    virtual ~GStackWidget() override;

    GWidget* active_widget() const { return m_active_widget; }
    void set_active_widget(GWidget*);

protected:
    virtual void child_event(GChildEvent&) override;
    virtual void resize_event(GResizeEvent&) override;

private:
    virtual const char* class_name() const override { return "GStackWidget"; }

    GWidget* m_active_widget { nullptr };
};
