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
/// @file data/data_types.h Common types, functions for data_types.
#pragma once

#include <qglobal.h>
#include <QPair>
#include "core/utils.h" // TimeMs and stuff

using VideoId = quint64;
using AudioId = quint64;
using DocumentId = quint64;
using WebPageId = quint64;
using GameId = quint64;

using PeerId = quint64;

using MediaKey = QPair<quint64, quint64>;

enum ActionOnLoad { ActionOnLoadNone, ActionOnLoadOpen, ActionOnLoadOpenWith, ActionOnLoadPlayInline };

typedef qint32 UserId;
typedef qint32 ChatId;
typedef qint32 ChannelId;
constexpr ChannelId NoChannel = 0;

typedef qint32 MsgId;

/// @defgroup data_types.Constants
/// @{

constexpr const MsgId StartClientMsgId = -0x7FFFFFFF;
constexpr const MsgId EndClientMsgId = -0x40000000;
constexpr const MsgId ShowAtTheEndMsgId = -0x40000000;
constexpr const MsgId SwitchAtTopMsgId = -0x3FFFFFFF;
constexpr const MsgId ShowAtProfileMsgId = -0x3FFFFFFE;
constexpr const MsgId ShowAndStartBotMsgId = -0x3FFFFFD;
constexpr const MsgId ShowAtGameShareMsgId = -0x3FFFFFC;
constexpr const MsgId ServerMaxMsgId = 0x3FFFFFFF;
constexpr const MsgId ShowAtUnreadMsgId = 0;

/// @}

struct FullMsgId {
	FullMsgId() = default;
	FullMsgId(ChannelId channel, MsgId msg)
	    : channel(channel)
	    , msg(msg) {}
	ChannelId channel = NoChannel;
	MsgId msg = 0;
};

/// @brief Represents user continuous action.
struct SendAction {
	enum class Type {
		Typing,
		RecordVideo,
		UploadVideo,
		RecordVoice,
		UploadVoice,
		RecordRound,
		UploadRound,
		UploadPhoto,
		UploadFile,
		ChooseLocation,
		ChooseContact,
		PlayGame,
	};
	SendAction(Type type, TimeMs until, int progress = 0)
	    : type(type)
	    , until(until)
	    , progress(progress) {}
	Type type;
	TimeMs until;
	int progress;
};
