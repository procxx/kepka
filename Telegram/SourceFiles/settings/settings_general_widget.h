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

#include "settings/settings_block_widget.h"

namespace Settings {

class GeneralWidget : public BlockWidget {
	Q_OBJECT

public:
	GeneralWidget(QWidget *parent, UserData *self);

protected:
	int resizeGetHeight(int newWidth) override;

private slots:
	void onChangeLanguage();

	void onEnableTrayIcon();
	void onEnableTaskbarIcon();

#ifndef OS_WIN_STORE
	void onAutoStart();
	void onStartMinimized();
	void onAddInSendTo();
#endif // !OS_WIN_STORE

	void onRestart();

private:
	void refreshControls();
	void updateWorkmode();

	object_ptr<Ui::LinkButton> _changeLanguage;
	object_ptr<Ui::Checkbox> _enableTrayIcon = {nullptr};
	object_ptr<Ui::Checkbox> _enableTaskbarIcon = {nullptr};
	object_ptr<Ui::Checkbox> _autoStart = {nullptr};
	object_ptr<Ui::WidgetSlideWrap<Ui::Checkbox>> _startMinimized = {nullptr};
	object_ptr<Ui::Checkbox> _addInSendTo = {nullptr};

	int _languagesLoadedSubscription = 0;
};

} // namespace Settings
