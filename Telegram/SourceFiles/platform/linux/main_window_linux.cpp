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
#include "platform/linux/main_window_linux.h"

#include "app.h"
#include "application.h"
#include "facades.h"
#include "lang/lang_keys.h"
#include "mainwindow.h"
#include "messenger.h"
#include "platform/linux/linux_desktop_environment.h"
#include "platform/linux/linux_libs.h"
#include "platform/platform_notifications_manager.h"
#include "storage/localstorage.h"
#include "styles/style_window.h"

namespace Platform {
namespace {

bool noQtTrayIcon = false, tryAppIndicator = false;


qint32 _trayIconSize = 22;
bool _trayIconMuted = true;
qint32 _trayIconCount = 0;
QImage _trayIconImageBack, _trayIconImage;


#define QT_RED 0
#define QT_GREEN 1
#define QT_BLUE 2
#define QT_ALPHA 3


QImage _trayIconImageGen() {
	qint32 counter = App::histories().unreadBadge(),
	       counterSlice = (counter >= 1000) ? (1000 + (counter % 100)) : counter;
	bool muted = App::histories().unreadOnlyMuted();
	if (_trayIconImage.isNull() || _trayIconImage.width() != _trayIconSize || muted != _trayIconMuted ||
	    counterSlice != _trayIconCount) {
		if (_trayIconImageBack.isNull() || _trayIconImageBack.width() != _trayIconSize) {
			_trayIconImageBack = Messenger::Instance().logo().scaled(_trayIconSize, _trayIconSize,
			                                                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			_trayIconImageBack = _trayIconImageBack.convertToFormat(QImage::Format_ARGB32);
			int w = _trayIconImageBack.width(), h = _trayIconImageBack.height(),
			    perline = _trayIconImageBack.bytesPerLine();
			uchar *bytes = _trayIconImageBack.bits();
			for (qint32 y = 0; y < h; ++y) {
				for (qint32 x = 0; x < w; ++x) {
					qint32 srcoff = y * perline + x * 4;
					bytes[srcoff + QT_RED] = qMax(bytes[srcoff + QT_RED], uchar(224));
					bytes[srcoff + QT_GREEN] = qMax(bytes[srcoff + QT_GREEN], uchar(165));
					bytes[srcoff + QT_BLUE] = qMax(bytes[srcoff + QT_BLUE], uchar(44));
				}
			}
		}
		_trayIconImage = _trayIconImageBack;
		if (counter > 0) {
			QPainter p(&_trayIconImage);
			qint32 layerSize = -16;
			if (_trayIconSize >= 48) {
				layerSize = -32;
			} else if (_trayIconSize >= 36) {
				layerSize = -24;
			} else if (_trayIconSize >= 32) {
				layerSize = -20;
			}
			auto &bg = (muted ? st::trayCounterBgMute : st::trayCounterBg);
			auto &fg = st::trayCounterFg;
			auto layer = App::wnd()->iconWithCounter(layerSize, counter, bg, fg, false);
			p.drawImage(_trayIconImage.width() - layer.width() - 1, _trayIconImage.height() - layer.height() - 1,
			            layer);
		}
	}
	return _trayIconImage;
}

QString _trayIconImageFile() {
	qint32 counter = App::histories().unreadBadge(),
	       counterSlice = (counter >= 1000) ? (1000 + (counter % 100)) : counter;
	bool muted = App::histories().unreadOnlyMuted();

	QString name = cWorkingDir() +
	               qsl("tdata/ticons/ico%1_%2_%3.png").arg(muted ? "mute" : "").arg(_trayIconSize).arg(counterSlice);
	QFileInfo info(name);
	if (info.exists()) return name;

	QImage img = _trayIconImageGen();
	if (img.save(name, "PNG")) return name;

	QDir dir(info.absoluteDir());
	if (!dir.exists()) {
		dir.mkpath(dir.absolutePath());
		if (img.save(name, "PNG")) return name;
	}

	return QString();
}


} // namespace

MainWindow::MainWindow() {
	connect(&_psCheckStatusIconTimer, SIGNAL(timeout()), this, SLOT(psStatusIconCheck()));
	_psCheckStatusIconTimer.setSingleShot(false);

	connect(&_psUpdateIndicatorTimer, SIGNAL(timeout()), this, SLOT(psUpdateIndicator()));
	_psUpdateIndicatorTimer.setSingleShot(true);
}

bool MainWindow::hasTrayIcon() const {
	return trayIcon;
}

void MainWindow::psStatusIconCheck() {
	if (cSupportTray() || !--_psCheckStatusIconLeft) {
		_psCheckStatusIconTimer.stop();
		return;
	}
}

void MainWindow::psShowTrayMenu() {}

void MainWindow::psTrayMenuUpdated() {}

void MainWindow::psSetupTrayIcon() {
	if (noQtTrayIcon) {
		if (!cSupportTray()) return;
		updateIconCounters();
	} else {
		LOG(("Using Qt tray icon."));
		if (!trayIcon) {
			trayIcon = new QSystemTrayIcon(this);
			QIcon icon;
			QFileInfo iconFile(_trayIconImageFile());
			if (iconFile.exists()) {
				QByteArray path = QFile::encodeName(iconFile.absoluteFilePath());
				icon = QIcon(path.constData());
			} else {
				icon = Window::CreateIcon();
			}
			trayIcon->setIcon(icon);

			trayIcon->setToolTip(str_const_toString(AppName));
			connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
			        SLOT(toggleTray(QSystemTrayIcon::ActivationReason)), Qt::UniqueConnection);

			// This is very important for native notifications via libnotify!
			// Some notification servers compose several notifications with a "Reply"
			// action into one and after that a click on "Reply" button does not call
			// the specified callback from any of the sent notification - libnotify
			// just ignores ibus messages, but Qt tray icon at least emits this signal.
			connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(showFromTray()));

			App::wnd()->updateTrayMenu();
		}
		updateIconCounters();

		trayIcon->show();
	}
}

void MainWindow::workmodeUpdated(DBIWorkMode mode) {
	if (!cSupportTray()) return;

	if (mode == dbiwmWindowOnly) {
		if (noQtTrayIcon) {
		} else {
			if (trayIcon) {
				trayIcon->setContextMenu(0);
				trayIcon->deleteLater();
			}
			trayIcon = 0;
		}
	} else {
		if (noQtTrayIcon) {
		} else {
			psSetupTrayIcon();
		}
	}
}

void MainWindow::psUpdateIndicator() {}

void MainWindow::unreadCounterChangedHook() {
	setWindowTitle(titleText());
	updateIconCounters();
}

void MainWindow::updateIconCounters() {
	updateWindowIcon();

	if (noQtTrayIcon) {
	} else if (trayIcon) {
		QIcon icon;
		QFileInfo iconFile(_trayIconImageFile());
		if (iconFile.exists()) {
			QByteArray path = QFile::encodeName(iconFile.absoluteFilePath());
			icon = QIcon(path.constData());
		} else {
			qint32 counter = App::histories().unreadBadge();
			bool muted = App::histories().unreadOnlyMuted();

			auto &bg = (muted ? st::trayCounterBgMute : st::trayCounterBg);
			auto &fg = st::trayCounterFg;
			icon.addPixmap(App::pixmapFromImageInPlace(iconWithCounter(16, counter, bg, fg, true)));
			icon.addPixmap(App::pixmapFromImageInPlace(iconWithCounter(32, counter, bg, fg, true)));
		}
		trayIcon->setIcon(icon);
	}
}

void MainWindow::LibsLoaded() {
	noQtTrayIcon = !DesktopEnvironment::TryQtTrayIcon();

	LOG(("Tray Icon: Try Qt = %1, Prefer appindicator = %2").arg(Logs::b(!noQtTrayIcon)).arg(Logs::b(tryAppIndicator)));

	if (noQtTrayIcon) cSetSupportTray(false);
}

void MainWindow::psCreateTrayIcon() {
	if (!noQtTrayIcon) {
		LOG(("Tray Icon: Using Qt tray icon, available: %1").arg(Logs::b(QSystemTrayIcon::isSystemTrayAvailable())));
		cSetSupportTray(QSystemTrayIcon::isSystemTrayAvailable());
		return;
	}
}

void MainWindow::psFirstShow() {
	psCreateTrayIcon();

	psUpdateMargins();

	show();

	if (cWindowPos().maximized) {
		DEBUG_LOG(("Window Pos: First show, setting maximized."));
		setWindowState(Qt::WindowMaximized);
	}

	if ((cLaunchMode() == LaunchModeAutoStart && cStartMinimized()) || cStartInTray()) {
		setWindowState(Qt::WindowMinimized);
		if (Global::WorkMode().value() == dbiwmTrayOnly || Global::WorkMode().value() == dbiwmWindowAndTray) {
			hide();
		} else {
			show();
		}
	} else {
		show();
	}

	setPositionInited();
}

void MainWindow::psInitSysMenu() {}

void MainWindow::psUpdateMargins() {}

MainWindow::~MainWindow() {}

} // namespace Platform
