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

#include <cassert>
#include <cstdint>

void InitFromCommandLine(int argc, char *argv[]);

extern bool gDebug;
inline bool cDebug() {
#if defined _DEBUG
	return true;
#else
	return gDebug;
#endif
}
inline void cSetDebug(bool debug) {
	gDebug = debug;
}

#define DeclareReadSetting(Type, Name) extern Type g##Name; \
inline const Type &c##Name() { \
	return g##Name; \
}

#define DeclareSetting(Type, Name) DeclareReadSetting(Type, Name) \
inline void cSet##Name(const Type &Name) { \
	g##Name = Name; \
}

#define DeclareRefSetting(Type, Name) DeclareSetting(Type, Name) \
inline Type &cRef##Name() { \
	return g##Name; \
}

DeclareSetting(bool, Rtl);
DeclareSetting(Qt::LayoutDirection, LangDir);
inline bool rtl() {
	return cRtl();
}

DeclareReadSetting(QString, Arguments);

DeclareSetting(bool, AlphaVersion);
DeclareSetting(quint64, BetaVersion);
DeclareSetting(quint64, RealBetaVersion);
DeclareSetting(QByteArray, BetaPrivateKey);

DeclareSetting(bool, TestMode);
DeclareSetting(QString, LoggedPhoneNumber);
DeclareSetting(bool, AutoStart);
DeclareSetting(bool, StartMinimized);
DeclareSetting(bool, StartInTray);
DeclareSetting(bool, SendToMenu);
DeclareSetting(bool, UseExternalVideoPlayer);
enum LaunchMode {
	LaunchModeNormal = 0,
	LaunchModeAutoStart,
	LaunchModeFixPrevious,
	LaunchModeCleanup,
};
DeclareReadSetting(LaunchMode, LaunchMode);
DeclareSetting(QString, WorkingDir);
inline void cForceWorkingDir(const QString &newDir) {
	cSetWorkingDir(newDir);
	if (!gWorkingDir.isEmpty()) {
		QDir().mkpath(gWorkingDir);
		QFile::setPermissions(gWorkingDir,
			QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser);
	}

}
DeclareReadSetting(QString, ExeName);
DeclareReadSetting(QString, ExeDir);
DeclareSetting(QString, DialogLastPath);
DeclareSetting(QString, DialogHelperPath);
inline const QString &cDialogHelperPathFinal() {
	return cDialogHelperPath().isEmpty() ? cExeDir() : cDialogHelperPath();
}
DeclareSetting(bool, CtrlEnter);

DeclareSetting(bool, AutoUpdate);

struct TWindowPos {
	TWindowPos() = default;

	qint32 moncrc = 0;
	int maximized = 0;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};
DeclareSetting(TWindowPos, WindowPos);
DeclareSetting(bool, SupportTray);
DeclareSetting(bool, SeenTrayTooltip);
DeclareSetting(bool, RestartingUpdate);
DeclareSetting(bool, Restarting);
DeclareSetting(bool, RestartingToSettings);
DeclareSetting(bool, WriteProtected);
DeclareSetting(qint32, LastUpdateCheck);
DeclareSetting(bool, NoStartUpdate);
DeclareSetting(bool, StartToSettings);
DeclareSetting(bool, ReplaceEmojis);
DeclareSetting(bool, MessageFormatting);
DeclareReadSetting(bool, ManyInstance);

DeclareSetting(QByteArray, LocalSalt);
DeclareSetting(DBIScale, RealScale);
DeclareSetting(DBIScale, ScreenScale);
DeclareSetting(DBIScale, ConfigScale);
DeclareSetting(bool, CompressPastedImage);
DeclareSetting(QString, TimeFormat);

inline void cChangeTimeFormat(const QString &newFormat) {
	if (!newFormat.isEmpty()) cSetTimeFormat(newFormat);
}

inline DBIScale cEvalScale(DBIScale scale) {
	return (scale == dbisAuto) ? cScreenScale() : scale;
}
inline DBIScale cScale() {
	return cEvalScale(cRealScale());
}

template <typename T>
T convertScale(T v) {
	switch (cScale()) {
		case dbisOneAndQuarter: return qRound(double(v) * 1.25 - 0.01);
		case dbisOneAndHalf: return qRound(double(v) * 1.5 - 0.01);
		case dbisTwo: return v * 2;
		case dbisAuto: case dbisOne: return v;
		case dbisScaleCount: assert(false); // temp
	}
	return v;
}

namespace Ui {
namespace Emoji {
class One;
} // namespace Emoji
} // namespace Ui

using EmojiPtr = const Ui::Emoji::One*;

using EmojiPack = QVector<EmojiPtr>;
using RecentEmojiPreloadOldOld = QVector<QPair<quint32, quint16>>;
using RecentEmojiPreloadOld = QVector<QPair<quint64, quint16>>;
using RecentEmojiPreload = QVector<QPair<QString, quint16>>;
using RecentEmojiPack = QVector<QPair<EmojiPtr, quint16>>;
using EmojiColorVariantsOld = QMap<quint32, quint64>;
using EmojiColorVariants = QMap<QString, int>;
DeclareRefSetting(RecentEmojiPack, RecentEmoji);
DeclareSetting(RecentEmojiPreload, RecentEmojiPreload);
DeclareRefSetting(EmojiColorVariants, EmojiVariants);

class DocumentData;
typedef QVector<DocumentData*> StickerPack;

typedef QList<QPair<DocumentData*, qint16> > RecentStickerPackOld;
typedef QVector<QPair<quint64, quint16> > RecentStickerPreload;
typedef QVector<QPair<DocumentData*, quint16> > RecentStickerPack;
DeclareSetting(RecentStickerPreload, RecentStickersPreload);
DeclareRefSetting(RecentStickerPack, RecentStickers);

RecentStickerPack &cGetRecentStickers();

typedef QMap<EmojiPtr, StickerPack> StickersByEmojiMap;

typedef QVector<DocumentData*> SavedGifs;
DeclareRefSetting(SavedGifs, SavedGifs);
DeclareSetting(TimeMs, LastSavedGifsUpdate);

typedef QList<QPair<QString, quint16> > RecentHashtagPack;
DeclareRefSetting(RecentHashtagPack, RecentWriteHashtags);
DeclareSetting(RecentHashtagPack, RecentSearchHashtags);

class UserData;
typedef QVector<UserData*> RecentInlineBots;
DeclareRefSetting(RecentInlineBots, RecentInlineBots);

DeclareSetting(bool, PasswordRecovered);

DeclareSetting(qint32, PasscodeBadTries);
DeclareSetting(TimeMs, PasscodeLastTry);

inline bool passcodeCanTry() {
	if (cPasscodeBadTries() < 3) return true;
	auto dt = getms(true) - cPasscodeLastTry();
	switch (cPasscodeBadTries()) {
	case 3: return dt >= 5000;
	case 4: return dt >= 10000;
	case 5: return dt >= 15000;
	case 6: return dt >= 20000;
	case 7: return dt >= 25000;
	}
	return dt >= 30000;
}

inline void incrementRecentHashtag(RecentHashtagPack &recent, const QString &tag) {
	RecentHashtagPack::iterator i = recent.begin(), e = recent.end();
	for (; i != e; ++i) {
		if (i->first == tag) {
			++i->second;
		if (qAbs(i->second) > 0x4000) {
			for (RecentHashtagPack::iterator j = recent.begin(); j != e; ++j) {
				if (j->second > 1) {
					j->second /= 2;
				} else if (j->second > 0) {
					j->second = 1;
				}
			}
		}
			for (; i != recent.begin(); --i) {
				if (qAbs((i - 1)->second) > qAbs(i->second)) {
					break;
				}
				qSwap(*i, *(i - 1));
			}
			break;
		}
	}
	if (i == e) {
		while (recent.size() >= 64) recent.pop_back();
		recent.push_back(qMakePair(tag, 1));
		for (i = recent.end() - 1; i != recent.begin(); --i) {
			if ((i - 1)->second > i->second) {
				break;
			}
			qSwap(*i, *(i - 1));
		}
	}
}

DeclareSetting(QStringList, SendPaths);
DeclareSetting(QString, StartUrl);

DeclareSetting(bool, Retina);
DeclareSetting(double, RetinaFactor);
DeclareSetting(qint32, IntRetinaFactor);

DeclareReadSetting(DBIPlatform, Platform);
DeclareReadSetting(QString, PlatformString);
DeclareReadSetting(bool, IsElCapitan);
DeclareReadSetting(QUrl, UpdateURL);

DeclareSetting(int, OtherOnline);

class PeerData;
typedef QMap<PeerData*, QDateTime> SavedPeers;
typedef QMultiMap<QDateTime, PeerData*> SavedPeersByTime;
DeclareRefSetting(SavedPeers, SavedPeers);
DeclareRefSetting(SavedPeersByTime, SavedPeersByTime);

typedef QMap<quint64, DBIPeerReportSpamStatus> ReportSpamStatuses;
DeclareRefSetting(ReportSpamStatuses, ReportSpamStatuses);

enum DBIAutoDownloadFlags {
	dbiadNoPrivate = 0x01,
	dbiadNoGroups  = 0x02,
};

DeclareSetting(qint32, AutoDownloadPhoto);
DeclareSetting(qint32, AutoDownloadAudio);
DeclareSetting(qint32, AutoDownloadGif);
DeclareSetting(bool, AutoPlayGif);

void settingsParseArgs(int argc, char *argv[]);
