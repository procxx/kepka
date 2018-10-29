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
#include "ui/twidget.h"

namespace Ui {
class IconButton;
class FlatLabel;
class Menu;
} // namespace Ui

namespace Profile {
class UserpicButton;
} // namespace Profile

namespace Window {

class MainMenu : public TWidget, private base::Subscriber {
public:
	MainMenu(QWidget *parent);

	void setInnerFocus() {
		setFocus();
	}
	void showFinished();

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void checkSelf();
	void updateControlsGeometry();
	void updatePhone();
	void refreshMenu();

	object_ptr<Profile::UserpicButton> _userpicButton = {nullptr};
	object_ptr<Ui::IconButton> _cloudButton = {nullptr};
	object_ptr<Ui::Menu> _menu;
	object_ptr<Ui::FlatLabel> _telegram;
	object_ptr<Ui::FlatLabel> _version;
	std::shared_ptr<QPointer<QAction>> _nightThemeAction;
	base::Timer _nightThemeSwitch;

	bool _showFinished = false;
	QString _phoneText;
};

} // namespace Window
