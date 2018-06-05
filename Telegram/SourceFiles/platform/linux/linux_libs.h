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
#pragma once

#include <QLibrary>

#include "logs.h"

namespace Platform {
namespace Libs {

void start();

template <typename Function> bool load(QLibrary &lib, const char *name, Function &func) {
	func = nullptr;
	if (!lib.isLoaded()) {
		return false;
	}

	func = reinterpret_cast<Function>(lib.resolve(name));
	if (func) {
		return true;
	}
	LOG(("Error: failed to load '%1' function!").arg(name));
	return false;
}


} // namespace Libs
} // namespace Platform
