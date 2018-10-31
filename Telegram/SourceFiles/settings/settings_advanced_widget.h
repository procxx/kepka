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

#include "mtproto/core_types.h"
#include "mtproto/rpc_sender.h"
#include "scheme.h"
#include "settings/settings_block_widget.h"
#include "settings/settings_chat_settings_widget.h"

namespace Settings {

class AdvancedWidget : public BlockWidget, public RPCSender {
	Q_OBJECT

public:
	AdvancedWidget(QWidget *parent, UserData *self);

private slots:
	void onManageLocalStorage();
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
	void onConnectionType();
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY
	void onAskQuestion();
	void onAskQuestionSure();
	void onUseDefaultTheme();
	void onToggleNightTheme();
	void onTelegramFAQ();
	void onLogOut();

private:
	void createControls();
	void checkNonDefaultTheme();
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
	void connectionTypeUpdated();
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY
	void supportGot(const MTPhelp_Support &support);
	QString getNightThemeToggleText() const;

	object_ptr<Ui::LinkButton> _manageLocalStorage = {nullptr};
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
	object_ptr<LabeledLink> _connectionType = {nullptr};
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY
	object_ptr<Ui::WidgetSlideWrap<Ui::LinkButton>> _useDefaultTheme = {nullptr};
	object_ptr<Ui::WidgetSlideWrap<Ui::LinkButton>> _toggleNightTheme = {nullptr};
	object_ptr<Ui::LinkButton> _askQuestion = {nullptr};
	object_ptr<Ui::LinkButton> _telegramFAQ = {nullptr};
	object_ptr<Ui::LinkButton> _logOut = {nullptr};

	mtpRequestId _supportGetRequest = 0;
};

} // namespace Settings
