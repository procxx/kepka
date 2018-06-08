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
#include "platform/linux/linux_libs.h"

#include "logs.h"
#include "platform/linux/linux_desktop_environment.h"
#include "platform/linux/linux_gdk_helper.h"
#include "platform/linux/linux_libnotify.h"
#include "settings.h"

namespace Platform {
namespace Libs {
namespace {

bool loadLibrary(QLibrary &lib, const char *name, int version) {
	DEBUG_LOG(("Loading '%1' with version %2...").arg(QLatin1String(name)).arg(version));
	lib.setFileNameAndVersion(QLatin1String(name), version);
	if (lib.load()) {
		DEBUG_LOG(("Loaded '%1' with version %2!").arg(QLatin1String(name)).arg(version));
		return true;
	}
	lib.setFileNameAndVersion(QLatin1String(name), QString());
	if (lib.load()) {
		DEBUG_LOG(("Loaded '%1' without version!").arg(QLatin1String(name)));
		return true;
	}
	LOG(("Could not load '%1' with version %2 :(").arg(QLatin1String(name)).arg(version));
	return false;
}


} // namespace


void start() {
	DEBUG_LOG(("Loading libraries"));
}

} // namespace Libs
} // namespace Platform
