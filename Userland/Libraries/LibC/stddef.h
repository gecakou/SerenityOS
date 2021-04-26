/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifndef KERNEL

#    include <sys/cdefs.h>

#    ifdef __cplusplus
#        define NULL nullptr
#    else
#        define NULL ((void*)0)
#    endif

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__ size_t;
typedef long double max_align_t;

#endif
