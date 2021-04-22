/*
 * Copyright (c) 2020, The SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGemini/GeminiResponse.h>

namespace Gemini {

GeminiResponse::GeminiResponse(int status, String meta)
    : m_status(status)
    , m_meta(meta)
{
}

GeminiResponse::~GeminiResponse()
{
}

}
