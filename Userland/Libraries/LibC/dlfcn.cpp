/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/HashMap.h>
#include <AK/LexicalPath.h>
#include <AK/RefPtr.h>
#include <AK/ScopeGuard.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <LibELF/DynamicLoader.h>
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {

__attribute__((__weak__)) int dlclose(void*)
{
    dbgln("weak dlclose stub called\n");
    ASSERT_NOT_REACHED();
}

__attribute__((__weak__)) char* dlerror()
{
    dbgln("weak dlerror stub called\n");
    ASSERT_NOT_REACHED();
}

__attribute__((__weak__)) void* dlopen(const char*, int)
{
    dbgln("weak dlopen stub called\n");
    ASSERT_NOT_REACHED();
}

__attribute__((__weak__)) void* dlsym(void*, const char*)
{
    dbgln("weak dlsym stub called\n");
    ASSERT_NOT_REACHED();
}

} // extern "C"
