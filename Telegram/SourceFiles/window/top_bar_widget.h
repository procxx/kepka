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

#include "ui/animation.h"
#include "ui/twidget.h"

namespace Ui {
class PeerAvatarButton;
class RoundButton;
class IconButton;
class DropdownMenu;
} // namespace Ui

namespace Window {

class Controller;

class TopBarWidget : public TWidget, private base::Subscriber {
	Q_OBJECT

public:
	TopBarWidget(QWidget *parent, not_null<Window::Controller *> controller);

	struct SelectedState {
		bool textSelected = false;
		int count = 0;
		int canDeleteCount = 0;
		int canForwardCount = 0;
	};

	void updateControlsVisibility();
	void showSelected(SelectedState state);
	void animationFinished();
	void updateMembersShowArea();

	Ui::RoundButton *mediaTypeButton();

	static void paintUnreadCounter(Painter &p, int outerWidth);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	bool eventFilter(QObject *obj, QEvent *e) override;

signals:
	void clicked();

private:
	void refreshLang();
	void updateControlsGeometry();
	void selectedShowCallback();

	void onForwardSelection();
	void onDeleteSelection();
	void onClearSelection();
	void onInfoClicked();
	void onCall();
	void onSearch();
	void showMenu();

	void updateAdaptiveLayout();
	int countSelectedButtonsTop(double selectedShown);

	not_null<Window::Controller *> _controller;

	PeerData *_searchInPeer = nullptr;
	int _selectedCount = 0;
	bool _canDelete = false;
	bool _canForward = false;

	Animation _selectedShown;

	object_ptr<Ui::RoundButton> _clearSelection;
	object_ptr<Ui::RoundButton> _forward, _delete;

	object_ptr<Ui::PeerAvatarButton> _info;
	object_ptr<Ui::RoundButton> _mediaType;

	object_ptr<Ui::IconButton> _call;
	object_ptr<Ui::IconButton> _search;
	object_ptr<Ui::IconButton> _menuToggle;
	object_ptr<Ui::DropdownMenu> _menu = {nullptr};

	object_ptr<TWidget> _membersShowArea = {nullptr};

	int _unreadCounterSubscription = 0;
};

} // namespace Window
