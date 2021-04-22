/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <syscall.h>

extern "C" {

int prctl(int option, uintptr_t arg1, uintptr_t arg2)
{
    int rc = syscall(SC_prctl, option, arg1, arg2);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}
}
