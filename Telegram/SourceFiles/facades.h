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

#include "base/lambda_guard.h"
#include "history/history.h" // For MediaOverviewType ffs..
#include "structs.h"

class PeerData;
class LayerWidget;
class BoxContent;
class UserData;
class History;
class HistoryItem;

namespace InlineBots {
namespace Layout {
class ItemBase;
} // namespace Layout
} // namespace InlineBots

namespace App {
namespace internal {

void CallDelayed(int duration, FnMut<void()> &&lambda);

} // namespace internal

template <typename Lambda>
inline void CallDelayed(int duration, base::lambda_internal::guard_with_QObject<Lambda> &&guarded) {
	return internal::CallDelayed(duration, std::move(guarded));
}

template <typename Lambda>
inline void CallDelayed(int duration, base::lambda_internal::guard_with_weak<Lambda> &&guarded) {
	return internal::CallDelayed(duration, std::move(guarded));
}

template <typename Lambda> inline void CallDelayed(int duration, const QObject *object, Lambda &&lambda) {
	return internal::CallDelayed(duration, base::lambda_guarded(object, std::forward<Lambda>(lambda)));
}

template <typename Lambda>
inline void CallDelayed(int duration, const base::enable_weak_from_this *object, Lambda &&lambda) {
	return internal::CallDelayed(duration, base::lambda_guarded(object, std::forward<Lambda>(lambda)));
}

template <typename Lambda> inline auto LambdaDelayed(int duration, const QObject *object, Lambda &&lambda) {
	auto guarded = base::lambda_guarded(object, std::forward<Lambda>(lambda));
	return [saved = std::move(guarded), duration] {
		auto copy = saved;
		internal::CallDelayed(duration, std::move(copy));
	};
}

template <typename Lambda>
inline auto LambdaDelayed(int duration, const base::enable_weak_from_this *object, Lambda &&lambda) {
	auto guarded = base::lambda_guarded(object, std::forward<Lambda>(lambda));
	return [saved = std::move(guarded), duration] {
		auto copy = saved;
		internal::CallDelayed(duration, std::move(copy));
	};
}

template <typename Lambda> inline auto LambdaDelayedOnce(int duration, const QObject *object, Lambda &&lambda) {
	auto guarded = base::lambda_guarded(object, std::forward<Lambda>(lambda));
	return [saved = std::move(guarded), duration]() mutable { internal::CallDelayed(duration, std::move(saved)); };
}

template <typename Lambda>
inline auto LambdaDelayedOnce(int duration, const base::enable_weak_from_this *object, Lambda &&lambda) {
	auto guarded = base::lambda_guarded(object, std::forward<Lambda>(lambda));
	return [saved = std::move(guarded), duration]() mutable { internal::CallDelayed(duration, std::move(saved)); };
}

void sendBotCommand(PeerData *peer, UserData *bot, const QString &cmd, MsgId replyTo = 0);
bool insertBotCommand(const QString &cmd);
void activateBotCommand(const HistoryItem *msg, int row, int col);
void searchByHashtag(const QString &tag, PeerData *inPeer);
void openPeerByName(const QString &username, MsgId msgId = ShowAtUnreadMsgId, const QString &startToken = QString());
void joinGroupByHash(const QString &hash);
void stickersBox(const QString &name);
void removeDialog(History *history);
void showSettings();

void activateClickHandler(ClickHandlerPtr handler, Qt::MouseButton button);

void logOutDelayed();

} // namespace App

namespace Ui {
namespace internal {

void showBox(object_ptr<BoxContent> content, ShowLayerOptions options);

} // namespace internal

void showMediaPreview(DocumentData *document);
void showMediaPreview(PhotoData *photo);
void hideMediaPreview();

template <typename BoxType>
QPointer<BoxType> show(object_ptr<BoxType> content, ShowLayerOptions options = CloseOtherLayers) {
	auto result = QPointer<BoxType>(content.data());
	internal::showBox(std::move(content), options);
	return result;
}

void hideLayer(bool fast = false);
void hideSettingsAndLayer(bool fast = false);
bool isLayerShown();

void repaintHistoryItem(not_null<const HistoryItem *> item);
void autoplayMediaInlineAsync(const FullMsgId &msgId);

void showPeerProfile(const PeerId &peer);
void showPeerProfile(const PeerData *peer);
void showPeerProfile(const History *history);

void showPeerOverview(const PeerId &peer, MediaOverviewType type);
void showPeerOverview(const PeerData *peer, MediaOverviewType type);
void showPeerOverview(const History *history, MediaOverviewType type);

enum class ShowWay {
	ClearStack,
	Forward,
	Backward,
};
void showPeerHistory(const PeerId &peer, MsgId msgId, ShowWay way = ShowWay::ClearStack);
void showPeerHistory(const PeerData *peer, MsgId msgId, ShowWay way = ShowWay::ClearStack);
void showPeerHistory(const History *history, MsgId msgId, ShowWay way = ShowWay::ClearStack);
void showPeerHistoryAtItem(const HistoryItem *item, ShowWay way = ShowWay::ClearStack);
void showPeerHistoryAsync(const PeerId &peer, MsgId msgId, ShowWay way = ShowWay::ClearStack);
void showChatsList();
void showChatsListAsync();
PeerData *getPeerForMouseAction();

bool skipPaintEvent(QWidget *widget, QPaintEvent *event);

} // namespace Ui

enum ClipStopperType {
	ClipStopperMediaview,
	ClipStopperSavedGifsPanel,
};

namespace Notify {

void userIsBotChanged(UserData *user);
void userIsContactChanged(UserData *user, bool fromThisApp = false);
void botCommandsChanged(UserData *user);

void inlineBotRequesting(bool requesting);
void replyMarkupUpdated(const HistoryItem *item);
void inlineKeyboardMoved(const HistoryItem *item, int oldKeyboardTop, int newKeyboardTop);
bool switchInlineBotButtonReceived(const QString &query, UserData *samePeerBot = nullptr, MsgId samePeerReplyTo = 0);

void migrateUpdated(PeerData *peer);

void historyItemLayoutChanged(const HistoryItem *item);
void historyMuteUpdated(History *history);

// handle pending resize() / paint() on history items
void handlePendingHistoryUpdate();
void unreadCounterUpdated();


enum class ScreenCorner {
	TopLeft = 0,
	TopRight = 1,
	BottomRight = 2,
	BottomLeft = 3,
};

inline bool IsLeftCorner(ScreenCorner corner) {
	return (corner == ScreenCorner::TopLeft) || (corner == ScreenCorner::BottomLeft);
}

inline bool IsTopCorner(ScreenCorner corner) {
	return (corner == ScreenCorner::TopLeft) || (corner == ScreenCorner::TopRight);
}

} // namespace Notify

#define DeclareReadOnlyVar(Type, Name) const Type &Name();
#define DeclareRefVar(Type, Name) DeclareReadOnlyVar(Type, Name) Type &Ref##Name();
#define DeclareVar(Type, Name) DeclareRefVar(Type, Name) void Set##Name(const Type &Name);

namespace Sandbox {

bool CheckBetaVersionDir();
void WorkingDirReady();

void MainThreadTaskAdded();

void start();
bool started();
void finish();

quint64 UserTag();

DeclareVar(QByteArray, LastCrashDump);
DeclareVar(ProxyData, PreLaunchProxy);

} // namespace Sandbox

namespace Adaptive {

enum class WindowLayout {
	OneColumn,
	SmallColumn,
	Normal,
};

enum class ChatLayout {
	Normal,
	Wide,
};

} // namespace Adaptive

namespace DebugLogging {
enum Flags {
	FileLoaderFlag = 0x00000001,
};
} // namespace DebugLogging

namespace Stickers {

constexpr auto DefaultSetId = 0; // for backward compatibility
constexpr auto CustomSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFFF);
constexpr auto RecentSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFFE); // for emoji/stickers panel, should not appear in Sets
constexpr auto NoneSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFFD); // for emoji/stickers panel, should not appear in Sets
constexpr auto CloudRecentSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFFC); // for cloud-stored recent stickers
constexpr auto FeaturedSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFFB); // for emoji/stickers panel, should not appear in Sets
constexpr auto FavedSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFFA); // for cloud-stored faved stickers
constexpr auto MegagroupSetId = Q_UINT64_C(0xFFFFFFFFFFFFFFEF); // for setting up megagroup sticker set
struct Set {
	Set(quint64 id, quint64 access, const QString &title, const QString &shortName, qint32 count, qint32 hash,
	    MTPDstickerSet::Flags flags)
	    : id(id)
	    , access(access)
	    , title(title)
	    , shortName(shortName)
	    , count(count)
	    , hash(hash)
	    , flags(flags) {}
	quint64 id, access;
	QString title, shortName;
	qint32 count, hash;
	MTPDstickerSet::Flags flags;
	StickerPack stickers;
	StickersByEmojiMap emoji;
};
using Sets = QMap<quint64, Set>;
using Order = QList<quint64>;

inline MTPInputStickerSet inputSetId(const Set &set) {
	if (set.id && set.access) {
		return MTP_inputStickerSetID(MTP_long(set.id), MTP_long(set.access));
	}
	return MTP_inputStickerSetShortName(MTP_string(set.shortName));
}

} // namespace Stickers

namespace Global {

bool started();
void start();
void finish();

DeclareRefVar(SingleQueuedInvokation, HandleHistoryUpdate);
DeclareRefVar(SingleQueuedInvokation, HandleUnreadCounterUpdate);
DeclareRefVar(SingleQueuedInvokation, HandleDelayedPeerUpdates);
DeclareRefVar(SingleQueuedInvokation, HandleObservables);

DeclareVar(Adaptive::WindowLayout, AdaptiveWindowLayout);
DeclareVar(Adaptive::ChatLayout, AdaptiveChatLayout);
DeclareVar(bool, AdaptiveForWide);
DeclareRefVar(base::Observable<void>, AdaptiveChanged);

DeclareVar(bool, DialogsModeEnabled);
DeclareVar(Dialogs::Mode, DialogsMode);
DeclareVar(bool, ModerateModeEnabled);

DeclareVar(bool, ScreenIsLocked);

DeclareVar(qint32, DebugLoggingFlags);

constexpr double kDefaultVolume = 0.9;

DeclareVar(double, RememberedSongVolume);
DeclareVar(double, SongVolume);
DeclareRefVar(base::Observable<void>, SongVolumeChanged);
DeclareVar(double, VideoVolume);
DeclareRefVar(base::Observable<void>, VideoVolumeChanged);

// config
DeclareVar(qint32, ChatSizeMax);
DeclareVar(qint32, MegagroupSizeMax);
DeclareVar(qint32, ForwardedCountMax);
DeclareVar(qint32, OnlineUpdatePeriod);
DeclareVar(qint32, OfflineBlurTimeout);
DeclareVar(qint32, OfflineIdleTimeout);
DeclareVar(qint32, OnlineFocusTimeout); // not from config
DeclareVar(qint32, OnlineCloudTimeout);
DeclareVar(qint32, NotifyCloudDelay);
DeclareVar(qint32, NotifyDefaultDelay);
DeclareVar(qint32, ChatBigSize);
DeclareVar(qint32, PushChatPeriod);
DeclareVar(qint32, PushChatLimit);
DeclareVar(qint32, SavedGifsLimit);
DeclareVar(qint32, EditTimeLimit);
DeclareVar(qint32, StickersRecentLimit);
DeclareVar(qint32, StickersFavedLimit);
DeclareVar(qint32, PinnedDialogsCountMax);
DeclareVar(QString, InternalLinksDomain);
DeclareVar(qint32, CallReceiveTimeoutMs);
DeclareVar(qint32, CallRingTimeoutMs);
DeclareVar(qint32, CallConnectTimeoutMs);
DeclareVar(qint32, CallPacketTimeoutMs);
DeclareVar(bool, PhoneCallsEnabled);
DeclareRefVar(base::Observable<void>, PhoneCallsEnabledChanged);

typedef QMap<PeerId, MsgId> HiddenPinnedMessagesMap;
DeclareVar(HiddenPinnedMessagesMap, HiddenPinnedMessages);

typedef std::set<HistoryItem *> PendingItemsMap;
DeclareRefVar(PendingItemsMap, PendingRepaintItems);

DeclareVar(Stickers::Sets, StickerSets);
DeclareVar(Stickers::Order, StickerSetsOrder);
DeclareVar(TimeMs, LastStickersUpdate);
DeclareVar(TimeMs, LastRecentStickersUpdate);
DeclareVar(TimeMs, LastFavedStickersUpdate);
DeclareVar(Stickers::Order, FeaturedStickerSetsOrder);
DeclareVar(int, FeaturedStickerSetsUnreadCount);
DeclareRefVar(base::Observable<void>, FeaturedStickerSetsUnreadCountChanged);
DeclareVar(TimeMs, LastFeaturedStickersUpdate);
DeclareVar(Stickers::Order, ArchivedStickerSetsOrder);

typedef QMap<quint64, QPixmap> CircleMasksMap;
DeclareRefVar(CircleMasksMap, CircleMasks);

DeclareRefVar(base::Observable<void>, SelfChanged);

DeclareVar(bool, AskDownloadPath);
DeclareVar(QString, DownloadPath);
DeclareVar(QByteArray, DownloadPathBookmark);
DeclareRefVar(base::Observable<void>, DownloadPathChanged);

DeclareVar(bool, SoundNotify);
DeclareVar(bool, DesktopNotify);
DeclareVar(bool, RestoreSoundNotifyFromTray);
DeclareVar(bool, IncludeMuted);
DeclareVar(DBINotifyView, NotifyView);
DeclareVar(bool, NativeNotifications);
DeclareVar(int, NotificationsCount);
DeclareVar(Notify::ScreenCorner, NotificationsCorner);
DeclareVar(bool, NotificationsDemoIsShown);

DeclareVar(DBIConnectionType, ConnectionType);
DeclareVar(DBIConnectionType, LastProxyType);
DeclareVar(bool, TryIPv6);
DeclareVar(ProxyData, ConnectionProxy);
DeclareRefVar(base::Observable<void>, ConnectionTypeChanged);

DeclareVar(int, AutoLock);
DeclareVar(bool, LocalPasscode);
DeclareRefVar(base::Observable<void>, LocalPasscodeChanged);

DeclareRefVar(base::Variable<DBIWorkMode>, WorkMode);

DeclareRefVar(base::Observable<HistoryItem *>, ItemRemoved);
DeclareRefVar(base::Observable<void>, UnreadCounterUpdate);
DeclareRefVar(base::Observable<void>, PeerChooseCancel);

} // namespace Global

namespace Adaptive {

inline base::Observable<void> &Changed() {
	return Global::RefAdaptiveChanged();
}

inline bool OneColumn() {
	return Global::AdaptiveWindowLayout() == WindowLayout::OneColumn;
}

inline bool SmallColumn() {
	return Global::AdaptiveWindowLayout() == WindowLayout::SmallColumn;
}

inline bool Normal() {
	return Global::AdaptiveWindowLayout() == WindowLayout::Normal;
}

inline bool ChatNormal() {
	return !Global::AdaptiveForWide() || (Global::AdaptiveChatLayout() == ChatLayout::Normal);
}

inline bool ChatWide() {
	return !ChatNormal();
}

} // namespace Adaptive

namespace DebugLogging {

inline bool FileLoader() {
	return (Global::DebugLoggingFlags() & FileLoaderFlag) != 0;
}

} // namespace DebugLogging
