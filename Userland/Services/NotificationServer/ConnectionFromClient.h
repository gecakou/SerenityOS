/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibIPC/ConnectionFromClient.h>
#include <NotificationServer/NotificationClientEndpoint.h>
#include <NotificationServer/NotificationServerEndpoint.h>
#include <WindowServer/ScreenLayout.h>

namespace NotificationServer {

class ConnectionFromClient final : public IPC::ConnectionFromClient<NotificationClientEndpoint, NotificationServerEndpoint> {
    C_OBJECT(ConnectionFromClient)
public:
    ~ConnectionFromClient() override = default;

    virtual void die() override;

private:
    explicit ConnectionFromClient(NonnullOwnPtr<Core::LocalSocket>, int client_id);

    virtual void show_notification(DeprecatedString const&, DeprecatedString const&, Gfx::ShareableBitmap const&) override;
    virtual void close_notification() override;
    virtual Messages::NotificationServer::UpdateNotificationIconResponse update_notification_icon(Gfx::ShareableBitmap const&) override;
    virtual Messages::NotificationServer::UpdateNotificationTextResponse update_notification_text(DeprecatedString const&, DeprecatedString const&) override;
    virtual Messages::NotificationServer::IsShowingResponse is_showing() override;
};

}
