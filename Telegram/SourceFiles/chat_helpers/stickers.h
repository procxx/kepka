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

#include "mtproto/sender.h"

namespace Stickers {

constexpr auto kPanelPerRow = 5;

void ApplyArchivedResult(const MTPDmessages_stickerSetInstallResultArchive &d);
bool ApplyArchivedResultFake(); // For testing.
void InstallLocally(quint64 setId);
void UndoInstallLocally(quint64 setId);
void MarkFeaturedAsRead(quint64 setId);
bool IsFaved(not_null<DocumentData *> document);
void SetFaved(not_null<DocumentData *> document, bool faved);

void SetsReceived(const QVector<MTPStickerSet> &data, qint32 hash);
void SpecialSetReceived(quint64 setId, const QString &setTitle, const QVector<MTPDocument> &items, qint32 hash,
                        const QVector<MTPStickerPack> &packs = QVector<MTPStickerPack>());
void FeaturedSetsReceived(const QVector<MTPStickerSetCovered> &data, const QVector<MTPlong> &unread, qint32 hash);
void GifsReceived(const QVector<MTPDocument> &items, qint32 hash);

StickerPack GetListByEmoji(not_null<EmojiPtr> emoji);
base::optional<std::vector<not_null<EmojiPtr>>> GetEmojiListFromSet(not_null<DocumentData *> document);

Set *FeedSet(const MTPDstickerSet &data);
Set *FeedSetFull(const MTPmessages_StickerSet &data);

QString GetSetTitle(const MTPDstickerSet &s);

namespace internal {

class FeaturedReader : public QObject, private MTP::Sender {
public:
	FeaturedReader(QObject *parent);
	void scheduleRead(quint64 setId);

private:
	void readSets();

	object_ptr<SingleTimer> _timer;
	std::set<quint64> _setIds;
};

} // namespace internal
} // namespace Stickers
