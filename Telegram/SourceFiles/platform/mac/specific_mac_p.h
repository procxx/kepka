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

#include "core/utils.h" // TimeMs
#include <QByteArray>
#include <QString>
#include <QWindow> // WId

// e is NSEvent*
bool objc_handleMediaKeyEvent(void *e);

bool objc_darkMode();
void objc_showOverAll(WId winId, bool canFocus = true);
void objc_bringToBack(WId winId);

void objc_debugShowAlert(const QString &str);
void objc_outputDebugString(const QString &str);
bool objc_idleSupported();
bool objc_idleTime(TimeMs &idleTime);

void objc_start();
void objc_ignoreApplicationActivationRightNow();
void objc_finish();
bool objc_execUpdater();
void objc_execTelegram(const QString &crashreport);

void objc_registerCustomScheme();

void objc_activateProgram(WId winId);
bool objc_moveFile(const QString &from, const QString &to);
void objc_deleteDir(const QString &dir);

double objc_appkitVersion();

QString objc_documentsPath();
QString objc_appDataPath();
QString objc_downloadPath();
QByteArray objc_downloadPathBookmark(const QString &path);
QByteArray objc_pathBookmark(const QString &path);
void objc_downloadPathEnableAccess(const QByteArray &bookmark);

class objc_FileBookmark {
public:
	objc_FileBookmark(const QByteArray &bookmark);
	bool valid() const;
	bool enable() const;
	void disable() const;

	const QString &name(const QString &original) const;
	QByteArray bookmark() const;

	~objc_FileBookmark();

private:
#ifdef OS_MAC_STORE
	class objc_FileBookmarkData;
	objc_FileBookmarkData *data = nullptr;
#endif // OS_MAC_STORE
};
