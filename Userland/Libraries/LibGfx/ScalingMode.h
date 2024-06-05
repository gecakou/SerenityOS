/*
 * Copyright (c) 2024, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Gfx {

enum class ScalingMode {
    NearestNeighbor,
    SmoothPixels,
    BilinearBlend,
    BoxSampling,
    None,
};

}
