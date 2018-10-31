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

#include "platform/platform_file_utilities.h"

namespace Platform {
namespace File {

inline void UnsafeOpenEmailLink(const QString &email) {
	return ::File::internal::UnsafeOpenEmailLinkDefault(email);
}

inline void PostprocessDownloaded(const QString &filepath) {}

} // namespace File

namespace FileDialog {

inline void InitLastPath() {
	::FileDialog::internal::InitLastPathDefault();
}

inline bool Get(QStringList &files, QByteArray &remoteContent, const QString &caption, const QString &filter,
                ::FileDialog::internal::Type type, QString startFile) {
	return ::FileDialog::internal::GetDefault(files, remoteContent, caption, filter, type, startFile);
}

} // namespace FileDialog
} // namespace Platform
