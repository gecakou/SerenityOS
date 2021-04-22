/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGUI/Widget.h>

namespace GUI {

class LazyWidget : public Widget {
    C_OBJECT(LazyWidget)
public:
    virtual ~LazyWidget() override;

    Function<void(LazyWidget&)> on_first_show;

protected:
    LazyWidget();

private:
    virtual void show_event(ShowEvent&) override;

    bool m_has_been_shown { false };
};
}
