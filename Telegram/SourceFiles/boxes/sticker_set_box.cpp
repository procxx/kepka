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
#include "boxes/sticker_set_box.h"

#include "lang/lang_keys.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "chat_helpers/stickers.h"
#include "boxes/confirm_box.h"
#include "apiwrap.h"
#include "storage/localstorage.h"
#include "dialogs/dialogs_layout.h"
#include "styles/style_boxes.h"
#include "styles/style_chat_helpers.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/scroll_area.h"
#include "auth_session.h"
#include "messenger.h"

namespace {

constexpr auto kStickersPanelPerRow = Stickers::kPanelPerRow;

} // namespace

StickerSetBox::StickerSetBox(QWidget*, const MTPInputStickerSet &set)
: _set(set) {
}

void StickerSetBox::prepare() {
	setTitle(langFactory(lng_contacts_loading));

	_inner = setInnerWidget(object_ptr<Inner>(this, _set), st::stickersScroll);
	subscribe(Auth().data().stickersUpdated(), [this] { updateButtons(); });

	setDimensions(st::boxWideWidth, st::stickersMaxHeight);

	onUpdateButtons();

	connect(_inner, SIGNAL(updateButtons()), this, SLOT(onUpdateButtons()));
	connect(_inner, SIGNAL(installed(quint64)), this, SLOT(onInstalled(quint64)));
}

void StickerSetBox::onInstalled(quint64 setId) {
	emit installed(setId);
	closeBox();
}

void StickerSetBox::onAddStickers() {
	_inner->install();
}

void StickerSetBox::onShareStickers() {
	auto url = Messenger::Instance().createInternalLinkFull(qsl("addstickers/") + _inner->shortName());
	QApplication::clipboard()->setText(url);
	Ui::show(Box<InformBox>(lang(lng_stickers_copied)));
}

void StickerSetBox::onUpdateButtons() {
	setTitle(_inner->title());
	updateButtons();
}

void StickerSetBox::updateButtons() {
	clearButtons();
	if (_inner->loaded()) {
		if (_inner->notInstalled()) {
			addButton(langFactory(lng_stickers_add_pack), [this] { onAddStickers(); });
			addButton(langFactory(lng_cancel), [this] { closeBox(); });
		} else if (_inner->official()) {
			addButton(langFactory(lng_about_done), [this] { closeBox(); });
		} else {
			addButton(langFactory(lng_stickers_share_pack), [this] { onShareStickers(); });
			addButton(langFactory(lng_cancel), [this] { closeBox(); });
		}
	} else {
		addButton(langFactory(lng_cancel), [this] { closeBox(); });
	}
	update();
}

void StickerSetBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);
	_inner->resize(width(), _inner->height());
}

StickerSetBox::Inner::Inner(QWidget *parent, const MTPInputStickerSet &set) : TWidget(parent)
, _input(set) {
	switch (set.type()) {
	case mtpc_inputStickerSetID: _setId = set.c_inputStickerSetID().vid.v; _setAccess = set.c_inputStickerSetID().vaccess_hash.v; break;
	case mtpc_inputStickerSetShortName: _setShortName = qs(set.c_inputStickerSetShortName().vshort_name); break;
	}
	MTP::send(MTPmessages_GetStickerSet(_input), rpcDone(&Inner::gotSet), rpcFail(&Inner::failedSet));
	Auth().api().updateStickers();

	subscribe(Auth().downloaderTaskFinished(), [this] { update(); });

	setMouseTracking(true);

	_previewTimer.setSingleShot(true);
	connect(&_previewTimer, SIGNAL(timeout()), this, SLOT(onPreview()));
}

void StickerSetBox::Inner::gotSet(const MTPmessages_StickerSet &set) {
	_pack.clear();
	_emoji.clear();
	_packOvers.clear();
	_selected = -1;
	setCursor(style::cur_default);
	if (set.type() == mtpc_messages_stickerSet) {
		auto &d = set.c_messages_stickerSet();
		auto &v = d.vdocuments.v;
		_pack.reserve(v.size());
		_packOvers.reserve(v.size());
		for (int i = 0, l = v.size(); i < l; ++i) {
			auto doc = App::feedDocument(v.at(i));
			if (!doc || !doc->sticker()) continue;

			_pack.push_back(doc);
			_packOvers.push_back(Animation());
		}
		auto &packs = d.vpacks.v;
		for (auto i = 0, l = packs.size(); i != l; ++i) {
			if (packs.at(i).type() != mtpc_stickerPack) continue;
			auto &pack = packs.at(i).c_stickerPack();
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
				_emoji.insert(emoji, p);
			}
		}
		if (d.vset.type() == mtpc_stickerSet) {
			auto &s = d.vset.c_stickerSet();
			_setTitle = Stickers::GetSetTitle(s);
			_setShortName = qs(s.vshort_name);
			_setId = s.vid.v;
			_setAccess = s.vaccess_hash.v;
			_setCount = s.vcount.v;
			_setHash = s.vhash.v;
			_setFlags = s.vflags.v;
			auto &sets = Global::RefStickerSets();
			auto it = sets.find(_setId);
			if (it != sets.cend()) {
				auto clientFlags = it->flags & (MTPDstickerSet_ClientFlag::f_featured | MTPDstickerSet_ClientFlag::f_not_loaded | MTPDstickerSet_ClientFlag::f_unread | MTPDstickerSet_ClientFlag::f_special);
				_setFlags |= clientFlags;
				it->flags = _setFlags;
				it->stickers = _pack;
				it->emoji = _emoji;
			}
		}
	}

	if (_pack.isEmpty()) {
		Ui::show(Box<InformBox>(lang(lng_stickers_not_found)));
	} else {
		qint32 rows = _pack.size() / kStickersPanelPerRow + ((_pack.size() % kStickersPanelPerRow) ? 1 : 0);
		resize(st::stickersPadding.left() + kStickersPanelPerRow * st::stickersSize.width(), st::stickersPadding.top() + rows * st::stickersSize.height() + st::stickersPadding.bottom());
	}
	_loaded = true;

	updateSelected();

	emit updateButtons();
}

bool StickerSetBox::Inner::failedSet(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	_loaded = true;

	Ui::show(Box<InformBox>(lang(lng_stickers_not_found)));

	return true;
}

void StickerSetBox::Inner::installDone(const MTPmessages_StickerSetInstallResult &result) {
	auto &sets = Global::RefStickerSets();

	bool wasArchived = (_setFlags & MTPDstickerSet::Flag::f_archived);
	if (wasArchived) {
		auto index = Global::RefArchivedStickerSetsOrder().indexOf(_setId);
		if (index >= 0) {
			Global::RefArchivedStickerSetsOrder().removeAt(index);
		}
	}
	_setFlags &= ~MTPDstickerSet::Flag::f_archived;
	_setFlags |= MTPDstickerSet::Flag::f_installed;
	auto it = sets.find(_setId);
	if (it == sets.cend()) {
		it = sets.insert(_setId, Stickers::Set(_setId, _setAccess, _setTitle, _setShortName, _setCount, _setHash, _setFlags));
	} else {
		it->flags = _setFlags;
	}
	it->stickers = _pack;
	it->emoji = _emoji;

	auto &order = Global::RefStickerSetsOrder();
	int insertAtIndex = 0, currentIndex = order.indexOf(_setId);
	if (currentIndex != insertAtIndex) {
		if (currentIndex > 0) {
			order.removeAt(currentIndex);
		}
		order.insert(insertAtIndex, _setId);
	}

	auto custom = sets.find(Stickers::CustomSetId);
	if (custom != sets.cend()) {
		for_const (auto sticker, _pack) {
			int removeIndex = custom->stickers.indexOf(sticker);
			if (removeIndex >= 0) custom->stickers.removeAt(removeIndex);
		}
		if (custom->stickers.isEmpty()) {
			sets.erase(custom);
		}
	}

	if (result.type() == mtpc_messages_stickerSetInstallResultArchive) {
		Stickers::ApplyArchivedResult(result.c_messages_stickerSetInstallResultArchive());
	} else {
		if (wasArchived) {
			Local::writeArchivedStickers();
		}
		Local::writeInstalledStickers();
		Auth().data().stickersUpdated().notify(true);
	}
	emit installed(_setId);
}

bool StickerSetBox::Inner::installFail(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	Ui::show(Box<InformBox>(lang(lng_stickers_not_found)));

	return true;
}

void StickerSetBox::Inner::mousePressEvent(QMouseEvent *e) {
	int index = stickerFromGlobalPos(e->globalPos());
	if (index >= 0 && index < _pack.size()) {
		_previewTimer.start(QApplication::startDragTime());
	}
}

void StickerSetBox::Inner::mouseMoveEvent(QMouseEvent *e) {
	updateSelected();
	if (_previewShown >= 0) {
		int index = stickerFromGlobalPos(e->globalPos());
		if (index >= 0 && index < _pack.size() && index != _previewShown) {
			_previewShown = index;
			Ui::showMediaPreview(_pack.at(_previewShown));
		}
	}
}

void StickerSetBox::Inner::leaveEventHook(QEvent *e) {
	setSelected(-1);
}

void StickerSetBox::Inner::mouseReleaseEvent(QMouseEvent *e) {
	if (_previewShown >= 0) {
		_previewShown = -1;
		return;
	}
	if (_previewTimer.isActive()) {
		_previewTimer.stop();
		int index = stickerFromGlobalPos(e->globalPos());
		if (index >= 0 && index < _pack.size() && !isMasksSet()) {
			if (auto main = App::main()) {
				if (main->onSendSticker(_pack.at(index))) {
					Ui::hideSettingsAndLayer();
				}
			}
		}
	}
}

void StickerSetBox::Inner::updateSelected() {
	auto selected = stickerFromGlobalPos(QCursor::pos());
	setSelected(isMasksSet() ? -1 : selected);
}

void StickerSetBox::Inner::setSelected(int selected) {
	if (_selected != selected) {
		startOverAnimation(_selected, 1., 0.);
		_selected = selected;
		startOverAnimation(_selected, 0., 1.);
		setCursor(_selected >= 0 ? style::cur_pointer : style::cur_default);
	}
}

void StickerSetBox::Inner::startOverAnimation(int index, double from, double to) {
	if (index >= 0 && index < _packOvers.size()) {
		_packOvers[index].start([this, index] {
			int row = index / kStickersPanelPerRow;
			int column = index % kStickersPanelPerRow;
			int left = st::stickersPadding.left() + column * st::stickersSize.width();
			int top = st::stickersPadding.top() + row * st::stickersSize.height();
			rtlupdate(left, top, st::stickersSize.width(), st::stickersSize.height());
		}, from, to, st::emojiPanDuration);
	}
}

void StickerSetBox::Inner::onPreview() {
	int index = stickerFromGlobalPos(QCursor::pos());
	if (index >= 0 && index < _pack.size()) {
		_previewShown = index;
		Ui::showMediaPreview(_pack.at(_previewShown));
	}
}

qint32 StickerSetBox::Inner::stickerFromGlobalPos(const QPoint &p) const {
	QPoint l(mapFromGlobal(p));
	if (rtl()) l.setX(width() - l.x());
	qint32 row = (l.y() >= st::stickersPadding.top()) ? qFloor((l.y() - st::stickersPadding.top()) / st::stickersSize.height()) : -1;
	qint32 col = (l.x() >= st::stickersPadding.left()) ? qFloor((l.x() - st::stickersPadding.left()) / st::stickersSize.width()) : -1;
	if (row >= 0 && col >= 0 && col < kStickersPanelPerRow) {
		qint32 result = row * kStickersPanelPerRow + col;
		return (result < _pack.size()) ? result : -1;
	}
	return -1;
}

void StickerSetBox::Inner::paintEvent(QPaintEvent *e) {
	QRect r(e->rect());
	Painter p(this);

	if (_pack.isEmpty()) return;

	auto ms = getms();
	qint32 rows = _pack.size() / kStickersPanelPerRow + ((_pack.size() % kStickersPanelPerRow) ? 1 : 0);
	qint32 from = qFloor(e->rect().top() / st::stickersSize.height()), to = qFloor(e->rect().bottom() / st::stickersSize.height()) + 1;

	for (qint32 i = from; i < to; ++i) {
		for (qint32 j = 0; j < kStickersPanelPerRow; ++j) {
			qint32 index = i * kStickersPanelPerRow + j;
			if (index >= _pack.size()) break;
			Assert(index < _packOvers.size());

			DocumentData *doc = _pack.at(index);
			QPoint pos(st::stickersPadding.left() + j * st::stickersSize.width(), st::stickersPadding.top() + i * st::stickersSize.height());

			if (auto over = _packOvers[index].current(ms, (index == _selected) ? 1. : 0.)) {
				p.setOpacity(over);
				QPoint tl(pos);
				if (rtl()) tl.setX(width() - tl.x() - st::stickersSize.width());
				App::roundRect(p, QRect(tl, st::stickersSize), st::emojiPanHover, StickerHoverCorners);
				p.setOpacity(1);

			}
			bool goodThumb = !doc->thumb->isNull() && ((doc->thumb->width() >= 128) || (doc->thumb->height() >= 128));
			if (goodThumb) {
				doc->thumb->load();
			} else {
				if (doc->status == FileReady) {
					doc->automaticLoad(0);
				}
				if (doc->sticker()->img->isNull() && doc->loaded(DocumentData::FilePathResolveChecked)) {
					doc->sticker()->img = doc->data().isEmpty() ? ImagePtr(doc->filepath()) : ImagePtr(doc->data());
				}
			}

			double coef = qMin((st::stickersSize.width() - st::buttonRadius * 2) / double(doc->dimensions.width()), (st::stickersSize.height() - st::buttonRadius * 2) / double(doc->dimensions.height()));
			if (coef > 1) coef = 1;
			qint32 w = qRound(coef * doc->dimensions.width()), h = qRound(coef * doc->dimensions.height());
			if (w < 1) w = 1;
			if (h < 1) h = 1;
			QPoint ppos = pos + QPoint((st::stickersSize.width() - w) / 2, (st::stickersSize.height() - h) / 2);
			if (goodThumb) {
				p.drawPixmapLeft(ppos, width(), doc->thumb->pix(w, h));
			} else if (!doc->sticker()->img->isNull()) {
				p.drawPixmapLeft(ppos, width(), doc->sticker()->img->pix(w, h));
			}
		}
	}
}

void StickerSetBox::Inner::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	_visibleTop = visibleTop;
	_visibleBottom = visibleBottom;
}

bool StickerSetBox::Inner::loaded() const {
	return _loaded && !_pack.isEmpty();
}

qint32 StickerSetBox::Inner::notInstalled() const {
	if (!_loaded) return 0;
	auto it = Global::StickerSets().constFind(_setId);
	if (it == Global::StickerSets().cend() || !(it->flags & MTPDstickerSet::Flag::f_installed) || (it->flags & MTPDstickerSet::Flag::f_archived)) return _pack.size();
	return 0;
}

bool StickerSetBox::Inner::official() const {
	return _loaded && _setShortName.isEmpty();
}

base::lambda<TextWithEntities()> StickerSetBox::Inner::title() const {
	auto text = TextWithEntities { _setTitle };
	if (_loaded) {
		if (_pack.isEmpty()) {
			return [] { return TextWithEntities { lang(lng_attach_failed), EntitiesInText() }; };
		} else {
			TextUtilities::ParseEntities(text, TextParseMentions);
		}
	} else {
		return [] { return TextWithEntities { lang(lng_contacts_loading), EntitiesInText() }; };
	}
	return [text] { return text; };
}

QString StickerSetBox::Inner::shortName() const {
	return _setShortName;
}

void StickerSetBox::Inner::install() {
	if (isMasksSet()) {
		Ui::show(Box<InformBox>(lang(lng_stickers_masks_pack)), KeepOtherLayers);
		return;
	}
	if (_installRequest) return;
	_installRequest = MTP::send(MTPmessages_InstallStickerSet(_input, MTP_bool(false)), rpcDone(&Inner::installDone), rpcFail(&Inner::installFail));
}

StickerSetBox::Inner::~Inner() {
}
