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

#include <QPair>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>

#include "scheme.h"
#include "ui/images.h"
#include "ui/text/text.h"
#include "ui/twidget.h"
#include "data/data_types.h"
#include "data/data_photo.h"
#include "data/data_document.h"

typedef quint64 PeerId;

static const WebPageId CancelledWebPageId = 0xFFFFFFFFFFFFFFFFULL;

inline bool operator==(const FullMsgId &a, const FullMsgId &b) {
	return (a.channel == b.channel) && (a.msg == b.msg);
}
inline bool operator!=(const FullMsgId &a, const FullMsgId &b) {
	return !(a == b);
}
inline bool operator<(const FullMsgId &a, const FullMsgId &b) {
	if (a.msg < b.msg) return true;
	if (a.msg > b.msg) return false;
	return a.channel < b.channel;
}

inline constexpr bool isClientMsgId(MsgId id) {
	return id >= StartClientMsgId && id < EndClientMsgId;
}

class UserData;
class ChatData;
class ChannelData;

typedef QMap<char, QPixmap> PreparedPhotoThumbs;

bool fileIsImage(const QString &name, const QString &mime);

namespace Serialize {
class Document;
} // namespace Serialize

class AudioMsgId {
public:
	enum class Type {
		Unknown,
		Voice,
		Song,
		Video,
	};

	AudioMsgId() = default;
	AudioMsgId(DocumentData *audio, const FullMsgId &msgId, quint32 playId = 0)
	    : _audio(audio)
	    , _contextId(msgId)
	    , _playId(playId) {
		setTypeFromAudio();
	}

	Type type() const {
		return _type;
	}
	DocumentData *audio() const {
		return _audio;
	}
	FullMsgId contextId() const {
		return _contextId;
	}
	quint32 playId() const {
		return _playId;
	}

	explicit operator bool() const {
		return _audio != nullptr;
	}

private:
	void setTypeFromAudio() {
		if (_audio->voice() || _audio->isRoundVideo()) {
			_type = Type::Voice;
		} else if (_audio->isVideo()) {
			_type = Type::Video;
		} else if (_audio->tryPlaySong()) {
			_type = Type::Song;
		} else {
			_type = Type::Unknown;
		}
	}

	DocumentData *_audio = nullptr;
	Type _type = Type::Unknown;
	FullMsgId _contextId;
	quint32 _playId = 0;
};

inline bool operator<(const AudioMsgId &a, const AudioMsgId &b) {
	if (quintptr(a.audio()) < quintptr(b.audio())) {
		return true;
	} else if (quintptr(b.audio()) < quintptr(a.audio())) {
		return false;
	} else if (a.contextId() < b.contextId()) {
		return true;
	} else if (b.contextId() < a.contextId()) {
		return false;
	}
	return (a.playId() < b.playId());
}
inline bool operator==(const AudioMsgId &a, const AudioMsgId &b) {
	return a.audio() == b.audio() && a.contextId() == b.contextId() && a.playId() == b.playId();
}
inline bool operator!=(const AudioMsgId &a, const AudioMsgId &b) {
	return !(a == b);
}

class DocumentClickHandler : public LeftButtonClickHandler {
public:
	DocumentClickHandler(DocumentData *document)
	    : _document(document) {}
	DocumentData *document() const {
		return _document;
	}

private:
	DocumentData *_document;
};

class DocumentSaveClickHandler : public DocumentClickHandler {
public:
	using DocumentClickHandler::DocumentClickHandler;
	static void doSave(DocumentData *document, bool forceSavingAs = false);

protected:
	void onClickImpl() const override;
};

class DocumentOpenClickHandler : public DocumentClickHandler {
public:
	using DocumentClickHandler::DocumentClickHandler;
	static void doOpen(DocumentData *document, HistoryItem *context, ActionOnLoad action = ActionOnLoadOpen);

protected:
	void onClickImpl() const override;
};

class GifOpenClickHandler : public DocumentOpenClickHandler {
public:
	using DocumentOpenClickHandler::DocumentOpenClickHandler;

protected:
	void onClickImpl() const override;
};

class VoiceSeekClickHandler : public DocumentOpenClickHandler {
public:
	using DocumentOpenClickHandler::DocumentOpenClickHandler;

protected:
	void onClickImpl() const override {}
};

class DocumentCancelClickHandler : public DocumentClickHandler {
public:
	using DocumentClickHandler::DocumentClickHandler;

protected:
	void onClickImpl() const override;
};

enum WebPageType { WebPagePhoto, WebPageVideo, WebPageProfile, WebPageArticle };
inline WebPageType toWebPageType(const QString &type) {
	if (type == qstr("photo")) return WebPagePhoto;
	if (type == qstr("video")) return WebPageVideo;
	if (type == qstr("profile")) return WebPageProfile;
	return WebPageArticle;
}

struct WebPageData {
	WebPageData(const WebPageId &id, WebPageType type = WebPageArticle, const QString &url = QString(),
	            const QString &displayUrl = QString(), const QString &siteName = QString(),
	            const QString &title = QString(), const TextWithEntities &description = TextWithEntities(),
	            DocumentData *doc = nullptr, PhotoData *photo = nullptr, qint32 duration = 0,
	            const QString &author = QString(), qint32 pendingTill = -1);

	void forget() {
		if (document) document->forget();
		if (photo) photo->forget();
	}

	WebPageId id;
	WebPageType type;
	QString url, displayUrl, siteName, title;
	TextWithEntities description;
	qint32 duration;
	QString author;
	PhotoData *photo;
	DocumentData *document;
	qint32 pendingTill;
};

QString saveFileName(const QString &title, const QString &filter, const QString &prefix, QString name, bool savingAs,
                     const QDir &dir = QDir());
MsgId clientMsgId();

struct MessageCursor {
	MessageCursor() = default;
	MessageCursor(int position, int anchor, int scroll)
	    : position(position)
	    , anchor(anchor)
	    , scroll(scroll) {}
	MessageCursor(const QTextEdit *edit) {
		fillFrom(edit);
	}
	void fillFrom(const QTextEdit *edit) {
		QTextCursor c = edit->textCursor();
		position = c.position();
		anchor = c.anchor();
		QScrollBar *s = edit->verticalScrollBar();
		scroll = (s && (s->value() != s->maximum())) ? s->value() : QFIXED_MAX;
	}
	void applyTo(QTextEdit *edit) {
		auto cursor = edit->textCursor();
		cursor.setPosition(anchor, QTextCursor::MoveAnchor);
		cursor.setPosition(position, QTextCursor::KeepAnchor);
		edit->setTextCursor(cursor);
		if (auto scrollbar = edit->verticalScrollBar()) {
			scrollbar->setValue(scroll);
		}
	}
	int position = 0;
	int anchor = 0;
	int scroll = QFIXED_MAX;
};

inline bool operator==(const MessageCursor &a, const MessageCursor &b) {
	return (a.position == b.position) && (a.anchor == b.anchor) && (a.scroll == b.scroll);
}
