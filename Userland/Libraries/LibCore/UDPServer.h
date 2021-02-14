/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
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

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Forward.h>
#include <AK/Function.h>
#include <LibCore/Forward.h>
#include <LibCore/Object.h>
#include <LibCore/SocketAddress.h>

namespace Core {

class UDPServer : public Object {
    C_OBJECT(UDPServer)
public:
    virtual ~UDPServer() override;

    bool is_bound() const { return m_bound; }

    bool bind(const IPv4Address& address, u16 port);
    ByteBuffer receive(size_t size, sockaddr_in& from);
    ByteBuffer receive(size_t size)
    {
        struct sockaddr_in saddr;
        return receive(size, saddr);
    };

    Optional<IPv4Address> local_address() const;
    Optional<u16> local_port() const;

    int fd() const { return m_fd; }

    Function<void()> on_ready_to_receive;

protected:
    explicit UDPServer(Object* parent = nullptr);

private:
    int m_fd { -1 };
    bool m_bound { false };
    RefPtr<Notifier> m_notifier;
};

}
