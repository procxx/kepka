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

#include "base/object_ptr.h"
#include "mtproto/rpc_sender.h"
#include "scheme.h"
#include "settings/settings_block_widget.h"
#include "settings/settings_chat_settings_widget.h"

namespace Settings {

class LocalPasscodeState : public TWidget, private base::Subscriber {
	Q_OBJECT

public:
	LocalPasscodeState(QWidget *parent);

protected:
	int resizeGetHeight(int newWidth) override;

private slots:
	void onEdit();
	void onTurnOff();

private:
	static QString GetEditPasscodeText();

	void updateControls();

	object_ptr<Ui::LinkButton> _edit;
	object_ptr<Ui::LinkButton> _turnOff;
};

class CloudPasswordState : public TWidget, private base::Subscriber, public RPCSender {
	Q_OBJECT

public:
	CloudPasswordState(QWidget *parent);

protected:
	int resizeGetHeight(int newWidth) override;

	void paintEvent(QPaintEvent *e) override;

private slots:
	void onEdit();
	void onTurnOff();
	void onReloadPassword(Qt::ApplicationState state = Qt::ApplicationActive);

private:
	void getPasswordDone(const MTPaccount_Password &result);
	void offPasswordDone(const MTPBool &result);
	bool offPasswordFail(const RPCError &error);

	object_ptr<Ui::LinkButton> _edit;
	object_ptr<Ui::LinkButton> _turnOff;

	QString _waitingConfirm;
	QByteArray _curPasswordSalt;
	bool _hasPasswordRecovery = false;
	QString _curPasswordHint;
	QByteArray _newPasswordSalt;
};

class PrivacyWidget : public BlockWidget {
	Q_OBJECT

public:
	PrivacyWidget(QWidget *parent, UserData *self);

private slots:
	void onBlockedUsers();
	void onLastSeenPrivacy();
	void onCallsPrivacy();
	void onGroupsInvitePrivacy();
	void onAutoLock();
	void onShowSessions();
	void onSelfDestruction();

private:
	static QString GetAutoLockText();

	void createControls();
	void autoLockUpdated();

	object_ptr<Ui::LinkButton> _blockedUsers = {nullptr};
	object_ptr<Ui::LinkButton> _lastSeenPrivacy = {nullptr};
	object_ptr<Ui::LinkButton> _callsPrivacy = {nullptr};
	object_ptr<Ui::LinkButton> _groupsInvitePrivacy = {nullptr};
	object_ptr<LocalPasscodeState> _localPasscodeState = {nullptr};
	object_ptr<Ui::WidgetSlideWrap<LabeledLink>> _autoLock = {nullptr};
	object_ptr<CloudPasswordState> _cloudPasswordState = {nullptr};
	object_ptr<Ui::LinkButton> _showAllSessions = {nullptr};
	object_ptr<Ui::LinkButton> _selfDestruction = {nullptr};
};

} // namespace Settings
