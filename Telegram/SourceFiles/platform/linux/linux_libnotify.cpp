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
#include "platform/linux/linux_libnotify.h"

#include "logs.h"

#include "platform/linux/linux_libs.h"

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
} // namespace Libs
} // namespace Platform
