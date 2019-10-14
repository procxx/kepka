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
/// @file data/data_document.h Telegram Documents data_type.

#pragma once

#include <QByteArray>
#include <QScrollBar>
#include <QString>
#include <QSize>
#include <QTextEdit>
#include <QVector>

#include "core/click_handler.h" // LeftButtonClickHandler
#include "data/data_types.h"
#include "data/data_photo.h" // PhotoData
#include "ui/images.h" // StorageKey, ImagePtr, StorageImageLocation

#include "scheme.h"

class QTextEdit;

class HistoryItem;

enum LocationType {
	UnknownFileLocation = 0,
	// 1, 2, etc are used as "version" value in mediaKey() method.

	DocumentFileLocation = 0x4e45abe9, // mtpc_inputDocumentFileLocation
	AudioFileLocation = 0x74dc404d, // mtpc_inputAudioFileLocation
	VideoFileLocation = 0x3d0364ec, // mtpc_inputVideoFileLocation
};


using MediaKey = QPair<quint64, quint64>;

inline quint64 mediaMix32To64(qint32 a, qint32 b) {
	return (quint64(*reinterpret_cast<quint32 *>(&a)) << 32) | quint64(*reinterpret_cast<quint32 *>(&b));
}

// Old method, should not be used anymore.
// inline MediaKey mediaKey(LocationType type, qint32 dc, const quint64 &id) {
//	return MediaKey(mediaMix32To64(type, dc), id);
//}
// New method when version was introduced, type is not relevant anymore (all files are Documents).
inline MediaKey mediaKey(LocationType type, qint32 dc, const quint64 &id, qint32 version) {
	return (version > 0) ? MediaKey(mediaMix32To64(version, dc), id) : MediaKey(mediaMix32To64(type, dc), id);
}

inline StorageKey mediaKey(const MTPDfileLocation &location) {
	return storageKey(location.vdc_id.v, location.vvolume_id.v, location.vlocal_id.v);
}

struct DocumentAdditionalData {
	virtual ~DocumentAdditionalData() = default;
};

struct StickerData : public DocumentAdditionalData {
	ImagePtr img;
	QString alt;

	MTPInputStickerSet set = MTP_inputStickerSetEmpty();
	bool setInstalled() const;

	StorageImageLocation loc; // doc thumb location
};

struct SongData : public DocumentAdditionalData {
	qint32 duration = 0;
	QString title, performer;
};

typedef QVector<char> VoiceWaveform; // [0] == -1 -- counting, [0] == -2 -- could not count
struct VoiceData : public DocumentAdditionalData {
	~VoiceData();

	int duration = 0;
	VoiceWaveform waveform;
	char wavemax = 0;
};

VoiceWaveform documentWaveformDecode(const QByteArray &encoded5bit);
QByteArray documentWaveformEncode5bit(const VoiceWaveform &waveform);

// Don't change the values. This type is used for serialization.
enum DocumentType {
	FileDocument = 0,
	VideoDocument = 1,
	SongDocument = 2,
	StickerDocument = 3,
	AnimatedDocument = 4,
	VoiceDocument = 5,
	RoundVideoDocument = 6,
};

enum FileStatus {
	FileDownloadFailed = -2,
	FileUploadFailed = -1,
	FileUploading = 0,
	FileReady = 1,
};

namespace Serialize {
class Document;
} // namespace Serialize

/// @brief Any document object in chat.
class DocumentData {
public:
	static DocumentData *create(DocumentId id);
	static DocumentData *create(DocumentId id, qint32 dc, quint64 accessHash, qint32 version,
	                            const QVector<MTPDocumentAttribute> &attributes);
	static DocumentData *create(DocumentId id, const QString &url, const QVector<MTPDocumentAttribute> &attributes);

	void setattributes(const QVector<MTPDocumentAttribute> &attributes);

	void automaticLoad(const HistoryItem *item); // auto load sticker or video
	void automaticLoadSettingsChanged();

	enum FilePathResolveType {
		FilePathResolveCached,
		FilePathResolveChecked,
		FilePathResolveSaveFromData,
		FilePathResolveSaveFromDataSilent,
	};
	bool loaded(FilePathResolveType type = FilePathResolveCached) const;
	bool loading() const;
	QString loadingFilePath() const;
	bool displayLoading() const;
	void save(const QString &toFile, ActionOnLoad action = ActionOnLoadNone, const FullMsgId &actionMsgId = FullMsgId(),
	          LoadFromCloudSetting fromCloud = LoadFromCloudOrLocal, bool autoLoading = false);
	void cancel();
	double progress() const;
	qint32 loadOffset() const;
	bool uploading() const;

	QByteArray data() const;
	const FileLocation &location(bool check = false) const;
	void setLocation(const FileLocation &loc);

	QString filepath(FilePathResolveType type = FilePathResolveCached, bool forceSavingAs = false) const;

	bool saveToCache() const;

	void performActionOnLoad();

	void forget();
	ImagePtr makeReplyPreview();

	StickerData *sticker() {
		return (type == StickerDocument) ? static_cast<StickerData *>(_additional.get()) : nullptr;
	}
	void checkSticker() {
		StickerData *s = sticker();
		if (!s) return;

		automaticLoad(nullptr);
		if (s->img->isNull() && loaded()) {
			if (_data.isEmpty()) {
				const FileLocation &loc(location(true));
				if (loc.accessEnable()) {
					s->img = ImagePtr(loc.name());
					loc.accessDisable();
				}
			} else {
				s->img = ImagePtr(_data);
			}
		}
	}
	SongData *song() {
		return (type == SongDocument) ? static_cast<SongData *>(_additional.get()) : nullptr;
	}
	const SongData *song() const {
		return const_cast<DocumentData *>(this)->song();
	}
	VoiceData *voice() {
		return (type == VoiceDocument) ? static_cast<VoiceData *>(_additional.get()) : nullptr;
	}
	const VoiceData *voice() const {
		return const_cast<DocumentData *>(this)->voice();
	}
	bool isRoundVideo() const {
		return (type == RoundVideoDocument);
	}
	bool isAnimation() const {
		return (type == AnimatedDocument) || isRoundVideo() || hasMimeType(qstr("image/gif"));
	}
	bool isGifv() const {
		return (type == AnimatedDocument) && hasMimeType(qstr("video/mp4"));
	}
	bool isTheme() const {
		return _filename.endsWith(qstr(".tdesktop-theme"), Qt::CaseInsensitive) ||
		       _filename.endsWith(qstr(".tdesktop-palette"), Qt::CaseInsensitive);
	}
	bool tryPlaySong() const {
		return (song() != nullptr) || _mimeString.startsWith(qstr("audio/"), Qt::CaseInsensitive);
	}
	bool isMusic() const {
		if (auto s = song()) {
			return (s->duration > 0);
		}
		return false;
	}
	bool isVideo() const {
		return (type == VideoDocument);
	}
	qint32 duration() const {
		return (isAnimation() || isVideo()) ? _duration : -1;
	}
	bool isImage() const {
		return !isAnimation() && !isVideo() && (_duration > 0);
	}
	void recountIsImage();
	void setData(const QByteArray &data) {
		_data = data;
	}

	bool setRemoteVersion(qint32 version); // Returns true if version has changed.
	void setRemoteLocation(qint32 dc, quint64 access);
	void setContentUrl(const QString &url);
	bool hasRemoteLocation() const {
		return (_dc != 0 && _access != 0);
	}
	bool isValid() const {
		return hasRemoteLocation() || !_url.isEmpty();
	}
	MTPInputDocument mtpInput() const {
		if (_access) {
			return MTP_inputDocument(MTP_long(id), MTP_long(_access));
		}
		return MTP_inputDocumentEmpty();
	}

	// When we have some client-side generated document
	// (for example for displaying an external inline bot result)
	// and it has downloaded data, we can collect that data from it
	// to (this) received from the server "same" document.
	void collectLocalData(DocumentData *local);

	QString filename() const {
		return _filename;
	}
	QString mimeString() const {
		return _mimeString;
	}
	bool hasMimeType(QLatin1String mime) const {
		return !_mimeString.compare(mime, Qt::CaseInsensitive);
	}
	void setMimeString(const QString &mime) {
		_mimeString = mime;
	}


	~DocumentData();

	DocumentId id = 0;
	DocumentType type = FileDocument;
	QSize dimensions;
	qint32 date = 0;
	ImagePtr thumb, replyPreview;
	qint32 size = 0;

	FileStatus status = FileReady;
	qint32 uploadOffset = 0;

	qint32 md5[8];

	MediaKey mediaKey() const {
		return ::mediaKey(locationType(), _dc, id, _version);
	}

	static QString ComposeNameString(const QString &filename, const QString &songTitle, const QString &songPerformer);
	QString composeNameString() const {
		if (auto songData = song()) {
			return ComposeNameString(_filename, songData->title, songData->performer);
		}
		return ComposeNameString(_filename, QString(), QString());
	}

private:
	DocumentData(DocumentId id, qint32 dc, quint64 accessHash, qint32 version, const QString &url,
	             const QVector<MTPDocumentAttribute> &attributes);

	friend class Serialize::Document;

	LocationType locationType() const {
		return voice() ? AudioFileLocation : (isVideo() ? VideoFileLocation : DocumentFileLocation);
	}

	// Two types of location: from MTProto by dc+access+version or from web by url
	qint32 _dc = 0;
	quint64 _access = 0;
	qint32 _version = 0;
	QString _url;
	QString _filename;
	QString _mimeString;


	FileLocation _location;
	QByteArray _data;
	std::unique_ptr<DocumentAdditionalData> _additional;
	qint32 _duration = -1;

	ActionOnLoad _actionOnLoad = ActionOnLoadNone;
	FullMsgId _actionOnLoadMsgId;
	mutable FileLoader *_loader = nullptr;

	void notifyLayoutChanged() const;

	void destroyLoaderDelayed(mtpFileLoader *newValue = nullptr) const;
};


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
	static const int kMaxScroll;
	MessageCursor() = default;
	MessageCursor(int position, int anchor, int scroll)
	    : position(position)
	    , anchor(anchor)
	    , scroll(scroll) {}
	MessageCursor(const QTextEdit *edit) {
		fillFrom(edit);
	}
	void fillFrom(const QTextEdit *edit);
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
	int scroll = kMaxScroll;
};

inline bool operator==(const MessageCursor &a, const MessageCursor &b) {
	return (a.position == b.position) && (a.anchor == b.anchor) && (a.scroll == b.scroll);
}
