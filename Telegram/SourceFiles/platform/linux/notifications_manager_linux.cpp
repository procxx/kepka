/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#include "platform/linux/notifications_manager_linux.h"

#include "config.h"
#include "window/notifications_utilities.h"
#include "platform/linux/linux_libnotify.h"
#include "platform/linux/linux_libs.h"
#include "lang/lang_keys.h"
#include "base/task_queue.h"

namespace Platform {
namespace Notifications {

bool Supported() {
	return false;
}

std::unique_ptr<Window::Notifications::Manager> Create(Window::Notifications::System *system) {
	return nullptr;
}

void Finish() {
}

} // namespace Notifications
} // namespace Platform
