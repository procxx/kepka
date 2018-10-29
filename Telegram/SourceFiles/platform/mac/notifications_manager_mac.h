//
// This file is part of Kepka,
// an unofficial desktop version of Telegram messaging app,
// see https://github.com/procxx/kepka
//
// Kepka is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// It is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// In addition, as a special exception, the copyright holders give permission
// to link the code of portions of this program with the OpenSSL library.
//
// Full license: https://github.com/procxx/kepka/blob/master/LICENSE
// Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
// Copyright (c) 2017- Kepka Contributors, https://github.com/procxx
//
#pragma once

#include "base/weak_unique_ptr.h"
#include "platform/platform_notifications_manager.h"

namespace Platform {
namespace Notifications {

bool SkipAudio();
bool SkipToast();

class Manager : public Window::Notifications::NativeManager, public base::enable_weak_from_this {
public:
	Manager(Window::Notifications::System *system);
	~Manager();

protected:
	void doShowNativeNotification(PeerData *peer, MsgId msgId, const QString &title, const QString &subtitle,
	                              const QString &msg, bool hideNameAndPhoto, bool hideReplyButton) override;
	void doClearAllFast() override;
	void doClearFromHistory(History *history) override;

private:
	class Private;
	const std::unique_ptr<Private> _private;
};

} // namespace Notifications
} // namespace Platform
