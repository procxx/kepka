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
#include "stickers.h"

#include "boxes/stickers_box.h"
#include "boxes/confirm_box.h"
#include "lang/lang_keys.h"
#include "apiwrap.h"
#include "storage/localstorage.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "ui/toast/toast.h"
#include "styles/style_chat_helpers.h"

namespace Stickers {
namespace {

constexpr int kReadFeaturedSetsTimeoutMs = 1000;
QPointer<internal::FeaturedReader> FeaturedReaderInstance;

} // namespace

void ApplyArchivedResult(const MTPDmessages_stickerSetInstallResultArchive &d) {
	auto &v = d.vsets.v;
	auto &order = Global::RefStickerSetsOrder();
	Order archived;
	archived.reserve(v.size());
	QMap<quint64, quint64> setsToRequest;
	for_const (auto &stickerSet, v) {
		const MTPDstickerSet *setData = nullptr;
		switch (stickerSet.type()) {
		case mtpc_stickerSetCovered: {
			auto &d = stickerSet.c_stickerSetCovered();
			if (d.vset.type() == mtpc_stickerSet) {
				setData = &d.vset.c_stickerSet();
			}
		} break;
		case mtpc_stickerSetMultiCovered: {
			auto &d = stickerSet.c_stickerSetMultiCovered();
			if (d.vset.type() == mtpc_stickerSet) {
				setData = &d.vset.c_stickerSet();
			}
		} break;
		}
		if (setData) {
			auto set = FeedSet(*setData);
			if (set->stickers.isEmpty()) {
				setsToRequest.insert(set->id, set->access);
			}
			auto index = order.indexOf(set->id);
			if (index >= 0) {
				order.removeAt(index);
			}
			archived.push_back(set->id);
		}
	}
	if (!setsToRequest.isEmpty()) {
		for (auto i = setsToRequest.cbegin(), e = setsToRequest.cend(); i != e; ++i) {
			Auth().api().scheduleStickerSetRequest(i.key(), i.value());
		}
		Auth().api().requestStickerSets();
	}
	Local::writeInstalledStickers();
	Local::writeArchivedStickers();

	Ui::Toast::Config toast;
	toast.text = lang(lng_stickers_packs_archived);
	toast.maxWidth = st::stickersToastMaxWidth;
	toast.padding = st::stickersToastPadding;
	Ui::Toast::Show(toast);
//	Ui::show(Box<StickersBox>(archived), KeepOtherLayers);

	Auth().data().stickersUpdated().notify(true);
}

// For testing: Just apply random subset or your sticker sets as archived.
bool ApplyArchivedResultFake() {
	auto sets = QVector<MTPStickerSetCovered>();
	for (auto &set : Global::RefStickerSets()) {
		if ((set.flags & MTPDstickerSet::Flag::f_installed) && !(set.flags & MTPDstickerSet_ClientFlag::f_special)) {
			if (rand_value<quint32>() % 128 < 64) {
				auto data = MTP_stickerSet(MTP_flags(set.flags | MTPDstickerSet::Flag::f_archived), MTP_long(set.id), MTP_long(set.access), MTP_string(set.title), MTP_string(set.shortName), MTP_int(set.count), MTP_int(set.hash));
				sets.push_back(MTP_stickerSetCovered(data, MTP_documentEmpty(MTP_long(0))));
			}
		}
	}
	if (sets.size() > 3) sets = sets.mid(0, 3);
	auto fakeResult = MTP_messages_stickerSetInstallResultArchive(MTP_vector<MTPStickerSetCovered>(sets));
	ApplyArchivedResult(fakeResult.c_messages_stickerSetInstallResultArchive());
	return true;
}

void InstallLocally(quint64 setId) {
	auto &sets = Global::RefStickerSets();
	auto it = sets.find(setId);
	if (it == sets.end()) {
		return;
	}

	auto flags = it->flags;
	it->flags &= ~(MTPDstickerSet::Flag::f_archived | MTPDstickerSet_ClientFlag::f_unread);
	it->flags |= MTPDstickerSet::Flag::f_installed;
	auto changedFlags = flags ^ it->flags;

	auto &order = Global::RefStickerSetsOrder();
	int insertAtIndex = 0, currentIndex = order.indexOf(setId);
	if (currentIndex != insertAtIndex) {
		if (currentIndex > 0) {
			order.removeAt(currentIndex);
		}
		order.insert(insertAtIndex, setId);
	}

	auto custom = sets.find(CustomSetId);
	if (custom != sets.cend()) {
		for_const (auto sticker, it->stickers) {
			int removeIndex = custom->stickers.indexOf(sticker);
			if (removeIndex >= 0) custom->stickers.removeAt(removeIndex);
		}
		if (custom->stickers.isEmpty()) {
			sets.erase(custom);
		}
	}
	Local::writeInstalledStickers();
	if (changedFlags & MTPDstickerSet_ClientFlag::f_unread) Local::writeFeaturedStickers();
	if (changedFlags & MTPDstickerSet::Flag::f_archived) {
		auto index = Global::RefArchivedStickerSetsOrder().indexOf(setId);
		if (index >= 0) {
			Global::RefArchivedStickerSetsOrder().removeAt(index);
			Local::writeArchivedStickers();
		}
	}
	Auth().data().stickersUpdated().notify(true);
}

void UndoInstallLocally(quint64 setId) {
	auto &sets = Global::RefStickerSets();
	auto it = sets.find(setId);
	if (it == sets.end()) {
		return;
	}

	it->flags &= ~MTPDstickerSet::Flag::f_installed;

	auto &order = Global::RefStickerSetsOrder();
	int currentIndex = order.indexOf(setId);
	if (currentIndex >= 0) {
		order.removeAt(currentIndex);
	}

	Local::writeInstalledStickers();
	Auth().data().stickersUpdated().notify(true);

	Ui::show(Box<InformBox>(lang(lng_stickers_not_found)), KeepOtherLayers);
}

void MarkFeaturedAsRead(quint64 setId) {
	if (!FeaturedReaderInstance) {
		if (auto main = App::main()) {
			FeaturedReaderInstance = object_ptr<internal::FeaturedReader>(main);
		} else {
			return;
		}
	}
	FeaturedReaderInstance->scheduleRead(setId);
}

bool IsFaved(not_null<DocumentData*> document) {
	auto it = Global::StickerSets().constFind(FavedSetId);
	return (it != Global::StickerSets().cend()) && it->stickers.contains(document);
}

void CheckFavedLimit(Set &set) {
	if (set.stickers.size() <= Global::StickersFavedLimit()) {
		return;
	}
	auto removing = set.stickers.back();
	set.stickers.pop_back();
	for (auto i = set.emoji.begin(); i != set.emoji.end();) {
		auto index = i->indexOf(removing);
		if (index >= 0) {
			i->removeAt(index);
			if (i->empty()) {
				i = set.emoji.erase(i);
				continue;
			}
		}
		++i;
	}
}

void PushFavedToFront(
		Set &set,
		not_null<DocumentData*> document,
		const std::vector<not_null<EmojiPtr>> &emojiList) {
	set.stickers.push_front(document);
	for (auto emoji : emojiList) {
		set.emoji[emoji].push_front(document);
	}
	CheckFavedLimit(set);
}

void MoveFavedToFront(Set &set, int index) {
	Expects(index > 0 && index < set.stickers.size());
	auto document = set.stickers[index];
	while (index-- != 0) {
		set.stickers[index + 1] = set.stickers[index];
	}
	set.stickers[0] = document;
	for (auto &list : set.emoji) {
		auto index = list.indexOf(document);
		if (index > 0) {
			while (index-- != 0) {
				list[index + 1] = list[index];
			}
			list[0] = document;
		}
	}
}

void RequestSetToPushFaved(not_null<DocumentData*> document);

void SetIsFaved(not_null<DocumentData*> document, base::optional<std::vector<not_null<EmojiPtr>>> emojiList = base::none) {
	auto &sets = Global::RefStickerSets();
	auto it = sets.find(FavedSetId);
	if (it == sets.end()) {
		it = sets.insert(FavedSetId, Set(FavedSetId, 0, lang(lng_faved_stickers), QString(), 0, 0, MTPDstickerSet_ClientFlag::f_special | 0));
	}
	auto index = it->stickers.indexOf(document);
	if (index == 0) {
		return;
	}
	if (index > 0) {
		MoveFavedToFront(*it, index);
	} else if (emojiList) {
		PushFavedToFront(*it, document, *emojiList);
	} else if (auto list = GetEmojiListFromSet(document)) {
		PushFavedToFront(*it, document, *list);
	} else {
		RequestSetToPushFaved(document);
		return;
	}
	Local::writeFavedStickers();
	Auth().data().stickersUpdated().notify(true);
	App::main()->onStickersInstalled(FavedSetId);
}

void RequestSetToPushFaved(not_null<DocumentData*> document) {
	auto addAnyway = [document](std::vector<not_null<EmojiPtr>> list) {
		if (list.empty()) {
			if (auto sticker = document->sticker()) {
				if (auto emoji = Ui::Emoji::Find(sticker->alt)) {
					list.push_back(emoji);
				}
			}
		}
		SetIsFaved(document, std::move(list));
	};
	MTP::send(MTPmessages_GetStickerSet(document->sticker()->set), rpcDone([document, addAnyway](const MTPmessages_StickerSet &result) {
		Expects(result.type() == mtpc_messages_stickerSet);
		auto list = std::vector<not_null<EmojiPtr>>();
		auto &d = result.c_messages_stickerSet();
		list.reserve(d.vpacks.v.size());
		for_const (auto &mtpPack, d.vpacks.v) {
			auto &pack = mtpPack.c_stickerPack();
			for_const (auto &documentId, pack.vdocuments.v) {
				if (documentId.v == document->id) {
					if (auto emoji = Ui::Emoji::Find(qs(mtpPack.c_stickerPack().vemoticon))) {
						list.push_back(emoji);
					}
					break;
				}
			}
		}
		addAnyway(std::move(list));
	}), rpcFail([addAnyway](const RPCError &error) {
		if (MTP::isDefaultHandledError(error)) {
			return false;
		}
		// Perhaps this is a deleted sticker pack. Add anyway.
		addAnyway({});
		return true;
	}));
}

void SetIsNotFaved(not_null<DocumentData*> document) {
	auto &sets = Global::RefStickerSets();
	auto it = sets.find(FavedSetId);
	if (it == sets.end()) {
		return;
	}
	auto index = it->stickers.indexOf(document);
	if (index < 0) {
		return;
	}
	it->stickers.removeAt(index);
	for (auto i = it->emoji.begin(); i != it->emoji.end();) {
		auto index = i->indexOf(document);
		if (index >= 0) {
			i->removeAt(index);
			if (i->empty()) {
				i = it->emoji.erase(i);
				continue;
			}
		}
		++i;
	}
	if (it->stickers.empty()) {
		sets.erase(it);
	}
	Local::writeFavedStickers();
	Auth().data().stickersUpdated().notify(true);
}

void SetFaved(not_null<DocumentData*> document, bool faved) {
	if (faved) {
		SetIsFaved(document);
	} else {
		SetIsNotFaved(document);
	}
}

void SetsReceived(const QVector<MTPStickerSet> &data, qint32 hash) {
	auto &setsOrder = Global::RefStickerSetsOrder();
	setsOrder.clear();

	auto &sets = Global::RefStickerSets();
	QMap<quint64, quint64> setsToRequest;
	for (auto &set : sets) {
		if (!(set.flags & MTPDstickerSet::Flag::f_archived)) {
			set.flags &= ~MTPDstickerSet::Flag::f_installed; // mark for removing
		}
	}
	for_const (auto &setData, data) {
		if (setData.type() == mtpc_stickerSet) {
			auto set = FeedSet(setData.c_stickerSet());
			if (!(set->flags & MTPDstickerSet::Flag::f_archived) || (set->flags & MTPDstickerSet::Flag::f_official)) {
				setsOrder.push_back(set->id);
				if (set->stickers.isEmpty() || (set->flags & MTPDstickerSet_ClientFlag::f_not_loaded)) {
					setsToRequest.insert(set->id, set->access);
				}
			}
		}
	}
	auto writeRecent = false;
	auto &recent = cGetRecentStickers();
	for (auto it = sets.begin(), e = sets.end(); it != e;) {
		bool installed = (it->flags & MTPDstickerSet::Flag::f_installed);
		bool featured = (it->flags & MTPDstickerSet_ClientFlag::f_featured);
		bool special = (it->flags & MTPDstickerSet_ClientFlag::f_special);
		bool archived = (it->flags & MTPDstickerSet::Flag::f_archived);
		if (!installed) { // remove not mine sets from recent stickers
			for (auto i = recent.begin(); i != recent.cend();) {
				if (it->stickers.indexOf(i->first) >= 0) {
					i = recent.erase(i);
					writeRecent = true;
				} else {
					++i;
				}
			}
		}
		if (installed || featured || special || archived) {
			++it;
		} else {
			it = sets.erase(it);
		}
	}

	if (!setsToRequest.isEmpty()) {
		auto &api = Auth().api();
		for (auto i = setsToRequest.cbegin(), e = setsToRequest.cend(); i != e; ++i) {
			api.scheduleStickerSetRequest(i.key(), i.value());
		}
		api.requestStickerSets();
	}

	Local::writeInstalledStickers();
	if (writeRecent) Local::writeUserSettings();

	if (Local::countStickersHash() != hash) {
		LOG(("API Error: received stickers hash %1 while counted hash is %2").arg(hash).arg(Local::countStickersHash()));
	}

	Auth().data().stickersUpdated().notify(true);
}

void SetPackAndEmoji(Set &set, StickerPack &&pack, const QVector<MTPStickerPack> &packs) {
	set.stickers = std::move(pack);
	set.emoji.clear();
	for_const (auto &mtpPack, packs) {
		Assert(mtpPack.type() == mtpc_stickerPack);
		auto &pack = mtpPack.c_stickerPack();
		if (auto emoji = Ui::Emoji::Find(qs(pack.vemoticon))) {
			emoji = emoji->original();
			auto &stickers = pack.vdocuments.v;

			auto p = StickerPack();
			p.reserve(stickers.size());
			for (auto j = 0, c = stickers.size(); j != c; ++j) {
				auto document = App::document(stickers[j].v);
				if (!document || !document->sticker()) continue;

				p.push_back(document);
			}
			set.emoji.insert(emoji, p);
		}
	}
}

void SpecialSetReceived(quint64 setId, const QString &setTitle, const QVector<MTPDocument> &items, qint32 hash, const QVector<MTPStickerPack> &packs) {
	auto &sets = Global::RefStickerSets();
	auto it = sets.find(setId);

	auto &d_docs = items;
	if (d_docs.isEmpty()) {
		if (it != sets.cend()) {
			sets.erase(it);
		}
	} else {
		if (it == sets.cend()) {
			it = sets.insert(setId, Set(setId, 0, setTitle, QString(), 0, 0, MTPDstickerSet_ClientFlag::f_special | 0));
		} else {
			it->title = setTitle;
		}
		it->hash = hash;

		auto custom = sets.find(CustomSetId);
		auto pack = StickerPack();
		pack.reserve(d_docs.size());
		for_const (auto &mtpDocument, d_docs) {
			auto document = App::feedDocument(mtpDocument);
			if (!document || !document->sticker()) continue;

			pack.push_back(document);
			if (custom != sets.cend()) {
				auto index = custom->stickers.indexOf(document);
				if (index >= 0) {
					custom->stickers.removeAt(index);
				}
			}
		}
		if (custom != sets.cend() && custom->stickers.isEmpty()) {
			sets.erase(custom);
			custom = sets.end();
		}

		auto writeRecent = false;
		auto &recent = cGetRecentStickers();
		for (auto i = recent.begin(); i != recent.cend();) {
			if (it->stickers.indexOf(i->first) >= 0 && pack.indexOf(i->first) < 0) {
				i = recent.erase(i);
				writeRecent = true;
			} else {
				++i;
			}
		}

		if (pack.isEmpty()) {
			sets.erase(it);
		} else {
			SetPackAndEmoji(*it, std::move(pack), packs);
		}

		if (writeRecent) {
			Local::writeUserSettings();
		}
	}

	switch (setId) {
	case CloudRecentSetId: {
		if (Local::countRecentStickersHash() != hash) {
			LOG(("API Error: received recent stickers hash %1 while counted hash is %2").arg(hash).arg(Local::countRecentStickersHash()));
		}
		Local::writeRecentStickers();
	} break;
	case FavedSetId: {
		if (Local::countFavedStickersHash() != hash) {
			LOG(("API Error: received faved stickers hash %1 while counted hash is %2").arg(hash).arg(Local::countFavedStickersHash()));
		}
		Local::writeFavedStickers();
	} break;
	default: Unexpected("setId in SpecialSetReceived()");
	}

	Auth().data().stickersUpdated().notify(true);
}

void FeaturedSetsReceived(const QVector<MTPStickerSetCovered> &data, const QVector<MTPlong> &unread, qint32 hash) {
	OrderedSet<quint64> unreadMap;
	for_const (auto &unreadSetId, unread) {
		unreadMap.insert(unreadSetId.v);
	}

	auto &setsOrder = Global::RefFeaturedStickerSetsOrder();
	setsOrder.clear();

	auto &sets = Global::RefStickerSets();
	QMap<quint64, quint64> setsToRequest;
	for (auto &set : sets) {
		set.flags &= ~MTPDstickerSet_ClientFlag::f_featured; // mark for removing
	}
	for (int i = 0, l = data.size(); i != l; ++i) {
		auto &setData = data[i];
		const MTPDstickerSet *set = nullptr;
		switch (setData.type()) {
		case mtpc_stickerSetCovered: {
			auto &d = setData.c_stickerSetCovered();
			if (d.vset.type() == mtpc_stickerSet) {
				set = &d.vset.c_stickerSet();
			}
		} break;
		case mtpc_stickerSetMultiCovered: {
			auto &d = setData.c_stickerSetMultiCovered();
			if (d.vset.type() == mtpc_stickerSet) {
				set = &d.vset.c_stickerSet();
			}
		} break;
		}

		if (set) {
			auto it = sets.find(set->vid.v);
			auto title = GetSetTitle(*set);
			if (it == sets.cend()) {
				auto setClientFlags = MTPDstickerSet_ClientFlag::f_featured | MTPDstickerSet_ClientFlag::f_not_loaded;
				if (unreadMap.contains(set->vid.v)) {
					setClientFlags |= MTPDstickerSet_ClientFlag::f_unread;
				}
				it = sets.insert(set->vid.v, Set(set->vid.v, set->vaccess_hash.v, title, qs(set->vshort_name), set->vcount.v, set->vhash.v, set->vflags.v | setClientFlags));
			} else {
				it->access = set->vaccess_hash.v;
				it->title = title;
				it->shortName = qs(set->vshort_name);
				auto clientFlags = it->flags & (MTPDstickerSet_ClientFlag::f_featured | MTPDstickerSet_ClientFlag::f_unread | MTPDstickerSet_ClientFlag::f_not_loaded | MTPDstickerSet_ClientFlag::f_special);
				it->flags = set->vflags.v | clientFlags;
				it->flags |= MTPDstickerSet_ClientFlag::f_featured;
				if (unreadMap.contains(it->id)) {
					it->flags |= MTPDstickerSet_ClientFlag::f_unread;
				} else {
					it->flags &= ~MTPDstickerSet_ClientFlag::f_unread;
				}
				if (it->count != set->vcount.v || it->hash != set->vhash.v || it->emoji.isEmpty()) {
					it->count = set->vcount.v;
					it->hash = set->vhash.v;
					it->flags |= MTPDstickerSet_ClientFlag::f_not_loaded; // need to request this set
				}
			}
			setsOrder.push_back(set->vid.v);
			if (it->stickers.isEmpty() || (it->flags & MTPDstickerSet_ClientFlag::f_not_loaded)) {
				setsToRequest.insert(set->vid.v, set->vaccess_hash.v);
			}
		}
	}

	auto unreadCount = 0;
	for (auto it = sets.begin(), e = sets.end(); it != e;) {
		bool installed = (it->flags & MTPDstickerSet::Flag::f_installed);
		bool featured = (it->flags & MTPDstickerSet_ClientFlag::f_featured);
		bool special = (it->flags & MTPDstickerSet_ClientFlag::f_special);
		bool archived = (it->flags & MTPDstickerSet::Flag::f_archived);
		if (installed || featured || special || archived) {
			if (featured && (it->flags & MTPDstickerSet_ClientFlag::f_unread)) {
				++unreadCount;
			}
			++it;
		} else {
			it = sets.erase(it);
		}
	}
	if (Global::FeaturedStickerSetsUnreadCount() != unreadCount) {
		Global::SetFeaturedStickerSetsUnreadCount(unreadCount);
		Global::RefFeaturedStickerSetsUnreadCountChanged().notify();
	}

	if (Local::countFeaturedStickersHash() != hash) {
		LOG(("API Error: received featured stickers hash %1 while counted hash is %2").arg(hash).arg(Local::countFeaturedStickersHash()));
	}

	if (!setsToRequest.isEmpty()) {
		auto &api = Auth().api();
		for (auto i = setsToRequest.cbegin(), e = setsToRequest.cend(); i != e; ++i) {
			api.scheduleStickerSetRequest(i.key(), i.value());
		}
		api.requestStickerSets();
	}

	Local::writeFeaturedStickers();

	Auth().data().stickersUpdated().notify(true);
}

void GifsReceived(const QVector<MTPDocument> &items, qint32 hash) {
	auto &saved = cRefSavedGifs();
	saved.clear();

	saved.reserve(items.size());
	for_const (auto &gif, items) {
		auto document = App::feedDocument(gif);
		if (!document || !document->isGifv()) {
			LOG(("API Error: bad document returned in HistoryWidget::savedGifsGot!"));
			continue;
		}

		saved.push_back(document);
	}
	if (Local::countSavedGifsHash() != hash) {
		LOG(("API Error: received saved gifs hash %1 while counted hash is %2").arg(hash).arg(Local::countSavedGifsHash()));
	}

	Local::writeSavedGifs();

	Auth().data().savedGifsUpdated().notify();
}

StickerPack GetListByEmoji(not_null<EmojiPtr> emoji) {
	auto original = emoji->original();
	auto result = StickerPack();
	auto setsToRequest = QMap<quint64, quint64>();
	auto &sets = Global::RefStickerSets();

	auto faved = StickerPack();
	auto favedIt = sets.find(Stickers::FavedSetId);
	if (favedIt != sets.cend()) {
		auto i = favedIt->emoji.constFind(original);
		if (i != favedIt->emoji.cend()) {
			faved = *i;
			result = faved;
		}
	}
	auto &order = Global::StickerSetsOrder();
	for (auto i = 0, l = order.size(); i != l; ++i) {
		auto it = sets.find(order[i]);
		if (it != sets.cend()) {
			if (it->emoji.isEmpty()) {
				setsToRequest.insert(it->id, it->access);
				it->flags |= MTPDstickerSet_ClientFlag::f_not_loaded;
			} else if (!(it->flags & MTPDstickerSet::Flag::f_archived)) {
				auto i = it->emoji.constFind(original);
				if (i != it->emoji.cend()) {
					result.reserve(result.size() + i->size());
					for_const (auto sticker, *i) {
						if (!faved.contains(sticker)) {
							result.push_back(sticker);
						}
					}
				}
			}
		}
	}
	if (!setsToRequest.isEmpty()) {
		for (auto i = setsToRequest.cbegin(), e = setsToRequest.cend(); i != e; ++i) {
			Auth().api().scheduleStickerSetRequest(i.key(), i.value());
		}
		Auth().api().requestStickerSets();
	}
	return result;
}

base::optional<std::vector<not_null<EmojiPtr>>> GetEmojiListFromSet(
		not_null<DocumentData*> document) {
	if (auto sticker = document->sticker()) {
		auto &inputSet = sticker->set;
		if (inputSet.type() != mtpc_inputStickerSetID) {
			return base::none;
		}
		auto &sets = Global::StickerSets();
		auto it = sets.constFind(inputSet.c_inputStickerSetID().vid.v);
		if (it == sets.cend()) {
			return base::none;
		}
		auto result = std::vector<not_null<EmojiPtr>>();
		for (auto i = it->emoji.cbegin(), e = it->emoji.cend(); i != e; ++i) {
			if (i->contains(document)) {
				result.push_back(i.key());
			}
		}
		if (result.empty()) {
			return base::none;
		}
		return std::move(result);
	}
	return base::none;
}

Set *FeedSet(const MTPDstickerSet &set) {
	auto &sets = Global::RefStickerSets();
	auto it = sets.find(set.vid.v);
	auto title = GetSetTitle(set);
	auto flags = MTPDstickerSet::Flags(0);
	if (it == sets.cend()) {
		it = sets.insert(set.vid.v, Stickers::Set(set.vid.v, set.vaccess_hash.v, title, qs(set.vshort_name), set.vcount.v, set.vhash.v, set.vflags.v | MTPDstickerSet_ClientFlag::f_not_loaded));
	} else {
		it->access = set.vaccess_hash.v;
		it->title = title;
		it->shortName = qs(set.vshort_name);
		flags = it->flags;
		auto clientFlags = it->flags & (MTPDstickerSet_ClientFlag::f_featured | MTPDstickerSet_ClientFlag::f_unread | MTPDstickerSet_ClientFlag::f_not_loaded | MTPDstickerSet_ClientFlag::f_special);
		it->flags = set.vflags.v | clientFlags;
		if (it->count != set.vcount.v || it->hash != set.vhash.v || it->emoji.isEmpty()) {
			it->count = set.vcount.v;
			it->hash = set.vhash.v;
			it->flags |= MTPDstickerSet_ClientFlag::f_not_loaded; // need to request this set
		}
	}
	auto changedFlags = (flags ^ it->flags);
	if (changedFlags & MTPDstickerSet::Flag::f_archived) {
		auto index = Global::ArchivedStickerSetsOrder().indexOf(it->id);
		if (it->flags & MTPDstickerSet::Flag::f_archived) {
			if (index < 0) {
				Global::RefArchivedStickerSetsOrder().push_front(it->id);
			}
		} else if (index >= 0) {
			Global::RefArchivedStickerSetsOrder().removeAt(index);
		}
	}
	return &it.value();
}

Set *FeedSetFull(const MTPmessages_StickerSet &data) {
	Expects(data.type() == mtpc_messages_stickerSet);
	Expects(data.c_messages_stickerSet().vset.type() == mtpc_stickerSet);
	auto &d = data.c_messages_stickerSet();
	auto set = FeedSet(d.vset.c_stickerSet());

	set->flags &= ~MTPDstickerSet_ClientFlag::f_not_loaded;

	auto &sets = Global::RefStickerSets();
	auto &d_docs = d.vdocuments.v;
	auto custom = sets.find(Stickers::CustomSetId);

	auto pack = StickerPack();
	pack.reserve(d_docs.size());
	for (auto i = 0, l = d_docs.size(); i != l; ++i) {
		auto doc = App::feedDocument(d_docs.at(i));
		if (!doc || !doc->sticker()) continue;

		pack.push_back(doc);
		if (custom != sets.cend()) {
			auto index = custom->stickers.indexOf(doc);
			if (index >= 0) {
				custom->stickers.removeAt(index);
			}
		}
	}
	if (custom != sets.cend() && custom->stickers.isEmpty()) {
		sets.erase(custom);
		custom = sets.end();
	}

	auto writeRecent = false;
	auto &recent = cGetRecentStickers();
	for (auto i = recent.begin(); i != recent.cend();) {
		if (set->stickers.indexOf(i->first) >= 0 && pack.indexOf(i->first) < 0) {
			i = recent.erase(i);
			writeRecent = true;
		} else {
			++i;
		}
	}

	if (pack.isEmpty()) {
		int removeIndex = Global::StickerSetsOrder().indexOf(set->id);
		if (removeIndex >= 0) Global::RefStickerSetsOrder().removeAt(removeIndex);
		sets.remove(set->id);
		set = nullptr;
	} else {
		set->stickers = pack;
		set->emoji.clear();
		auto &v = d.vpacks.v;
		for (auto i = 0, l = v.size(); i != l; ++i) {
			if (v[i].type() != mtpc_stickerPack) continue;

			auto &pack = v[i].c_stickerPack();
			if (auto emoji = Ui::Emoji::Find(qs(pack.vemoticon))) {
				emoji = emoji->original();
				auto &stickers = pack.vdocuments.v;

				StickerPack p;
				p.reserve(stickers.size());
				for (auto j = 0, c = stickers.size(); j != c; ++j) {
					auto doc = App::document(stickers[j].v);
					if (!doc || !doc->sticker()) continue;

					p.push_back(doc);
				}
				set->emoji.insert(emoji, p);
			}
		}
	}

	if (writeRecent) {
		Local::writeUserSettings();
	}

	if (set) {
		if (set->flags & MTPDstickerSet::Flag::f_installed) {
			if (!(set->flags & MTPDstickerSet::Flag::f_archived)) {
				Local::writeInstalledStickers();
			}
		}
		if (set->flags & MTPDstickerSet_ClientFlag::f_featured) {
			Local::writeFeaturedStickers();
		}
	}

	Auth().data().stickersUpdated().notify(true);

	return set;
}

QString GetSetTitle(const MTPDstickerSet &s) {
	auto title = qs(s.vtitle);
	if ((s.vflags.v & MTPDstickerSet::Flag::f_official) && !title.compare(qstr("Great Minds"), Qt::CaseInsensitive)) {
		return lang(lng_stickers_default_set);
	}
	return title;
}

namespace internal {

FeaturedReader::FeaturedReader(QObject *parent) : QObject(parent)
, _timer(this) {
	_timer->setTimeoutHandler([this] { readSets(); });
}

void FeaturedReader::scheduleRead(quint64 setId) {
	if (!_setIds.contains(setId)) {
		_setIds.insert(setId);
		_timer->start(kReadFeaturedSetsTimeoutMs);
	}
}

void FeaturedReader::readSets() {
	auto &sets = Global::RefStickerSets();
	auto count = Global::FeaturedStickerSetsUnreadCount();
	QVector<MTPlong> wrappedIds;
	wrappedIds.reserve(_setIds.size());
	for_const (auto setId, _setIds) {
		auto it = sets.find(setId);
		if (it != sets.cend()) {
			it->flags &= ~MTPDstickerSet_ClientFlag::f_unread;
			wrappedIds.append(MTP_long(setId));
			if (count) {
				--count;
			}
		}
	}
	_setIds.clear();

	if (!wrappedIds.empty()) {
		request(MTPmessages_ReadFeaturedStickers(MTP_vector<MTPlong>(wrappedIds))).done([](const MTPBool &result) {
			Local::writeFeaturedStickers();
			if (AuthSession::Exists()) {
				Auth().data().stickersUpdated().notify(true);
			}
		}).send();

		if (Global::FeaturedStickerSetsUnreadCount() != count) {
			Global::SetFeaturedStickerSetsUnreadCount(count);
			Global::RefFeaturedStickerSetsUnreadCountChanged().notify();
		}
	}
}

} // namespace internal
} // namespace Stickers
