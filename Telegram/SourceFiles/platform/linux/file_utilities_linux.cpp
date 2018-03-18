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
#include "platform/linux/file_utilities_linux.h"

#include <private/qguiapplication_p.h>
#include "platform/linux/linux_libs.h"
#include "platform/linux/linux_gdk_helper.h"
#include "messenger.h"
#include "mainwindow.h"
#include "storage/localstorage.h"

QStringList qt_make_filter_list(const QString &filter);

namespace Platform {
namespace File {
namespace internal {

QByteArray EscapeShell(const QByteArray &content) {
	auto result = QByteArray();

	auto b = content.constData(), e = content.constEnd();
	for (auto ch = b; ch != e; ++ch) {
		if (*ch == ' ' || *ch == '"' || *ch == '\'' || *ch == '\\') {
			if (result.isEmpty()) {
				result.reserve(content.size() * 2);
			}
			if (ch > b) {
				result.append(b, ch - b);
			}
			result.append('\\');
			b = ch;
		}
	}
	if (result.isEmpty()) {
		return content;
	}

	if (e > b) {
		result.append(b, e - b);
	}
	return result;
}

} // namespace internal

void UnsafeShowInFolder(const QString &filepath) {
	Ui::hideLayer(true); // Hide mediaview to make other apps visible.

	auto absolutePath = QFileInfo(filepath).absoluteFilePath();
	QProcess process;
	process.start("xdg-mime", QStringList() << "query" << "default" << "inode/directory");
	process.waitForFinished();
	auto output = QString::fromLatin1(process.readLine().simplified());
	auto command = qsl("xdg-open");
	auto arguments = QStringList();
	if (output == qstr("dolphin.desktop") || output == qstr("org.kde.dolphin.desktop")) {
		command = qsl("dolphin");
		arguments << "--select" << absolutePath;
	} else if (output == qstr("nautilus.desktop") || output == qstr("org.gnome.Nautilus.desktop") || output == qstr("nautilus-folder-handler.desktop")) {
		command = qsl("nautilus");
		arguments << "--no-desktop" << absolutePath;
	} else if (output == qstr("nemo.desktop")) {
		command = qsl("nemo");
		arguments << "--no-desktop" << absolutePath;
	} else if (output == qstr("konqueror.desktop") || output == qstr("kfmclient_dir.desktop")) {
		command = qsl("konqueror");
		arguments << "--select" << absolutePath;
	} else {
		arguments << QFileInfo(filepath).absoluteDir().absolutePath();
	}
	if (!process.startDetached(command, arguments)) {
		LOG(("Failed to launch '%1 %2'").arg(command).arg(arguments.join(' ')));
	}
}

} // namespace File

namespace FileDialog {
namespace {

using Type = ::FileDialog::internal::Type;

} // namespace

bool Get(QStringList &files, QByteArray &remoteContent, const QString &caption, const QString &filter, Type type, QString startFile) {
	return ::FileDialog::internal::GetDefault(files, remoteContent, caption, filter, type, startFile);
}

namespace {

const char *filterRegExp =
"^(.*)\\(([a-zA-Z0-9_.,*? +;#\\-\\[\\]@\\{\\}/!<>\\$%&=^~:\\|]*)\\)$";

// Makes a list of filters from a normal filter string "Image Files (*.png *.jpg)"
QStringList cleanFilterList(const QString &filter) {
	QRegExp regexp(QString::fromLatin1(filterRegExp));
	Q_ASSERT(regexp.isValid());
	QString f = filter;
	int i = regexp.indexIn(f);
	if (i >= 0)
		f = regexp.cap(2);
	return f.split(QLatin1Char(' '), QString::SkipEmptyParts);
}

} // namespace
} // namespace FileDialog
} // namespace Platform
