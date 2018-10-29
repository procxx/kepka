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

#include "base/timer.h"
#include "platform/mac/specific_mac_p.h"
#include "platform/platform_main_window.h"

#include <QMenuBar>
#include <QSystemTrayIcon>
#include <QTimer>

namespace Platform {

class MainWindow : public Window::MainWindow {
	Q_OBJECT

public:
	MainWindow();

	void psFirstShow();
	void psInitSysMenu();
	void psUpdateMargins();

	void psRefreshTaskbarIcon() {}

	bool psFilterNativeEvent(void *event);

	virtual QImage iconWithCounter(int size, int count, style::color bg, style::color fg, bool smallIcon) = 0;

	int getCustomTitleHeight() const {
		return _customTitleHeight;
	}

	~MainWindow();

	class Private;

public slots:
	void psShowTrayMenu();

	void psMacUndo();
	void psMacRedo();
	void psMacCut();
	void psMacCopy();
	void psMacPaste();
	void psMacDelete();
	void psMacSelectAll();

protected:
	bool eventFilter(QObject *obj, QEvent *evt) override;

	void stateChangedHook(Qt::WindowState state) override;
	void initHook() override;
	void updateWindowIcon() override;
	void titleVisibilityChangedHook() override;
	void unreadCounterChangedHook() override;

	QImage psTrayIcon(bool selected = false) const;
	bool hasTrayIcon() const override {
		return trayIcon;
	}

	void updateGlobalMenuHook() override;

	void workmodeUpdated(DBIWorkMode mode) override;

	QSystemTrayIcon *trayIcon = nullptr;
	QMenu *trayIconMenu = nullptr;

	QImage trayImg, trayImgSel;

	void psTrayMenuUpdated();
	void psSetupTrayIcon();
	virtual void placeSmallCounter(QImage &img, int size, int count, style::color bg, const QPoint &shift,
	                               style::color color) = 0;

	QTimer psUpdatedPositionTimer;

	void closeWithoutDestroy() override;

private:
	void hideAndDeactivate();
	void createGlobalMenu();
	void updateTitleCounter();
	void updateIconCounters();

	friend class Private;
	std::unique_ptr<Private> _private;

	mutable bool psIdle;
	mutable QTimer psIdleTimer;

	base::Timer _hideAfterFullScreenTimer;

	QMenuBar psMainMenu;
	QAction *psLogout = nullptr;
	QAction *psUndo = nullptr;
	QAction *psRedo = nullptr;
	QAction *psCut = nullptr;
	QAction *psCopy = nullptr;
	QAction *psPaste = nullptr;
	QAction *psDelete = nullptr;
	QAction *psSelectAll = nullptr;
	QAction *psContacts = nullptr;
	QAction *psAddContact = nullptr;
	QAction *psNewGroup = nullptr;
	QAction *psNewChannel = nullptr;
	QAction *psShowTelegram = nullptr;

	int _customTitleHeight = 0;
};

} // namespace Platform
