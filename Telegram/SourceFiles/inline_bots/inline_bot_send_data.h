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

#include "core/basic_types.h"
#include "history/history_location_manager.h"
#include "mtproto/type_utils.h"
#include "structs.h"

class History;
namespace InlineBots {

class Result;

namespace internal {

// Abstract class describing the message that will be
// sent if the user chooses this inline bot result.
// For each type of message that can be sent there will be a subclass.
class SendData {
public:
	SendData() = default;
	SendData(const SendData &other) = delete;
	SendData &operator=(const SendData &other) = delete;
	virtual ~SendData() = default;

	virtual bool isValid() const = 0;

	virtual void addToHistory(const Result *owner, History *history, MTPDmessage::Flags flags, MsgId msgId,
	                          UserId fromId, MTPint mtpDate, UserId viaBotId, MsgId replyToId,
	                          const QString &postAuthor, const MTPReplyMarkup &markup) const = 0;
	virtual QString getErrorOnSend(const Result *owner, History *history) const = 0;

	virtual bool hasLocationCoords() const {
		return false;
	}
	virtual bool getLocationCoords(LocationCoords *outLocation) const {
		return false;
	}
	virtual QString getLayoutTitle(const Result *owner) const;
	virtual QString getLayoutDescription(const Result *owner) const;
};

// This class implements addHistory() for most of the types hiding
// the differences in getSentMessageFields() method.
// Only SendFile and SendPhoto work by their own.
class SendDataCommon : public SendData {
public:
	struct SentMTPMessageFields {
		MTPString text = MTP_string("");
		MTPVector<MTPMessageEntity> entities = MTPnullEntities;
		MTPMessageMedia media = MTP_messageMediaEmpty();
	};
	virtual SentMTPMessageFields getSentMessageFields() const = 0;

	void addToHistory(const Result *owner, History *history, MTPDmessage::Flags flags, MsgId msgId, UserId fromId,
	                  MTPint mtpDate, UserId viaBotId, MsgId replyToId, const QString &postAuthor,
	                  const MTPReplyMarkup &markup) const override;

	QString getErrorOnSend(const Result *owner, History *history) const override;
};

// Plain text message.
class SendText : public SendDataCommon {
public:
	SendText(const QString &message, const EntitiesInText &entities, bool /* noWebPage*/)
	    : _message(message)
	    , _entities(entities) {}

	bool isValid() const override {
		return !_message.isEmpty();
	}

	SentMTPMessageFields getSentMessageFields() const override;

private:
	QString _message;
	EntitiesInText _entities;
};

// Message with geo location point media.
class SendGeo : public SendDataCommon {
public:
	explicit SendGeo(const MTPDgeoPoint &point)
	    : _location(point) {}

	bool isValid() const override {
		return true;
	}

	SentMTPMessageFields getSentMessageFields() const override;

	bool hasLocationCoords() const override {
		return true;
	}
	bool getLocationCoords(LocationCoords *outLocation) const override {
		Assert(outLocation != nullptr);
		*outLocation = _location;
		return true;
	}

private:
	LocationCoords _location;
};

// Message with venue media.
class SendVenue : public SendDataCommon {
public:
	SendVenue(const MTPDgeoPoint &point, const QString &venueId, const QString &provider, const QString &title,
	          const QString &address)
	    : _location(point)
	    , _venueId(venueId)
	    , _provider(provider)
	    , _title(title)
	    , _address(address) {}

	bool isValid() const override {
		return true;
	}

	SentMTPMessageFields getSentMessageFields() const override;

	bool hasLocationCoords() const override {
		return true;
	}
	bool getLocationCoords(LocationCoords *outLocation) const override {
		Assert(outLocation != nullptr);
		*outLocation = _location;
		return true;
	}

private:
	LocationCoords _location;
	QString _venueId, _provider, _title, _address;
};

// Message with shared contact media.
class SendContact : public SendDataCommon {
public:
	SendContact(const QString &firstName, const QString &lastName, const QString &phoneNumber)
	    : _firstName(firstName)
	    , _lastName(lastName)
	    , _phoneNumber(phoneNumber) {}

	bool isValid() const override {
		return (!_firstName.isEmpty() || !_lastName.isEmpty()) && !_phoneNumber.isEmpty();
	}

	SentMTPMessageFields getSentMessageFields() const override;

	QString getLayoutDescription(const Result *owner) const override;

private:
	QString _firstName, _lastName, _phoneNumber;
};

// Message with photo.
class SendPhoto : public SendData {
public:
	SendPhoto(PhotoData *photo, const QString &caption)
	    : _photo(photo)
	    , _caption(caption) {}

	bool isValid() const override {
		return _photo != nullptr;
	}

	void addToHistory(const Result *owner, History *history, MTPDmessage::Flags flags, MsgId msgId, UserId fromId,
	                  MTPint mtpDate, UserId viaBotId, MsgId replyToId, const QString &postAuthor,
	                  const MTPReplyMarkup &markup) const override;

	QString getErrorOnSend(const Result *owner, History *history) const override;

private:
	PhotoData *_photo;
	QString _caption;
};

// Message with file.
class SendFile : public SendData {
public:
	SendFile(DocumentData *document, const QString &caption)
	    : _document(document)
	    , _caption(caption) {}

	bool isValid() const override {
		return _document != nullptr;
	}

	void addToHistory(const Result *owner, History *history, MTPDmessage::Flags flags, MsgId msgId, UserId fromId,
	                  MTPint mtpDate, UserId viaBotId, MsgId replyToId, const QString &postAuthor,
	                  const MTPReplyMarkup &markup) const override;

	QString getErrorOnSend(const Result *owner, History *history) const override;

private:
	DocumentData *_document;
	QString _caption;
};

// Message with game.
class SendGame : public SendData {
public:
	SendGame(GameData *game)
	    : _game(game) {}

	bool isValid() const override {
		return _game != nullptr;
	}

	void addToHistory(const Result *owner, History *history, MTPDmessage::Flags flags, MsgId msgId, UserId fromId,
	                  MTPint mtpDate, UserId viaBotId, MsgId replyToId, const QString &postAuthor,
	                  const MTPReplyMarkup &markup) const override;

	QString getErrorOnSend(const Result *owner, History *history) const override;

private:
	GameData *_game;
};

} // namespace internal
} // namespace InlineBots
