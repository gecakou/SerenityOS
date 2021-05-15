/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/BlockBox.h>

namespace Web::Layout {

class TableRowGroupBox final : public BlockBox {
public:
    TableRowGroupBox(DOM::Document&, DOM::Element&, NonnullRefPtr<CSS::StyleProperties>);
    virtual ~TableRowGroupBox() override;

    size_t column_count() const;
};

}
