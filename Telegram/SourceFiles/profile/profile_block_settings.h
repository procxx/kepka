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

#include "profile/profile_block_widget.h"

namespace Ui {
class Checkbox;
class LeftOutlineButton;
} // namespace Ui

namespace Notify {
struct PeerUpdate;
} // namespace Notify

namespace Profile {

class SettingsWidget : public BlockWidget {
	Q_OBJECT

public:
	SettingsWidget(QWidget *parent, PeerData *peer);

protected:
	// Resizes content and counts natural widget height for the desired width.
	int resizeGetHeight(int newWidth) override;

private slots:
	void onNotificationsChange();
	void onManageAdmins();
	void onRecentActions();
	void onManageBannedUsers();
	void onManageRestrictedUsers();
	void onInviteLink();

private:
	// Observed notifications.
	void notifyPeerUpdated(const Notify::PeerUpdate &update);

	void refreshButtons();
	void refreshEnableNotifications();
	void refreshManageAdminsButton();
	void refreshManageBannedUsersButton();
	void refreshInviteLinkButton();

	object_ptr<Ui::Checkbox> _enableNotifications;
	object_ptr<Ui::LeftOutlineButton> _manageAdmins = {nullptr};
	object_ptr<Ui::LeftOutlineButton> _recentActions = {nullptr};
	object_ptr<Ui::LeftOutlineButton> _manageBannedUsers = {nullptr};
	object_ptr<Ui::LeftOutlineButton> _manageRestrictedUsers = {nullptr};
	object_ptr<Ui::LeftOutlineButton> _inviteLink = {nullptr};
};

} // namespace Profile
