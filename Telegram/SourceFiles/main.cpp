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
#include "application.h"
#include "platform/platform_specific.h"
#include "storage/localstorage.h"

int main(int argc, char *argv[]) {
#if !defined(Q_OS_MAC) && QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	// Retina display support is working fine, others are not.
	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling, true);
#endif // not defined Q_OS_MAC and QT_VERSION >= 5.6.0
	QCoreApplication::setApplicationName(qsl("TelegramDesktop"));

	InitFromCommandLine(argc, argv);
	if (cLaunchMode() == LaunchModeFixPrevious) {
		return psFixPrevious();
	} else if (cLaunchMode() == LaunchModeCleanup) {
		return psCleanup();
	}

	// both are finished in Application::closeApplication
	Logs::start(); // must be started before Platform is started
	Platform::start(); // must be started before QApplication is created
#if defined(Q_OS_LINUX64)
	QCoreApplication::addLibraryPath("/usr/lib64/qt5/plugins");
#else
	QCoreApplication::addLibraryPath("/usr/lib/qt5/plugins");
#endif
	qputenv("QT_STYLE_OVERRIDE", "qwerty");
	qunsetenv("QT_QPA_PLATFORMTHEME");

	int result = 0;
	{
		Application app(argc, argv);
		result = app.exec();
	}

	DEBUG_LOG(("Telegram finished, result: %1").arg(result));

	if (cRestarting()) {
		DEBUG_LOG(("Application Info: executing Telegram, because of restart..."));
		psExecTelegram();
	}

	SignalHandlers::finish();
	Platform::finish();
	Logs::finish();

	return result;
}
