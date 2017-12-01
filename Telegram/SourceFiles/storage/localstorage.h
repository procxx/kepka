/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

#include "core/basic_types.h"
#include "storage/file_download.h"
#include "auth_session.h"

namespace Window {
namespace Theme {
struct Cached;
} // namespace Theme
} // namespace Window

namespace Local {

struct StoredAuthSession {
	AuthSessionData data;
	double dialogsWidthRatio;
};

void start();
void finish();

void readSettings();
void writeSettings();
void writeUserSettings();
void writeMtpData();

void reset();

bool checkPasscode(const QByteArray &passcode);
void setPasscode(const QByteArray &passcode);

enum ClearManagerTask {
	ClearManagerAll = 0xFFFF,
	ClearManagerDownloads = 0x01,
	ClearManagerStorage = 0x02,
};

struct ClearManagerData;
class ClearManager : public QObject {
	Q_OBJECT

public:
	ClearManager();
	bool addTask(int task);
	bool hasTask(ClearManagerTask task);
	void start();
	void stop();

signals:
	void succeed(int task, void *manager);
	void failed(int task, void *manager);

private slots:
	void onStart();

private:
	~ClearManager();

	ClearManagerData *data;

};

enum ReadMapState {
	ReadMapFailed = 0,
	ReadMapDone = 1,
	ReadMapPassNeeded = 2,
};
ReadMapState readMap(const QByteArray &pass);
int32_t oldMapVersion();

int32_t oldSettingsVersion();

struct MessageDraft {
	MessageDraft(MsgId msgId = 0, TextWithTags textWithTags = TextWithTags(), bool previewCancelled = false)
		: msgId(msgId)
		, textWithTags(textWithTags)
		, previewCancelled(previewCancelled) {
	}
	MsgId msgId;
	TextWithTags textWithTags;
	bool previewCancelled;
};
void writeDrafts(const PeerId &peer, const MessageDraft &localDraft, const MessageDraft &editDraft);
void readDraftsWithCursors(History *h);
void writeDraftCursors(const PeerId &peer, const MessageCursor &localCursor, const MessageCursor &editCursor);
bool hasDraftCursors(const PeerId &peer);
bool hasDraft(const PeerId &peer);

void writeFileLocation(MediaKey location, const FileLocation &local);
FileLocation readFileLocation(MediaKey location, bool check = true);

void writeImage(const StorageKey &location, const ImagePtr &img);
void writeImage(const StorageKey &location, const StorageImageSaved &jpeg, bool overwrite = true);
TaskId startImageLoad(const StorageKey &location, mtpFileLoader *loader);
int32_t hasImages();
int64_t storageImagesSize();

void writeStickerImage(const StorageKey &location, const QByteArray &data, bool overwrite = true);
TaskId startStickerImageLoad(const StorageKey &location, mtpFileLoader *loader);
bool willStickerImageLoad(const StorageKey &location);
bool copyStickerImage(const StorageKey &oldLocation, const StorageKey &newLocation);
int32_t hasStickers();
int64_t storageStickersSize();

void writeAudio(const StorageKey &location, const QByteArray &data, bool overwrite = true);
TaskId startAudioLoad(const StorageKey &location, mtpFileLoader *loader);
bool copyAudio(const StorageKey &oldLocation, const StorageKey &newLocation);
int32_t hasAudios();
int64_t storageAudiosSize();

void writeWebFile(const QString &url, const QByteArray &data, bool overwrite = true);
TaskId startWebFileLoad(const QString &url, webFileLoader *loader);
int32_t hasWebFiles();
int64_t storageWebFilesSize();

void countVoiceWaveform(DocumentData *document);

void cancelTask(TaskId id);

void writeInstalledStickers();
void writeFeaturedStickers();
void writeRecentStickers();
void writeFavedStickers();
void writeArchivedStickers();
void readInstalledStickers();
void readFeaturedStickers();
void readRecentStickers();
void readFavedStickers();
void readArchivedStickers();
int32_t countStickersHash(bool checkOutdatedInfo = false);
int32_t countRecentStickersHash();
int32_t countFavedStickersHash();
int32_t countFeaturedStickersHash();

void writeSavedGifs();
void readSavedGifs();
int32_t countSavedGifsHash();

void writeBackground(int32_t id, const QImage &img);
bool readBackground();

void writeTheme(const QString &pathRelative, const QString &pathAbsolute, const QByteArray &content, const Window::Theme::Cached &cache);
void clearTheme();
bool hasTheme();
QString themeAbsolutePath();
QString themePaletteAbsolutePath();
bool copyThemeColorsToPalette(const QString &file);

void writeLangPack();

void writeRecentHashtagsAndBots();
void readRecentHashtagsAndBots();

void addSavedPeer(PeerData *peer, const QDateTime &position);
void removeSavedPeer(PeerData *peer);
void readSavedPeers();

void writeReportSpamStatuses();

void makeBotTrusted(UserData *bot);
bool isBotTrusted(UserData *bot);

bool encrypt(const void *src, void *dst, uint32_t len, const void *key128);
bool decrypt(const void *src, void *dst, uint32_t len, const void *key128);

namespace internal {

class Manager : public QObject {
	Q_OBJECT

public:
	Manager();

	void writeMap(bool fast);
	void writingMap();
	void writeLocations(bool fast);
	void writingLocations();
	void finish();

public slots:
	void mapWriteTimeout();
	void locationsWriteTimeout();

private:
	QTimer _mapWriteTimer;
	QTimer _locationsWriteTimer;

};

} // namespace internal
} // namespace Local
