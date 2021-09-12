/*
 * Copyright (c) 2020, Itamar S. <itamar8910@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "../LanguageClient.h"
#include <DevTools/HackStudio/LanguageServers/LanguageClientEndpoint.h>
#include <DevTools/HackStudio/LanguageServers/LanguageServerEndpoint.h>
#include <LibIPC/ServerConnection.h>

#define LANGUAGE_CLIENT(language_name_, socket_name)                                           \
    namespace language_name_ {                                                                 \
    class ServerConnection final : public HackStudio::ServerConnection {                       \
        C_OBJECT(ServerConnection)                                                             \
    public:                                                                                    \
        static const char* language_name() { return #language_name_; }                         \
                                                                                               \
    private:                                                                                   \
        ServerConnection(const String& project_path)                                           \
            : HackStudio::ServerConnection("/tmp/portal/language/" #socket_name, project_path) \
        {                                                                                      \
        }                                                                                      \
    };                                                                                         \
    }

namespace LanguageClients {

LANGUAGE_CLIENT(Cpp, cpp)
LANGUAGE_CLIENT(Shell, shell)

}

#undef LANGUAGE_CLIENT
