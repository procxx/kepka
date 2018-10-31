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

#include "window/notifications_manager.h"

namespace Platform {
namespace Notifications {

bool SkipAudio();
bool SkipToast();

bool Supported();
std::unique_ptr<Window::Notifications::Manager> Create(Window::Notifications::System *system);
void FlashBounce();

} // namespace Notifications
} // namespace Platform

// Platform dependent implementations.

#ifdef Q_OS_MAC
#include "platform/mac/notifications_manager_mac.h"
#elif defined Q_OS_LINUX // Q_OS_MAC
#include "platform/linux/notifications_manager_linux.h"
#elif defined Q_OS_WIN // Q_OS_MAC || Q_OS_LINUX
#include "platform/win/notifications_manager_win.h"
#endif // Q_OS_MAC || Q_OS_LINUX || Q_OS_WIN
