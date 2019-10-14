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

#include "base/flags.h"
#include "base/observer.h"
#include "data/data_document.h"

#include "history/history.h"

namespace Notify {

// Generic notifications about updates of some PeerData.
// You can subscribe to them by Notify::registerPeerObserver().
// 0x0000FFFFU for general peer updates (valid for any peer).
// 0xFFFF0000U for specific peer updates (valid for user / chat / channel).
struct PeerUpdate {
	PeerUpdate(PeerData *updated = nullptr)
	    : peer(updated) {}
	PeerData *peer;

	enum class Flag : quint32 {
		None = 0,

		// Common flags
		NameChanged = (1 << 0),
		UsernameChanged = (1 << 1),
		PhotoChanged = (1 << 2),
		AboutChanged = (1 << 3),
		NotificationsEnabled = (1 << 4),
		SharedMediaChanged = (1 << 5),
		MigrationChanged = (1 << 6),
		PinnedChanged = (1 << 7),
		RestrictionReasonChanged = (1 << 8),

		// For chats and channels
		InviteLinkChanged = (1 << 9),
		MembersChanged = (1 << 10),
		AdminsChanged = (1 << 11),
		BannedUsersChanged = (1 << 12),
		UnreadMentionsChanged = (1 << 13),

		// For users
		UserCanShareContact = (1 << 16),
		UserIsContact = (1 << 17),
		UserPhoneChanged = (1 << 18),
		UserIsBlocked = (1 << 19),
		BotCommandsChanged = (1 << 20),
		UserOnlineChanged = (1 << 21),
		BotCanAddToGroups = (1 << 22),
		UserCommonChatsChanged = (1 << 23),
		UserHasCalls = (1 << 24),

		// For chats
		ChatCanEdit = (1 << 16),

		// For channels
		ChannelAmIn = (1 << 16),
		ChannelRightsChanged = (1 << 17),
		ChannelStickersChanged = (1 << 18),
		ChannelPinnedChanged = (1 << 19),
	};
	using Flags = base::flags<Flag>;
	friend inline constexpr auto is_flag_type(Flag) {
		return true;
	}

	Flags flags = 0;

	// NameChanged data
	PeerData::Names oldNames;
	PeerData::NameFirstChars oldNameFirstChars;

	// SharedMediaChanged data
	qint32 mediaTypesMask = 0;
};

void peerUpdatedDelayed(const PeerUpdate &update);
inline void peerUpdatedDelayed(PeerData *peer, PeerUpdate::Flags events) {
	PeerUpdate update(peer);
	update.flags = events;
	peerUpdatedDelayed(update);
}
void peerUpdatedSendDelayed();

inline void mediaOverviewUpdated(PeerData *peer, MediaOverviewType type) {
	PeerUpdate update(peer);
	update.flags |= PeerUpdate::Flag::SharedMediaChanged;
	update.mediaTypesMask |= (1 << type);
	peerUpdatedDelayed(update);
}

class PeerUpdatedHandler {
public:
	template <typename Lambda>
	PeerUpdatedHandler(PeerUpdate::Flags events, Lambda &&handler)
	    : _events(events)
	    , _handler(std::move(handler)) {}
	void operator()(const PeerUpdate &update) const {
		if (update.flags & _events) {
			_handler(update);
		}
	}

private:
	PeerUpdate::Flags _events;
	Fn<void(const PeerUpdate &)> _handler;
};
base::Observable<PeerUpdate, PeerUpdatedHandler> &PeerUpdated();

} // namespace Notify
