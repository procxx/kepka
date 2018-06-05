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

#include "platform/platform_file_utilities.h"

extern "C" {
#undef signals
#define signals public
} // extern "C"

namespace Platform {
namespace File {
namespace internal {

QByteArray EscapeShell(const QByteArray &content);

} // namespace internal

inline QString UrlToLocal(const QUrl &url) {
	return ::File::internal::UrlToLocalDefault(url);
}

inline void UnsafeOpenEmailLink(const QString &email) {
	return ::File::internal::UnsafeOpenEmailLinkDefault(email);
}

inline bool UnsafeShowOpenWithDropdown(const QString &filepath, QPoint menuPosition) {
	return false;
}

inline bool UnsafeShowOpenWith(const QString &filepath) {
	return false;
}

inline void UnsafeLaunch(const QString &filepath) {
	return ::File::internal::UnsafeLaunchDefault(filepath);
}

inline void PostprocessDownloaded(const QString &filepath) {}

} // namespace File

namespace FileDialog {

inline void InitLastPath() {
	::FileDialog::internal::InitLastPathDefault();
}

} // namespace FileDialog
} // namespace Platform
