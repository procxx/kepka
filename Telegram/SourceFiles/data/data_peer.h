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
// Copyright (c) 2019- Kepka Contributors, https://github.com/procxx
//
/// @file data/data_peer.h Data_types for any conversation (peer).

#pragma once

#include "data/data_types.h"
#include "core/click_handler.h"
#include "core/basic_types.h"

#include "ui/images.h"
#include "ui/text/text.h"

class PeerData;

class PeerClickHandler : public ClickHandler {
public:
	PeerClickHandler(not_null<PeerData *> peer);
	void onClick(Qt::MouseButton button) const override;

	not_null<PeerData *> peer() const {
		return _peer;
	}

private:
	not_null<PeerData *> _peer;
};

class UserData;
class ChatData;
class ChannelData;

struct NotifySettings {
	NotifySettings()
	    : flags(MTPDpeerNotifySettings::Flag::f_show_previews)
	    , sound(qsl("default")) {}
	MTPDpeerNotifySettings::Flags flags;
	TimeId mute = 0;
	QString sound;
	bool previews() const {
		return flags & MTPDpeerNotifySettings::Flag::f_show_previews;
	}
	bool silent() const {
		return flags & MTPDpeerNotifySettings::Flag::f_silent;
	}
};

using NotifySettingsPtr = NotifySettings *;

static const NotifySettingsPtr UnknownNotifySettings = NotifySettingsPtr(0);
static const NotifySettingsPtr EmptyNotifySettings = NotifySettingsPtr(1);
extern NotifySettings globalNotifyAll, globalNotifyUsers, globalNotifyChats;
extern NotifySettingsPtr globalNotifyAllPtr, globalNotifyUsersPtr, globalNotifyChatsPtr;

inline bool isNotifyMuted(NotifySettingsPtr settings, TimeId *changeIn = nullptr) {
	if (settings != UnknownNotifySettings && settings != EmptyNotifySettings) {
		auto t = unixtime();
		if (settings->mute > t) {
			if (changeIn) *changeIn = settings->mute - t + 1;
			return true;
		}
	}
	if (changeIn) *changeIn = 0;
	return false;
}


static constexpr int kUserColorsCount = 8;
static constexpr int kChatColorsCount = 4;
static constexpr int kChannelColorsCount = 4;

class EmptyUserpic {
public:
	EmptyUserpic();
	EmptyUserpic(int index, const QString &name);

	void set(int index, const QString &name);
	void clear();

	explicit operator bool() const {
		return (_impl != nullptr);
	}

	void paint(Painter &p, int x, int y, int outerWidth, int size) const;
	void paintRounded(Painter &p, int x, int y, int outerWidth, int size) const;
	void paintSquare(Painter &p, int x, int y, int outerWidth, int size) const;
	QPixmap generate(int size);
	StorageKey uniqueKey() const;

	~EmptyUserpic();

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
	friend class Impl;
};

static const PhotoId UnknownPeerPhotoId = 0xFFFFFFFFFFFFFFFFULL;

inline const QString &emptyUsername() {
	static QString empty;
	return empty;
}

typedef quint64 PeerId;
static const quint64 PeerIdMask = 0xFFFFFFFFULL;
static const quint64 PeerIdTypeMask = 0x300000000ULL;
static const quint64 PeerIdUserShift = 0x000000000ULL;
static const quint64 PeerIdChatShift = 0x100000000ULL;
static const quint64 PeerIdChannelShift = 0x200000000ULL;
inline bool peerIsUser(const PeerId &id) {
	return (id & PeerIdTypeMask) == PeerIdUserShift;
}
inline bool peerIsChat(const PeerId &id) {
	return (id & PeerIdTypeMask) == PeerIdChatShift;
}
inline bool peerIsChannel(const PeerId &id) {
	return (id & PeerIdTypeMask) == PeerIdChannelShift;
}
inline PeerId peerFromUser(UserId user_id) {
	return PeerIdUserShift | quint64(quint32(user_id));
}
inline PeerId peerFromChat(ChatId chat_id) {
	return PeerIdChatShift | quint64(quint32(chat_id));
}
inline PeerId peerFromChannel(ChannelId channel_id) {
	return PeerIdChannelShift | quint64(quint32(channel_id));
}
inline PeerId peerFromUser(const MTPint &user_id) {
	return peerFromUser(user_id.v);
}
inline PeerId peerFromChat(const MTPint &chat_id) {
	return peerFromChat(chat_id.v);
}
inline PeerId peerFromChannel(const MTPint &channel_id) {
	return peerFromChannel(channel_id.v);
}
inline qint32 peerToBareInt(const PeerId &id) {
	return qint32(quint32(id & PeerIdMask));
}
inline UserId peerToUser(const PeerId &id) {
	return peerIsUser(id) ? peerToBareInt(id) : 0;
}
inline ChatId peerToChat(const PeerId &id) {
	return peerIsChat(id) ? peerToBareInt(id) : 0;
}
inline ChannelId peerToChannel(const PeerId &id) {
	return peerIsChannel(id) ? peerToBareInt(id) : NoChannel;
}
inline MTPint peerToBareMTPInt(const PeerId &id) {
	return MTP_int(peerToBareInt(id));
}
inline PeerId peerFromMTP(const MTPPeer &peer) {
	switch (peer.type()) {
	case mtpc_peerUser: return peerFromUser(peer.c_peerUser().vuser_id);
	case mtpc_peerChat: return peerFromChat(peer.c_peerChat().vchat_id);
	case mtpc_peerChannel: return peerFromChannel(peer.c_peerChannel().vchannel_id);
	}
	return 0;
}
inline MTPpeer peerToMTP(const PeerId &id) {
	if (peerIsUser(id)) {
		return MTP_peerUser(peerToBareMTPInt(id));
	} else if (peerIsChat(id)) {
		return MTP_peerChat(peerToBareMTPInt(id));
	} else if (peerIsChannel(id)) {
		return MTP_peerChannel(peerToBareMTPInt(id));
	}
	return MTP_peerUser(MTP_int(0));
}
inline PeerId peerFromMessage(const MTPmessage &msg) {
	auto compute = [](auto &message) {
		auto from_id = message.has_from_id() ? peerFromUser(message.vfrom_id) : 0;
		auto to_id = peerFromMTP(message.vto_id);
		auto out = message.is_out();
		return (out || !peerIsUser(to_id)) ? to_id : from_id;
	};
	switch (msg.type()) {
	case mtpc_message: return compute(msg.c_message());
	case mtpc_messageService: return compute(msg.c_messageService());
	}
	return 0;
}
inline MTPDmessage::Flags flagsFromMessage(const MTPmessage &msg) {
	switch (msg.type()) {
	case mtpc_message: return msg.c_message().vflags.v;
	case mtpc_messageService: return mtpCastFlags(msg.c_messageService().vflags.v);
	}
	return 0;
}
inline MsgId idFromMessage(const MTPmessage &msg) {
	switch (msg.type()) {
	case mtpc_messageEmpty: return msg.c_messageEmpty().vid.v;
	case mtpc_message: return msg.c_message().vid.v;
	case mtpc_messageService: return msg.c_messageService().vid.v;
	}
	Unexpected("Type in idFromMessage()");
}
inline TimeId dateFromMessage(const MTPmessage &msg) {
	switch (msg.type()) {
	case mtpc_message: return msg.c_message().vdate.v;
	case mtpc_messageService: return msg.c_messageService().vdate.v;
	}
	return 0;
}

class PeerData {
protected:
	PeerData(const PeerId &id);
	PeerData(const PeerData &other) = delete;
	PeerData &operator=(const PeerData &other) = delete;

public:
	virtual ~PeerData() {
		if (notify != UnknownNotifySettings && notify != EmptyNotifySettings) {
			delete base::take(notify);
		}
	}

	bool isUser() const {
		return peerIsUser(id);
	}
	bool isChat() const {
		return peerIsChat(id);
	}
	bool isChannel() const {
		return peerIsChannel(id);
	}
	bool isSelf() const {
		return (input.type() == mtpc_inputPeerSelf);
	}
	bool isVerified() const;
	bool isMegagroup() const;
	bool isMuted() const {
		return (notify != EmptyNotifySettings) && (notify != UnknownNotifySettings) && (notify->mute >= unixtime());
	}
	bool canWrite() const;
	UserData *asUser();
	const UserData *asUser() const;
	ChatData *asChat();
	const ChatData *asChat() const;
	ChannelData *asChannel();
	const ChannelData *asChannel() const;
	ChannelData *asMegagroup();
	const ChannelData *asMegagroup() const;

	ChatData *migrateFrom() const;
	ChannelData *migrateTo() const;

	void updateFull();
	void updateFullForced();
	void fullUpdated();
	bool wasFullUpdated() const {
		return (_lastFullUpdate != 0);
	}

	const Text &dialogName() const;
	const QString &shortName() const;
	const QString &userName() const;

	const PeerId id;
	qint32 bareId() const {
		return qint32(quint32(id & 0xFFFFFFFFULL));
	}

	QString name;
	Text nameText;
	using Names = std::set<QString>;
	Names names; // for filtering
	using NameFirstChars = std::set<QChar>;
	NameFirstChars chars;

	enum LoadedStatus {
		NotLoaded = 0x00,
		MinimalLoaded = 0x01,
		FullLoaded = 0x02,
	};
	LoadedStatus loadedStatus = NotLoaded;
	MTPinputPeer input;

	int colorIndex() const {
		return _colorIndex;
	}
	void setUserpic(ImagePtr userpic);
	void paintUserpic(Painter &p, int x, int y, int size) const;
	void paintUserpicLeft(Painter &p, int x, int y, int w, int size) const {
		paintUserpic(p, rtl() ? (w - x - size) : x, y, size);
	}
	void paintUserpicRounded(Painter &p, int x, int y, int size) const;
	void paintUserpicSquare(Painter &p, int x, int y, int size) const;
	void loadUserpic(bool loadFirst = false, bool prior = true) {
		_userpic->load(loadFirst, prior);
	}
	bool userpicLoaded() const {
		return _userpic->loaded();
	}
	StorageKey userpicUniqueKey() const;
	void saveUserpic(const QString &path, int size) const;
	void saveUserpicRounded(const QString &path, int size) const;
	QPixmap genUserpic(int size) const;
	QPixmap genUserpicRounded(int size) const;

	PhotoId photoId = UnknownPeerPhotoId;
	StorageImageLocation photoLoc;

	int nameVersion = 1;

	NotifySettingsPtr notify = UnknownNotifySettings;

	// if this string is not empty we must not allow to open the
	// conversation and we must show this string instead
	virtual QString restrictionReason() const {
		return QString();
	}

	ClickHandlerPtr createOpenLink();
	const ClickHandlerPtr &openLink() {
		if (!_openLink) {
			_openLink = createOpenLink();
		}
		return _openLink;
	}

	ImagePtr currentUserpic() const;

protected:
	void updateNameDelayed(const QString &newName, const QString &newNameOrPhone, const QString &newUsername);

	ImagePtr _userpic;
	mutable EmptyUserpic _userpicEmpty;

private:
	void fillNames();

	ClickHandlerPtr _openLink;

	int _colorIndex = 0;
	TimeMs _lastFullUpdate = 0;
};


class BotCommand {
public:
	BotCommand(const QString &command, const QString &description)
	    : command(command)
	    , _description(description) {}
	QString command;

	bool setDescription(const QString &description) {
		if (_description != description) {
			_description = description;
			_descriptionText = Text();
			return true;
		}
		return false;
	}

	const Text &descriptionText() const;

private:
	QString _description;
	mutable Text _descriptionText;
};

struct BotInfo {
	bool inited = false;
	bool readsAllHistory = false;
	bool cantJoinGroups = false;
	int version = 0;
	QString description, inlinePlaceholder;
	QList<BotCommand> commands;
	Text text = Text{int(st::msgMinWidth)}; // description

	QString startToken, startGroupToken, shareGameShortName;
	PeerId inlineReturnPeerId = 0;
};

class PhotoData;
class UserData : public PeerData {
public:
	UserData(const PeerId &id)
	    : PeerData(id) {}
	void setPhoto(const MTPUserProfilePhoto &photo);

	void setName(const QString &newFirstName, const QString &newLastName, const QString &newPhoneName,
	             const QString &newUsername);

	void setPhone(const QString &newPhone);
	void setBotInfoVersion(int version);
	void setBotInfo(const MTPBotInfo &info);

	void setNameOrPhone(const QString &newNameOrPhone);

	void madeAction(TimeId when); // pseudo-online

	quint64 access = 0;

	MTPDuser::Flags flags = 0;
	bool isVerified() const {
		return flags & MTPDuser::Flag::f_verified;
	}
	bool isBotInlineGeo() const {
		return flags & MTPDuser::Flag::f_bot_inline_geo;
	}
	bool isInaccessible() const {
		return (access == NoAccess);
	}
	void setIsInaccessible() {
		access = NoAccess;
	}
	bool canWrite() const {
		return !isInaccessible();
	}
	bool isContact() const {
		return (contact > 0);
	}

	bool canShareThisContact() const;
	bool canAddContact() const {
		return canShareThisContact() && !isContact();
	}

	// In feedUsers() we check only that.
	// When actually trying to share contact we perform
	// a full check by canShareThisContact() call.
	bool canShareThisContactFast() const {
		return !_phone.isEmpty();
	}

	MTPInputUser inputUser;

	QString firstName;
	QString lastName;
	QString username;
	const QString &phone() const {
		return _phone;
	}
	QString nameOrPhone;
	Text phoneText;
	TimeId onlineTill = 0;
	qint32 contact = -1; // -1 - not contact, cant add (self, empty, deleted, foreign), 0 - not contact, can add
	                     // (request), 1 - contact

	enum class BlockStatus {
		Unknown,
		Blocked,
		NotBlocked,
	};
	BlockStatus blockStatus() const {
		return _blockStatus;
	}
	bool isBlocked() const {
		return (blockStatus() == BlockStatus::Blocked);
	}
	void setBlockStatus(BlockStatus blockStatus);

	enum class CallsStatus {
		Unknown,
		Enabled,
		Disabled,
		Private,
	};
	CallsStatus callsStatus() const {
		return _callsStatus;
	}
	bool hasCalls() const;
	void setCallsStatus(CallsStatus callsStatus);

	typedef QList<PhotoData *> Photos;
	Photos photos;
	int photosCount = -1; // -1 not loaded, 0 all loaded

	bool setAbout(const QString &newAbout);
	const QString &about() const {
		return _about;
	}

	std::unique_ptr<BotInfo> botInfo;

	QString restrictionReason() const override {
		return _restrictionReason;
	}
	void setRestrictionReason(const QString &reason);

	int commonChatsCount() const {
		return _commonChatsCount;
	}
	void setCommonChatsCount(int count);

private:
	QString _restrictionReason;
	QString _about;
	QString _phone;
	BlockStatus _blockStatus = BlockStatus::Unknown;
	CallsStatus _callsStatus = CallsStatus::Unknown;
	int _commonChatsCount = 0;

	static constexpr const quint64 NoAccess = 0xFFFFFFFFFFFFFFFFULL;
};

class ChatData : public PeerData {
public:
	ChatData(const PeerId &id)
	    : PeerData(id)
	    , inputChat(MTP_int(bareId())) {}
	void setPhoto(const MTPChatPhoto &photo, const PhotoId &phId = UnknownPeerPhotoId);

	void setName(const QString &newName);

	void invalidateParticipants();
	bool noParticipantInfo() const {
		return (count > 0 || amIn()) && participants.isEmpty();
	}

	MTPint inputChat;

	ChannelData *migrateToPtr = nullptr;

	int count = 0;
	TimeId date = 0;
	int version = 0;
	UserId creator = 0;

	MTPDchat::Flags flags = 0;
	bool isForbidden() const {
		return _isForbidden;
	}
	void setIsForbidden(bool forbidden) {
		_isForbidden = forbidden;
	}
	bool amIn() const {
		return !isForbidden() && !haveLeft() && !wasKicked();
	}
	bool canEdit() const {
		return !isDeactivated() && (amCreator() || (adminsEnabled() ? amAdmin() : amIn()));
	}
	bool canWrite() const {
		return !isDeactivated() && amIn();
	}
	bool haveLeft() const {
		return flags & MTPDchat::Flag::f_left;
	}
	bool wasKicked() const {
		return flags & MTPDchat::Flag::f_kicked;
	}
	bool adminsEnabled() const {
		return flags & MTPDchat::Flag::f_admins_enabled;
	}
	bool amCreator() const {
		return flags & MTPDchat::Flag::f_creator;
	}
	bool amAdmin() const {
		return (flags & MTPDchat::Flag::f_admin) && adminsEnabled();
	}
	bool isDeactivated() const {
		return flags & MTPDchat::Flag::f_deactivated;
	}
	bool isMigrated() const {
		return flags & MTPDchat::Flag::f_migrated_to;
	}
	QMap<not_null<UserData *>, int> participants;
	std::set<not_null<UserData *>> invitedByMe;
	std::set<not_null<UserData *>> admins;
	QList<not_null<UserData *>> lastAuthors;
	std::set<not_null<PeerData *>> markupSenders;
	int botStatus = 0; // -1 - no bots, 0 - unknown, 1 - one bot, that sees all history, 2 - other
	//	ImagePtr photoFull;

	void setInviteLink(const QString &newInviteLink);
	QString inviteLink() const {
		return _inviteLink;
	}

private:
	bool _isForbidden = false;
	QString _inviteLink;
};

enum PtsSkippedQueue {
	SkippedUpdate,
	SkippedUpdates,
};

class PtsWaiter {
public:
	PtsWaiter()
	    : _good(0)
	    , _last(0)
	    , _count(0)
	    , _applySkippedLevel(0)
	    , _requesting(false)
	    , _waitingForSkipped(false)
	    , _waitingForShortPoll(false) {}
	void init(qint32 pts) {
		_good = _last = _count = pts;
		clearSkippedUpdates();
	}
	bool inited() const {
		return _good > 0;
	}
	void setRequesting(bool isRequesting) {
		_requesting = isRequesting;
		if (_requesting) {
			clearSkippedUpdates();
		}
	}
	bool requesting() const {
		return _requesting;
	}
	bool waitingForSkipped() const {
		return _waitingForSkipped;
	}
	bool waitingForShortPoll() const {
		return _waitingForShortPoll;
	}
	void setWaitingForSkipped(ChannelData *channel, qint32 ms); // < 0 - not waiting
	void setWaitingForShortPoll(ChannelData *channel, qint32 ms); // < 0 - not waiting
	qint32 current() const {
		return _good;
	}
	bool updated(ChannelData *channel, qint32 pts, qint32 count, const MTPUpdates &updates);
	bool updated(ChannelData *channel, qint32 pts, qint32 count, const MTPUpdate &update);
	bool updated(ChannelData *channel, qint32 pts, qint32 count);
	bool updateAndApply(ChannelData *channel, qint32 pts, qint32 count, const MTPUpdates &updates);
	bool updateAndApply(ChannelData *channel, qint32 pts, qint32 count, const MTPUpdate &update);
	bool updateAndApply(ChannelData *channel, qint32 pts, qint32 count);
	void applySkippedUpdates(ChannelData *channel);
	void clearSkippedUpdates();

private:
	bool check(ChannelData *channel, qint32 pts,
	           qint32 count); // return false if need to save that update and apply later
	quint64 ptsKey(PtsSkippedQueue queue, qint32 pts);
	void checkForWaiting(ChannelData *channel);
	QMap<quint64, PtsSkippedQueue> _queue;
	QMap<quint64, MTPUpdate> _updateQueue;
	QMap<quint64, MTPUpdates> _updatesQueue;
	qint32 _good, _last, _count;
	qint32 _applySkippedLevel;
	bool _requesting, _waitingForSkipped, _waitingForShortPoll;
	quint32 _skippedKey = 0;
};

/// @brief Data model with supergroup (old name was Megagroup) info.
struct MegagroupInfo {
	struct Admin {
		explicit Admin(MTPChannelAdminRights rights)
		    : rights(rights) {}
		Admin(MTPChannelAdminRights rights, bool canEdit)
		    : rights(rights)
		    , canEdit(canEdit) {}
		MTPChannelAdminRights rights;
		bool canEdit = false;
	};
	struct Restricted {
		explicit Restricted(MTPChannelBannedRights rights)
		    : rights(rights) {}
		MTPChannelBannedRights rights;
	};
	QList<not_null<UserData *>> lastParticipants;
	QMap<not_null<UserData *>, Admin> lastAdmins;
	QMap<not_null<UserData *>, Restricted> lastRestricted;
	std::set<not_null<PeerData *>> markupSenders;
	std::set<not_null<UserData *>> bots;

	UserData *creator = nullptr; // nullptr means unknown
	int botStatus = 0; // -1 - no bots, 0 - unknown, 1 - one bot, that sees all history, 2 - other
	bool joinedMessageFound = false;
	MTPInputStickerSet stickerSet = MTP_inputStickerSetEmpty();

	enum LastParticipantsStatus {
		LastParticipantsUpToDate = 0x00,
		LastParticipantsAdminsOutdated = 0x01,
		LastParticipantsCountOutdated = 0x02,
	};
	mutable int lastParticipantsStatus = LastParticipantsUpToDate;
	int lastParticipantsCount = 0;

	ChatData *migrateFromPtr = nullptr;
};

class ChannelData : public PeerData {
public:
	ChannelData(const PeerId &id)
	    : PeerData(id)
	    , inputChannel(MTP_inputChannel(MTP_int(bareId()), MTP_long(0))) {}
	void setPhoto(const MTPChatPhoto &photo, const PhotoId &phId = UnknownPeerPhotoId);

	void setName(const QString &name, const QString &username);

	quint64 access = 0;

	MTPinputChannel inputChannel;

	QString username;

	// Returns true if about text was changed.
	bool setAbout(const QString &newAbout);
	const QString &about() const {
		return _about;
	}

	int membersCount() const {
		return _membersCount;
	}
	void setMembersCount(int newMembersCount);

	int adminsCount() const {
		return _adminsCount;
	}
	void setAdminsCount(int newAdminsCount);

	int restrictedCount() const {
		return _restrictedCount;
	}
	void setRestrictedCount(int newRestrictedCount);

	int kickedCount() const {
		return _kickedCount;
	}
	void setKickedCount(int newKickedCount);

	bool haveLeft() const {
		return flags & MTPDchannel::Flag::f_left;
	}
	bool amIn() const {
		return !isForbidden() && !haveLeft();
	}
	bool addsSignature() const {
		return flags & MTPDchannel::Flag::f_signatures;
	}
	bool isForbidden() const {
		return _isForbidden;
	}
	void setIsForbidden(bool forbidden) {
		_isForbidden = forbidden;
	}
	bool isVerified() const {
		return flags & MTPDchannel::Flag::f_verified;
	}

	static MTPChannelBannedRights KickedRestrictedRights();
	static constexpr auto kRestrictUntilForever = TimeId(INT_MAX);
	static bool IsRestrictedForever(TimeId until) {
		return !until || (until == kRestrictUntilForever);
	}
	void applyEditAdmin(not_null<UserData *> user, const MTPChannelAdminRights &oldRights,
	                    const MTPChannelAdminRights &newRights);
	void applyEditBanned(not_null<UserData *> user, const MTPChannelBannedRights &oldRights,
	                     const MTPChannelBannedRights &newRights);

	qint32 date = 0;
	int version = 0;
	MTPDchannel::Flags flags = 0;
	MTPDchannelFull::Flags flagsFull = 0;
	std::unique_ptr<MegagroupInfo> mgInfo;
	bool lastParticipantsCountOutdated() const {
		if (!mgInfo || !(mgInfo->lastParticipantsStatus & MegagroupInfo::LastParticipantsCountOutdated)) {
			return false;
		}
		if (mgInfo->lastParticipantsCount == membersCount()) {
			mgInfo->lastParticipantsStatus &= ~MegagroupInfo::LastParticipantsCountOutdated;
			return false;
		}
		return true;
	}
	void flagsUpdated();
	bool isMegagroup() const {
		return flags & MTPDchannel::Flag::f_megagroup;
	}
	bool isBroadcast() const {
		return flags & MTPDchannel::Flag::f_broadcast;
	}
	bool isPublic() const {
		return flags & MTPDchannel::Flag::f_username;
	}
	bool amCreator() const {
		return flags & MTPDchannel::Flag::f_creator;
	}
	const MTPChannelAdminRights &adminRightsBoxed() const {
		return _adminRights;
	}
	const MTPDchannelAdminRights &adminRights() const {
		return _adminRights.c_channelAdminRights();
	}
	void setAdminRights(const MTPChannelAdminRights &rights);
	bool hasAdminRights() const {
		return (adminRights().vflags.v != 0);
	}
	const MTPChannelBannedRights &restrictedRightsBoxed() const {
		return _restrictedRights;
	}
	const MTPDchannelBannedRights &restrictedRights() const {
		return _restrictedRights.c_channelBannedRights();
	}
	void setRestrictedRights(const MTPChannelBannedRights &rights);
	bool hasRestrictedRights() const {
		return (restrictedRights().vflags.v != 0);
	}
	bool hasRestrictedRights(qint32 now) const {
		return hasRestrictedRights() && (restrictedRights().vuntil_date.v > now);
	}
	bool canBanMembers() const {
		return adminRights().is_ban_users() || amCreator();
	}
	bool canEditMessages() const {
		return adminRights().is_edit_messages() || amCreator();
	}
	bool canDeleteMessages() const {
		return adminRights().is_delete_messages() || amCreator();
	}
	bool anyoneCanAddMembers() const {
		return (flags & MTPDchannel::Flag::f_democracy);
	}
	bool canAddMembers() const {
		return adminRights().is_invite_users() || amCreator() ||
		       (anyoneCanAddMembers() && amIn() && !hasRestrictedRights());
	}
	bool canAddAdmins() const {
		return adminRights().is_add_admins() || amCreator();
	}
	bool canPublish() const {
		return adminRights().is_post_messages() || amCreator();
	}
	bool canWrite() const {
		return amIn() && (canPublish() || (!isBroadcast() && !restrictedRights().is_send_messages()));
	}
	bool canViewMembers() const {
		return flagsFull & MTPDchannelFull::Flag::f_can_view_participants;
	}
	bool canViewAdmins() const {
		return (isMegagroup() || hasAdminRights() || amCreator());
	}
	bool canViewBanned() const {
		return (hasAdminRights() || amCreator());
	}
	bool canEditInformation() const {
		return adminRights().is_change_info() || amCreator();
	}
	bool canEditUsername() const {
		return amCreator() && (flagsFull & MTPDchannelFull::Flag::f_can_set_username);
	}
	bool canEditStickers() const {
		return (flagsFull & MTPDchannelFull::Flag::f_can_set_stickers);
	}
	bool canDelete() const {
		constexpr auto kDeleteChannelMembersLimit = 1000;
		return amCreator() && (membersCount() <= kDeleteChannelMembersLimit);
	}
	bool canEditAdmin(not_null<UserData *> user) const;
	bool canRestrictUser(not_null<UserData *> user) const;

	void setInviteLink(const QString &newInviteLink);
	QString inviteLink() const {
		return _inviteLink;
	}
	bool canHaveInviteLink() const {
		return adminRights().is_invite_link() || amCreator();
	}

	qint32 inviter = 0; // > 0 - user who invited me to channel, < 0 - not in channel
	QDateTime inviteDate;

	void ptsInit(qint32 pts) {
		_ptsWaiter.init(pts);
	}
	void ptsReceived(qint32 pts) {
		_ptsWaiter.updateAndApply(this, pts, 0);
	}
	bool ptsUpdateAndApply(qint32 pts, qint32 count) {
		return _ptsWaiter.updateAndApply(this, pts, count);
	}
	bool ptsUpdateAndApply(qint32 pts, qint32 count, const MTPUpdate &update) {
		return _ptsWaiter.updateAndApply(this, pts, count, update);
	}
	bool ptsUpdateAndApply(qint32 pts, qint32 count, const MTPUpdates &updates) {
		return _ptsWaiter.updateAndApply(this, pts, count, updates);
	}
	qint32 pts() const {
		return _ptsWaiter.current();
	}
	bool ptsInited() const {
		return _ptsWaiter.inited();
	}
	bool ptsRequesting() const {
		return _ptsWaiter.requesting();
	}
	void ptsSetRequesting(bool isRequesting) {
		return _ptsWaiter.setRequesting(isRequesting);
	}
	void ptsWaitingForShortPoll(qint32 ms) { // < 0 - not waiting
		return _ptsWaiter.setWaitingForShortPoll(this, ms);
	}
	bool ptsWaitingForSkipped() const {
		return _ptsWaiter.waitingForSkipped();
	}
	bool ptsWaitingForShortPoll() const {
		return _ptsWaiter.waitingForShortPoll();
	}

	QString restrictionReason() const override {
		return _restrictionReason;
	}
	void setRestrictionReason(const QString &reason);

	MsgId pinnedMessageId() const {
		return _pinnedMessageId;
	}
	void setPinnedMessageId(MsgId messageId);
	void clearPinnedMessage() {
		setPinnedMessageId(0);
	}

	bool canPinMessages() const;

private:
	bool canNotEditLastAdmin(not_null<UserData *> user) const;

	PtsWaiter _ptsWaiter;

	bool _isForbidden = true;
	int _membersCount = 1;
	int _adminsCount = 1;
	int _restrictedCount = 0;
	int _kickedCount = 0;

	MsgId _pinnedMessageId = 0;

	MTPChannelAdminRights _adminRights = MTP_channelAdminRights(MTP_flags(0));
	MTPChannelBannedRights _restrictedRights = MTP_channelBannedRights(MTP_flags(0), MTP_int(0));

	QString _restrictionReason;
	QString _about;

	QString _inviteLink;
};

inline bool isUser(const PeerData *peer) {
	return peer ? peer->isUser() : false;
}
inline UserData *PeerData::asUser() {
	return isUser() ? static_cast<UserData *>(this) : nullptr;
}
inline UserData *asUser(PeerData *peer) {
	return peer ? peer->asUser() : nullptr;
}
inline const UserData *PeerData::asUser() const {
	return isUser() ? static_cast<const UserData *>(this) : nullptr;
}
inline const UserData *asUser(const PeerData *peer) {
	return peer ? peer->asUser() : nullptr;
}
inline bool isChat(const PeerData *peer) {
	return peer ? peer->isChat() : false;
}
inline ChatData *PeerData::asChat() {
	return isChat() ? static_cast<ChatData *>(this) : nullptr;
}
inline ChatData *asChat(PeerData *peer) {
	return peer ? peer->asChat() : nullptr;
}
inline const ChatData *PeerData::asChat() const {
	return isChat() ? static_cast<const ChatData *>(this) : nullptr;
}
inline const ChatData *asChat(const PeerData *peer) {
	return peer ? peer->asChat() : nullptr;
}
inline bool isChannel(const PeerData *peer) {
	return peer ? peer->isChannel() : false;
}
inline ChannelData *PeerData::asChannel() {
	return isChannel() ? static_cast<ChannelData *>(this) : nullptr;
}
inline ChannelData *asChannel(PeerData *peer) {
	return peer ? peer->asChannel() : nullptr;
}
inline const ChannelData *PeerData::asChannel() const {
	return isChannel() ? static_cast<const ChannelData *>(this) : nullptr;
}
inline const ChannelData *asChannel(const PeerData *peer) {
	return peer ? peer->asChannel() : nullptr;
}
inline ChannelData *PeerData::asMegagroup() {
	return isMegagroup() ? static_cast<ChannelData *>(this) : nullptr;
}
inline ChannelData *asMegagroup(PeerData *peer) {
	return peer ? peer->asMegagroup() : nullptr;
}
inline const ChannelData *PeerData::asMegagroup() const {
	return isMegagroup() ? static_cast<const ChannelData *>(this) : nullptr;
}
inline const ChannelData *asMegagroup(const PeerData *peer) {
	return peer ? peer->asMegagroup() : nullptr;
}
inline bool isMegagroup(const PeerData *peer) {
	return peer ? peer->isMegagroup() : false;
}
inline ChatData *PeerData::migrateFrom() const {
	return (isMegagroup() && asChannel()->amIn()) ? asChannel()->mgInfo->migrateFromPtr : nullptr;
}
inline ChannelData *PeerData::migrateTo() const {
	return (isChat() && asChat()->migrateToPtr && asChat()->migrateToPtr->amIn()) ? asChat()->migrateToPtr : nullptr;
}
inline const Text &PeerData::dialogName() const {
	return migrateTo() ? migrateTo()->dialogName() :
	                     ((isUser() && !asUser()->phoneText.isEmpty()) ? asUser()->phoneText : nameText);
}
inline const QString &PeerData::shortName() const {
	return isUser() ? asUser()->firstName : name;
}
inline const QString &PeerData::userName() const {
	return isUser() ? asUser()->username : (isChannel() ? asChannel()->username : emptyUsername());
}
inline bool PeerData::isVerified() const {
	return isUser() ? asUser()->isVerified() : (isChannel() ? asChannel()->isVerified() : false);
}
inline bool PeerData::isMegagroup() const {
	return isChannel() ? asChannel()->isMegagroup() : false;
}
inline bool PeerData::canWrite() const {
	return isChannel() ? asChannel()->canWrite() :
	                     (isChat() ? asChat()->canWrite() : (isUser() ? asUser()->canWrite() : false));
}
