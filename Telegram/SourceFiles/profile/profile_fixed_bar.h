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
#include "base/observer.h"
#include "ui/twidget.h"

class PeerData;
class UserData;
class ChatData;
class ChannelData;

class QMouseEvent;

namespace Notify {
struct PeerUpdate;
} // namespace Notify

namespace Ui {
class RoundButton;
} // namespace Ui

namespace Profile {

class BackButton;

class FixedBar final : public TWidget, private base::Subscriber {
	Q_OBJECT

public:
	FixedBar(QWidget *parent, PeerData *peer);

	// When animating mode is enabled the content is hidden and the
	// whole fixed bar acts like a back button.
	void setAnimatingMode(bool enabled);

	// The "Share contact" button should be hidden if it is shown in the profile cover.
	void setHideShareContactButton(bool hideButton);

protected:
	void mousePressEvent(QMouseEvent *e) override;
	int resizeGetHeight(int newWidth) override;

public slots:
	void onBack();

private slots:
	void onEditChannel();
	void onEditGroup();
	void onAddContact();
	void onEditContact();
	void onShareContact();
	void onDeleteContact();
	void onLeaveGroup();

private:
	void refreshLang();
	void updateButtonsGeometry(int newWidth);
	void notifyPeerUpdate(const Notify::PeerUpdate &update);

	void refreshRightActions();
	void setUserActions();
	void setChatActions();
	void setMegagroupActions();
	void setChannelActions();

	enum class RightActionType {
		None,
		EditChannel,
		EditGroup,
		LeaveGroup,
		AddContact,
		EditContact,
		DeleteContact,
		ShareContact,
	};

	void addRightAction(RightActionType type, Fn<QString()> textFactory, const char *slot);
	void applyHideShareContactButton();

	PeerData *_peer;
	UserData *_peerUser;
	ChatData *_peerChat;
	ChannelData *_peerChannel;
	ChannelData *_peerMegagroup;

	object_ptr<BackButton> _backButton;

	int _currentAction = 0;
	struct RightAction {
		RightActionType type = RightActionType::None;
		Ui::RoundButton *button = nullptr;
	};
	QList<RightAction> _rightActions;

	bool _animatingMode = false;
	bool _hideShareContactButton = false;
};

} // namespace Profile
