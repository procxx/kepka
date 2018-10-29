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

#include "boxes/peer_list_box.h"
#include "profile/profile_channel_controllers.h"

namespace Dialogs {

void ShowSearchFromBox(PeerData *peer, Fn<void(not_null<UserData *>)> callback, Fn<void()> closedCallback);

class ChatSearchFromController : public PeerListController, protected base::Subscriber {
public:
	ChatSearchFromController(not_null<ChatData *> chat, Fn<void(not_null<UserData *>)> callback);

	void prepare() override;
	void rowClicked(not_null<PeerListRow *> row) override;

private:
	void rebuildRows();
	void checkForEmptyRows();
	void appendRow(not_null<UserData *> user);

	not_null<ChatData *> _chat;
	Fn<void(not_null<UserData *>)> _callback;
};

class ChannelSearchFromController : public Profile::ParticipantsBoxController {
public:
	ChannelSearchFromController(not_null<ChannelData *> channel, Fn<void(not_null<UserData *>)> callback);

	void prepare() override;
	void rowClicked(not_null<PeerListRow *> row) override;

protected:
	std::unique_ptr<PeerListRow> createRow(not_null<UserData *> user) const override;

private:
	Fn<void(not_null<UserData *>)> _callback;
};

} // namespace Dialogs
