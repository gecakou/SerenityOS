/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "XSV.h"
#include <AK/Forward.h>
#include <AK/StringView.h>

namespace Reader {

class CSV : public XSV {
public:
    CSV(StringView source, ParserBehaviour behaviours = default_behaviours())
        : XSV(source, { ",", "\"", ParserTraits::Repeat }, behaviours)
    {
    }
};

}
