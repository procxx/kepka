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
#include "chat_helpers/field_autocomplete.h"

#include "apiwrap.h"
#include "app.h"
#include "auth_session.h"
#include "chat_helpers/stickers.h"
#include "mainwindow.h"
#include "storage/localstorage.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_history.h"
#include "styles/style_widgets.h"
#include "ui/widgets/scroll_area.h"
#include <QApplication>

FieldAutocomplete::FieldAutocomplete(QWidget *parent)
    : TWidget(parent)
    , _scroll(this, st::mentionScroll) {
	_scroll->setGeometry(rect());

	_inner =
	    _scroll->setOwnedWidget(object_ptr<internal::FieldAutocompleteInner>(this, &_mrows, &_hrows, &_brows, &_srows));
	_inner->setGeometry(rect());

	connect(_inner, SIGNAL(mentionChosen(UserData *, FieldAutocomplete::ChooseMethod)), this,
	        SIGNAL(mentionChosen(UserData *, FieldAutocomplete::ChooseMethod)));
	connect(_inner, SIGNAL(hashtagChosen(QString, FieldAutocomplete::ChooseMethod)), this,
	        SIGNAL(hashtagChosen(QString, FieldAutocomplete::ChooseMethod)));
	connect(_inner, SIGNAL(botCommandChosen(QString, FieldAutocomplete::ChooseMethod)), this,
	        SIGNAL(botCommandChosen(QString, FieldAutocomplete::ChooseMethod)));
	connect(_inner, SIGNAL(stickerChosen(DocumentData *, FieldAutocomplete::ChooseMethod)), this,
	        SIGNAL(stickerChosen(DocumentData *, FieldAutocomplete::ChooseMethod)));
	connect(_inner, SIGNAL(mustScrollTo(int, int)), _scroll, SLOT(scrollToY(int, int)));

	_scroll->show();
	_inner->show();

	hide();

	connect(_scroll, SIGNAL(geometryChanged()), _inner, SLOT(onParentGeometryChanged()));
}

void FieldAutocomplete::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto opacity = _a_opacity.current(getms(), _hiding ? 0. : 1.);
	if (opacity < 1.) {
		if (opacity > 0.) {
			p.setOpacity(opacity);
			p.drawPixmap(0, 0, _cache);
		} else if (_hiding) {
		}
		return;
	}

	p.fillRect(rect(), st::mentionBg);
}

void FieldAutocomplete::showFiltered(PeerData *peer, QString query, bool addInlineBots) {
	_chat = peer->asChat();
	_user = peer->asUser();
	_channel = peer->asChannel();
	if (query.isEmpty()) {
		_type = Type::Mentions;
		rowsUpdated(internal::MentionRows(), internal::HashtagRows(), internal::BotCommandRows(), _srows, false);
		return;
	}

	_emoji = nullptr;

	query = query.toLower();
	auto type = Type::Stickers;
	auto plainQuery = query.midRef(0);
	switch (query.at(0).unicode()) {
	case '@':
		type = Type::Mentions;
		plainQuery = query.midRef(1);
		break;
	case '#':
		type = Type::Hashtags;
		plainQuery = query.midRef(1);
		break;
	case '/':
		type = Type::BotCommands;
		plainQuery = query.midRef(1);
		break;
	}
	bool resetScroll = (_type != type || _filter != plainQuery);
	if (resetScroll) {
		_type = type;
		_filter = TextUtilities::RemoveAccents(plainQuery.toString());
	}
	_addInlineBots = addInlineBots;

	updateFiltered(resetScroll);
}

void FieldAutocomplete::showStickers(EmojiPtr emoji) {
	bool resetScroll = (_emoji != emoji);
	_emoji = emoji;
	_type = Type::Stickers;
	if (!emoji) {
		rowsUpdated(_mrows, _hrows, _brows, StickerPack(), false);
		return;
	}

	_chat = 0;
	_user = 0;
	_channel = 0;

	updateFiltered(resetScroll);
}

bool FieldAutocomplete::clearFilteredBotCommands() {
	if (_brows.isEmpty()) return false;
	_brows.clear();
	return true;
}

namespace {
template <typename T, typename U> inline int indexOfInFirstN(const T &v, const U &elem, int last) {
	for (auto b = v.cbegin(), i = b, e = b + std::max(v.size(), last); i != e; ++i) {
		if (*i == elem) {
			return (i - b);
		}
	}
	return -1;
}
} // namespace

void FieldAutocomplete::updateFiltered(bool resetScroll) {
	qint32 now = unixtime(), recentInlineBots = 0;
	internal::MentionRows mrows;
	internal::HashtagRows hrows;
	internal::BotCommandRows brows;
	StickerPack srows;
	if (_emoji) {
		srows = Stickers::GetListByEmoji(_emoji);
	} else if (_type == Type::Mentions) {
		int maxListSize = _addInlineBots ? cRecentInlineBots().size() : 0;
		if (_chat) {
			maxListSize += (_chat->participants.isEmpty() ? _chat->lastAuthors.size() : _chat->participants.size());
		} else if (_channel && _channel->isMegagroup()) {
			if (_channel->mgInfo->lastParticipants.isEmpty() || _channel->lastParticipantsCountOutdated()) {
			} else {
				maxListSize += _channel->mgInfo->lastParticipants.size();
			}
		}
		if (maxListSize) {
			mrows.reserve(maxListSize);
		}

		auto filterNotPassedByUsername = [this](UserData *user) -> bool {
			if (user->username.startsWith(_filter, Qt::CaseInsensitive)) {
				bool exactUsername = (user->username.size() == _filter.size());
				return exactUsername;
			}
			return true;
		};
		auto filterNotPassedByName = [this, &filterNotPassedByUsername](UserData *user) -> bool {
			for_const (auto &namePart, user->names) {
				if (namePart.startsWith(_filter, Qt::CaseInsensitive)) {
					bool exactUsername = (user->username.compare(_filter, Qt::CaseInsensitive) == 0);
					return exactUsername;
				}
			}
			return filterNotPassedByUsername(user);
		};

		bool listAllSuggestions = _filter.isEmpty();
		if (_addInlineBots) {
			for_const (auto user, cRecentInlineBots()) {
				if (user->isInaccessible()) continue;
				if (!listAllSuggestions && filterNotPassedByUsername(user)) continue;
				mrows.push_back(user);
				++recentInlineBots;
			}
		}
		if (_chat) {
			QMultiMap<qint32, UserData *> ordered;
			mrows.reserve(mrows.size() +
			              (_chat->participants.isEmpty() ? _chat->lastAuthors.size() : _chat->participants.size()));
			if (_chat->noParticipantInfo()) {
				Auth().api().requestFullPeer(_chat);
			} else if (!_chat->participants.isEmpty()) {
				for (auto i = _chat->participants.cbegin(), e = _chat->participants.cend(); i != e; ++i) {
					auto user = i.key();
					if (user->isInaccessible()) continue;
					if (!listAllSuggestions && filterNotPassedByName(user)) continue;
					if (indexOfInFirstN(mrows, user, recentInlineBots) >= 0) continue;
					ordered.insertMulti(App::onlineForSort(user, now), user);
				}
			}
			for_const (auto user, _chat->lastAuthors) {
				if (user->isInaccessible()) continue;
				if (!listAllSuggestions && filterNotPassedByName(user)) continue;
				if (indexOfInFirstN(mrows, user, recentInlineBots) >= 0) continue;
				mrows.push_back(user);
				if (!ordered.isEmpty()) {
					ordered.remove(App::onlineForSort(user, now), user);
				}
			}
			if (!ordered.isEmpty()) {
				for (auto i = ordered.cend(), b = ordered.cbegin(); i != b;) {
					--i;
					mrows.push_back(i.value());
				}
			}
		} else if (_channel && _channel->isMegagroup()) {
			QMultiMap<qint32, UserData *> ordered;
			if (_channel->mgInfo->lastParticipants.isEmpty() || _channel->lastParticipantsCountOutdated()) {
				Auth().api().requestLastParticipants(_channel);
			} else {
				mrows.reserve(mrows.size() + _channel->mgInfo->lastParticipants.size());
				for_const (auto user, _channel->mgInfo->lastParticipants) {
					if (user->isInaccessible()) continue;
					if (!listAllSuggestions && filterNotPassedByName(user)) continue;
					if (indexOfInFirstN(mrows, user, recentInlineBots) >= 0) continue;
					mrows.push_back(user);
				}
			}
		}
	} else if (_type == Type::Hashtags) {
		bool listAllSuggestions = _filter.isEmpty();
		auto &recent(cRecentWriteHashtags());
		hrows.reserve(recent.size());
		for (auto i = recent.cbegin(), e = recent.cend(); i != e; ++i) {
			if (!listAllSuggestions &&
			    (!i->first.startsWith(_filter, Qt::CaseInsensitive) || i->first.size() == _filter.size())) {
				continue;
			}
			hrows.push_back(i->first);
		}
	} else if (_type == Type::BotCommands) {
		bool listAllSuggestions = _filter.isEmpty();
		bool hasUsername = _filter.indexOf('@') > 0;
		QMap<UserData *, bool> bots;
		qint32 cnt = 0;
		if (_chat) {
			if (_chat->noParticipantInfo()) {
				Auth().api().requestFullPeer(_chat);
			} else if (!_chat->participants.isEmpty()) {
				for (auto i = _chat->participants.cbegin(), e = _chat->participants.cend(); i != e; ++i) {
					auto user = i.key();
					if (!user->botInfo) continue;
					if (!user->botInfo->inited) {
						Auth().api().requestFullPeer(user);
					}
					if (user->botInfo->commands.isEmpty()) continue;
					bots.insert(user, true);
					cnt += user->botInfo->commands.size();
				}
			}
		} else if (_user && _user->botInfo) {
			if (!_user->botInfo->inited) {
				Auth().api().requestFullPeer(_user);
			}
			cnt = _user->botInfo->commands.size();
			bots.insert(_user, true);
		} else if (_channel && _channel->isMegagroup()) {
			if (_channel->mgInfo->bots.empty()) {
				if (!_channel->mgInfo->botStatus) {
					Auth().api().requestBots(_channel);
				}
			} else {
				for_const (auto user, _channel->mgInfo->bots) {
					if (!user->botInfo) continue;
					if (!user->botInfo->inited) {
						Auth().api().requestFullPeer(user);
					}
					if (user->botInfo->commands.isEmpty()) continue;
					bots.insert(user, true);
					cnt += user->botInfo->commands.size();
				}
			}
		}
		if (cnt) {
			brows.reserve(cnt);
			qint32 botStatus =
			    _chat ? _chat->botStatus : ((_channel && _channel->isMegagroup()) ? _channel->mgInfo->botStatus : -1);
			if (_chat) {
				for (auto i = _chat->lastAuthors.cbegin(), e = _chat->lastAuthors.cend(); i != e; ++i) {
					auto user = *i;
					if (!user->botInfo) continue;
					if (!bots.contains(user)) continue;
					if (!user->botInfo->inited) {
						Auth().api().requestFullPeer(user);
					}
					if (user->botInfo->commands.isEmpty()) continue;
					bots.remove(user);
					for (auto j = 0, l = user->botInfo->commands.size(); j != l; ++j) {
						if (!listAllSuggestions) {
							auto toFilter = (hasUsername || botStatus == 0 || botStatus == 2) ?
							                    user->botInfo->commands.at(j).command + '@' + user->username :
							                    user->botInfo->commands.at(j).command;
							if (!toFilter.startsWith(_filter,
							                         Qt::CaseInsensitive) /* || toFilter.size() == _filter.size()*/) {
								continue;
							}
						}
						brows.push_back(qMakePair(user, &user->botInfo->commands.at(j)));
					}
				}
			}
			if (!bots.isEmpty()) {
				for (QMap<UserData *, bool>::const_iterator i = bots.cbegin(), e = bots.cend(); i != e; ++i) {
					UserData *user = i.key();
					for (qint32 j = 0, l = user->botInfo->commands.size(); j < l; ++j) {
						if (!listAllSuggestions) {
							QString toFilter = (hasUsername || botStatus == 0 || botStatus == 2) ?
							                       user->botInfo->commands.at(j).command + '@' + user->username :
							                       user->botInfo->commands.at(j).command;
							if (!toFilter.startsWith(_filter,
							                         Qt::CaseInsensitive) /* || toFilter.size() == _filter.size()*/)
								continue;
						}
						brows.push_back(qMakePair(user, &user->botInfo->commands.at(j)));
					}
				}
			}
		}
	}
	rowsUpdated(mrows, hrows, brows, srows, resetScroll);
	_inner->setRecentInlineBotsInRows(recentInlineBots);
}

void FieldAutocomplete::rowsUpdated(const internal::MentionRows &mrows, const internal::HashtagRows &hrows,
                                    const internal::BotCommandRows &brows, const StickerPack &srows, bool resetScroll) {
	if (mrows.isEmpty() && hrows.isEmpty() && brows.isEmpty() && srows.isEmpty()) {
		if (!isHidden()) {
			hideAnimated();
		}
		_mrows.clear();
		_hrows.clear();
		_brows.clear();
		_srows.clear();
	} else {
		_mrows = mrows;
		_hrows = hrows;
		_brows = brows;
		_srows = srows;

		bool hidden = _hiding || isHidden();
		if (hidden) {
			show();
			_scroll->show();
		}
		recount(resetScroll);
		update();
		if (hidden) {
			hide();
			showAnimated();
		}
	}
}

void FieldAutocomplete::setBoundings(QRect boundings) {
	_boundings = boundings;
	recount();
}

void FieldAutocomplete::recount(bool resetScroll) {
	qint32 h = 0, oldst = _scroll->scrollTop(), st = oldst, maxh = 4.5 * st::mentionHeight;
	if (!_srows.isEmpty()) {
		qint32 stickersPerRow =
		    std::max(1, qint32(_boundings.width() - 2 * st::stickerPanPadding) / qint32(st::stickerPanSize.width()));
		qint32 rows = rowscount(_srows.size(), stickersPerRow);
		h = st::stickerPanPadding + rows * st::stickerPanSize.height();
	} else if (!_mrows.isEmpty()) {
		h = _mrows.size() * st::mentionHeight;
	} else if (!_hrows.isEmpty()) {
		h = _hrows.size() * st::mentionHeight;
	} else if (!_brows.isEmpty()) {
		h = _brows.size() * st::mentionHeight;
	}

	if (_inner->width() != _boundings.width() || _inner->height() != h) {
		_inner->resize(_boundings.width(), h);
	}
	if (h > _boundings.height()) h = _boundings.height();
	if (h > maxh) h = maxh;
	if (width() != _boundings.width() || height() != h) {
		setGeometry(_boundings.x(), _boundings.y() + _boundings.height() - h, _boundings.width(), h);
		_scroll->resize(_boundings.width(), h);
	} else if (y() != _boundings.y() + _boundings.height() - h) {
		move(_boundings.x(), _boundings.y() + _boundings.height() - h);
	}
	if (resetScroll) st = 0;
	if (st != oldst) _scroll->scrollToY(st);
	if (resetScroll) _inner->clearSel();
}

void FieldAutocomplete::hideFast() {
	_a_opacity.finish();
	hideFinish();
}

void FieldAutocomplete::hideAnimated() {
	if (isHidden() || _hiding) {
		return;
	}

	if (_cache.isNull()) {
		_scroll->show();
		_cache = myGrab(this);
	}
	_scroll->hide();
	_hiding = true;
	_a_opacity.start([this] { animationCallback(); }, 1., 0., st::emojiPanDuration);
	setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void FieldAutocomplete::hideFinish() {
	hide();
	_hiding = false;
	_filter = qsl("-");
	_inner->clearSel(true);
}

void FieldAutocomplete::showAnimated() {
	if (!isHidden() && !_hiding) {
		return;
	}
	if (_cache.isNull()) {
		_scroll->show();
		_cache = myGrab(this);
	}
	_scroll->hide();
	_hiding = false;
	show();
	_a_opacity.start([this] { animationCallback(); }, 0., 1., st::emojiPanDuration);
	setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void FieldAutocomplete::animationCallback() {
	update();
	if (!_a_opacity.animating()) {
		_cache = QPixmap();
		setAttribute(Qt::WA_OpaquePaintEvent);
		if (_hiding) {
			hideFinish();
		} else {
			_scroll->show();
			_inner->clearSel();
		}
	}
}

const QString &FieldAutocomplete::filter() const {
	return _filter;
}

ChatData *FieldAutocomplete::chat() const {
	return _chat;
}

ChannelData *FieldAutocomplete::channel() const {
	return _channel;
}

UserData *FieldAutocomplete::user() const {
	return _user;
}

qint32 FieldAutocomplete::innerTop() {
	return _scroll->scrollTop();
}

qint32 FieldAutocomplete::innerBottom() {
	return _scroll->scrollTop() + _scroll->height();
}

bool FieldAutocomplete::chooseSelected(ChooseMethod method) const {
	return _inner->chooseSelected(method);
}

bool FieldAutocomplete::eventFilter(QObject *obj, QEvent *e) {
	auto hidden = isHidden();
	auto moderate = Global::ModerateModeEnabled();
	if (hidden && !moderate) return QWidget::eventFilter(obj, e);

	if (e->type() == QEvent::KeyPress) {
		QKeyEvent *ev = static_cast<QKeyEvent *>(e);
		if (!(ev->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
			if (!hidden) {
				if (ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down ||
				    (!_srows.isEmpty() && (ev->key() == Qt::Key_Left || ev->key() == Qt::Key_Right))) {
					return _inner->moveSel(ev->key());
				} else if (ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return) {
					return _inner->chooseSelected(ChooseMethod::ByEnter);
				}
			}
			if (moderate && ((ev->key() >= Qt::Key_1 && ev->key() <= Qt::Key_9) || ev->key() == Qt::Key_Q)) {
				bool handled = false;
				emit moderateKeyActivate(ev->key(), &handled);
				return handled;
			}
		}
	}
	return QWidget::eventFilter(obj, e);
}

FieldAutocomplete::~FieldAutocomplete() {}

namespace internal {

FieldAutocompleteInner::FieldAutocompleteInner(FieldAutocomplete *parent, MentionRows *mrows, HashtagRows *hrows,
                                               BotCommandRows *brows, StickerPack *srows)
    : _parent(parent)
    , _mrows(mrows)
    , _hrows(hrows)
    , _brows(brows)
    , _srows(srows)
    , _stickersPerRow(1)
    , _recentInlineBotsInRows(0)
    , _sel(-1)
    , _down(-1)
    , _mouseSel(false)
    , _overDelete(false)
    , _previewShown(false) {
	_previewTimer.setSingleShot(true);
	connect(&_previewTimer, SIGNAL(timeout()), this, SLOT(onPreview()));
	subscribe(Auth().downloaderTaskFinished(), [this] { update(); });
}

void FieldAutocompleteInner::paintEvent(QPaintEvent *e) {
	Painter p(this);

	QRect r(e->rect());
	if (r != rect()) p.setClipRect(r);

	qint32 mentionleft = 2 * st::mentionPadding.left() + st::mentionPhotoSize;
	qint32 mentionwidth = width() - mentionleft - 2 * st::mentionPadding.right();
	qint32 htagleft = st::historyAttach.width + st::historyComposeField.textMrg.left() - st::lineWidth,
	       htagwidth = width() - st::mentionPadding.right() - htagleft - st::mentionScroll.width;

	if (!_srows->isEmpty()) {
		qint32 rows = rowscount(_srows->size(), _stickersPerRow);
		qint32 fromrow = floorclamp(r.y() - st::stickerPanPadding, st::stickerPanSize.height(), 0, rows);
		qint32 torow = ceilclamp(r.y() + r.height() - st::stickerPanPadding, st::stickerPanSize.height(), 0, rows);
		qint32 fromcol = floorclamp(r.x() - st::stickerPanPadding, st::stickerPanSize.width(), 0, _stickersPerRow);
		qint32 tocol =
		    ceilclamp(r.x() + r.width() - st::stickerPanPadding, st::stickerPanSize.width(), 0, _stickersPerRow);
		for (qint32 row = fromrow; row < torow; ++row) {
			for (qint32 col = fromcol; col < tocol; ++col) {
				qint32 index = row * _stickersPerRow + col;
				if (index >= _srows->size()) break;

				DocumentData *sticker = _srows->at(index);
				if (!sticker->sticker()) continue;

				QPoint pos(st::stickerPanPadding + col * st::stickerPanSize.width(),
				           st::stickerPanPadding + row * st::stickerPanSize.height());
				if (_sel == index) {
					QPoint tl(pos);
					if (rtl()) tl.setX(width() - tl.x() - st::stickerPanSize.width());
					App::roundRect(p, QRect(tl, st::stickerPanSize), st::emojiPanHover, StickerHoverCorners);
				}

				bool goodThumb = !sticker->thumb->isNull() &&
				                 ((sticker->thumb->width() >= 128) || (sticker->thumb->height() >= 128));
				if (goodThumb) {
					sticker->thumb->load();
				} else {
					sticker->checkSticker();
				}

				double coef = std::min(
				    (st::stickerPanSize.width() - st::buttonRadius * 2) / double(sticker->dimensions.width()),
				    (st::stickerPanSize.height() - st::buttonRadius * 2) / double(sticker->dimensions.height()));
				if (coef > 1) coef = 1;
				qint32 w = std::round(coef * sticker->dimensions.width()),
				       h = std::round(coef * sticker->dimensions.height());
				if (w < 1) w = 1;
				if (h < 1) h = 1;
				QPoint ppos = pos + QPoint((st::stickerPanSize.width() - w) / 2, (st::stickerPanSize.height() - h) / 2);
				if (goodThumb) {
					p.drawPixmapLeft(ppos, width(), sticker->thumb->pix(w, h));
				} else if (!sticker->sticker()->img->isNull()) {
					p.drawPixmapLeft(ppos, width(), sticker->sticker()->img->pix(w, h));
				}
			}
		}
	} else {
		qint32 from = std::floor(e->rect().top() / st::mentionHeight),
		       to = std::floor(e->rect().bottom() / st::mentionHeight) + 1;
		qint32 last = _mrows->isEmpty() ? (_hrows->isEmpty() ? _brows->size() : _hrows->size()) : _mrows->size();
		auto filter = _parent->filter();
		bool hasUsername = filter.indexOf('@') > 0;
		int filterSize = filter.size();
		bool filterIsEmpty = filter.isEmpty();
		for (qint32 i = from; i < to; ++i) {
			if (i >= last) break;

			bool selected = (i == _sel);
			if (selected) {
				p.fillRect(0, i * st::mentionHeight, width(), st::mentionHeight, st::mentionBgOver);
				int skip = (st::mentionHeight - st::smallCloseIconOver.height()) / 2;
				if (!_hrows->isEmpty() || (!_mrows->isEmpty() && i < _recentInlineBotsInRows)) {
					st::smallCloseIconOver.paint(
					    p, QPoint(width() - st::smallCloseIconOver.width() - skip, i * st::mentionHeight + skip),
					    width());
				}
			}
			if (!_mrows->isEmpty()) {
				UserData *user = _mrows->at(i);
				QString first = (!filterIsEmpty && user->username.startsWith(filter, Qt::CaseInsensitive)) ?
				                    ('@' + user->username.mid(0, filterSize)) :
				                    QString();
				QString second = first.isEmpty() ? (user->username.isEmpty() ? QString() : ('@' + user->username)) :
				                                   user->username.mid(filterSize);
				qint32 firstwidth = st::mentionFont->width(first), secondwidth = st::mentionFont->width(second),
				       unamewidth = firstwidth + secondwidth, namewidth = user->nameText.maxWidth();
				if (mentionwidth < unamewidth + namewidth) {
					namewidth = (mentionwidth * namewidth) / (namewidth + unamewidth);
					unamewidth = mentionwidth - namewidth;
					if (firstwidth < unamewidth + st::mentionFont->elidew) {
						if (firstwidth < unamewidth) {
							first = st::mentionFont->elided(first, unamewidth);
						} else if (!second.isEmpty()) {
							first = st::mentionFont->elided(first + second, unamewidth);
							second = QString();
						}
					} else {
						second = st::mentionFont->elided(second, unamewidth - firstwidth);
					}
				}
				user->loadUserpic();
				user->paintUserpicLeft(p, st::mentionPadding.left(), i * st::mentionHeight + st::mentionPadding.top(),
				                       width(), st::mentionPhotoSize);

				p.setPen(selected ? st::mentionNameFgOver : st::mentionNameFg);
				user->nameText.drawElided(p, 2 * st::mentionPadding.left() + st::mentionPhotoSize,
				                          i * st::mentionHeight + st::mentionTop, namewidth);

				p.setFont(st::mentionFont);
				p.setPen(selected ? st::mentionFgOverActive : st::mentionFgActive);
				p.drawText(mentionleft + namewidth + st::mentionPadding.right(),
				           i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent, first);
				if (!second.isEmpty()) {
					p.setPen(selected ? st::mentionFgOver : st::mentionFg);
					p.drawText(mentionleft + namewidth + st::mentionPadding.right() + firstwidth,
					           i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent, second);
				}
			} else if (!_hrows->isEmpty()) {
				QString hrow = _hrows->at(i);
				QString first = filterIsEmpty ? QString() : ('#' + hrow.mid(0, filterSize));
				QString second = filterIsEmpty ? ('#' + hrow) : hrow.mid(filterSize);
				qint32 firstwidth = st::mentionFont->width(first), secondwidth = st::mentionFont->width(second);
				if (htagwidth < firstwidth + secondwidth) {
					if (htagwidth < firstwidth + st::mentionFont->elidew) {
						first = st::mentionFont->elided(first + second, htagwidth);
						second = QString();
					} else {
						second = st::mentionFont->elided(second, htagwidth - firstwidth);
					}
				}

				p.setFont(st::mentionFont);
				if (!first.isEmpty()) {
					p.setPen((selected ? st::mentionFgOverActive : st::mentionFgActive)->p);
					p.drawText(htagleft, i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent, first);
				}
				if (!second.isEmpty()) {
					p.setPen((selected ? st::mentionFgOver : st::mentionFg)->p);
					p.drawText(htagleft + firstwidth, i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent,
					           second);
				}
			} else {
				UserData *user = _brows->at(i).first;

				const BotCommand *command = _brows->at(i).second;
				QString toHighlight = command->command;
				qint32 botStatus = _parent->chat() ? _parent->chat()->botStatus :
				                                     ((_parent->channel() && _parent->channel()->isMegagroup()) ?
				                                          _parent->channel()->mgInfo->botStatus :
				                                          -1);
				if (hasUsername || botStatus == 0 || botStatus == 2) {
					toHighlight += '@' + user->username;
				}
				user->loadUserpic();
				user->paintUserpicLeft(p, st::mentionPadding.left(), i * st::mentionHeight + st::mentionPadding.top(),
				                       width(), st::mentionPhotoSize);

				auto commandText = '/' + toHighlight;

				p.setPen(selected ? st::mentionNameFgOver : st::mentionNameFg);
				p.setFont(st::semiboldFont);
				p.drawText(2 * st::mentionPadding.left() + st::mentionPhotoSize,
				           i * st::mentionHeight + st::mentionTop + st::semiboldFont->ascent, commandText);

				auto commandTextWidth = st::semiboldFont->width(commandText);
				auto addleft = commandTextWidth + st::mentionPadding.left();
				auto widthleft = mentionwidth - addleft;

				if (widthleft > st::mentionFont->elidew && !command->descriptionText().isEmpty()) {
					p.setPen((selected ? st::mentionFgOver : st::mentionFg)->p);
					command->descriptionText().drawElided(p, mentionleft + addleft,
					                                      i * st::mentionHeight + st::mentionTop, widthleft);
				}
			}
		}
		p.fillRect(Adaptive::OneColumn() ? 0 : st::lineWidth, _parent->innerBottom() - st::lineWidth,
		           width() - (Adaptive::OneColumn() ? 0 : st::lineWidth), st::lineWidth, st::shadowFg);
	}
	p.fillRect(Adaptive::OneColumn() ? 0 : st::lineWidth, _parent->innerTop(),
	           width() - (Adaptive::OneColumn() ? 0 : st::lineWidth), st::lineWidth, st::shadowFg);
}

void FieldAutocompleteInner::resizeEvent(QResizeEvent *e) {
	_stickersPerRow = std::max(1, qint32(width() - 2 * st::stickerPanPadding) / qint32(st::stickerPanSize.width()));
}

void FieldAutocompleteInner::mouseMoveEvent(QMouseEvent *e) {
	_mousePos = mapToGlobal(e->pos());
	_mouseSel = true;
	onUpdateSelected(true);
}

void FieldAutocompleteInner::clearSel(bool hidden) {
	_mouseSel = _overDelete = false;
	setSel((_mrows->isEmpty() && _brows->isEmpty() && _hrows->isEmpty()) ? -1 : 0);
	if (hidden) {
		_down = -1;
		_previewShown = false;
	}
}

bool FieldAutocompleteInner::moveSel(int key) {
	_mouseSel = false;
	qint32 maxSel = (_mrows->isEmpty() ?
	                     (_hrows->isEmpty() ? (_brows->isEmpty() ? _srows->size() : _brows->size()) : _hrows->size()) :
	                     _mrows->size());
	qint32 direction = (key == Qt::Key_Up) ? -1 : (key == Qt::Key_Down ? 1 : 0);
	if (!_srows->isEmpty()) {
		if (key == Qt::Key_Left) {
			direction = -1;
		} else if (key == Qt::Key_Right) {
			direction = 1;
		} else {
			direction *= _stickersPerRow;
		}
	}
	if (_sel >= maxSel || _sel < 0) {
		if (direction < -1) {
			setSel(((maxSel - 1) / _stickersPerRow) * _stickersPerRow, true);
		} else if (direction < 0) {
			setSel(maxSel - 1, true);
		} else {
			setSel(0, true);
		}
		return (_sel >= 0 && _sel < maxSel);
	}
	setSel((_sel + direction >= maxSel || _sel + direction < 0) ? -1 : (_sel + direction), true);
	return true;
}

bool FieldAutocompleteInner::chooseSelected(FieldAutocomplete::ChooseMethod method) const {
	if (!_srows->isEmpty()) {
		if (_sel >= 0 && _sel < _srows->size()) {
			emit stickerChosen(_srows->at(_sel), method);
			return true;
		}
	} else if (!_mrows->isEmpty()) {
		if (_sel >= 0 && _sel < _mrows->size()) {
			emit mentionChosen(_mrows->at(_sel), method);
			return true;
		}
	} else if (!_hrows->isEmpty()) {
		if (_sel >= 0 && _sel < _hrows->size()) {
			emit hashtagChosen('#' + _hrows->at(_sel), method);
			return true;
		}
	} else if (!_brows->isEmpty()) {
		if (_sel >= 0 && _sel < _brows->size()) {
			UserData *user = _brows->at(_sel).first;
			const BotCommand *command(_brows->at(_sel).second);
			qint32 botStatus = _parent->chat() ? _parent->chat()->botStatus :
			                                     ((_parent->channel() && _parent->channel()->isMegagroup()) ?
			                                          _parent->channel()->mgInfo->botStatus :
			                                          -1);
			if (botStatus == 0 || botStatus == 2 || _parent->filter().indexOf('@') > 0) {
				emit botCommandChosen('/' + command->command + '@' + user->username, method);
			} else {
				emit botCommandChosen('/' + command->command, method);
			}
			return true;
		}
	}
	return false;
}

void FieldAutocompleteInner::setRecentInlineBotsInRows(qint32 bots) {
	_recentInlineBotsInRows = bots;
}

void FieldAutocompleteInner::mousePressEvent(QMouseEvent *e) {
	_mousePos = mapToGlobal(e->pos());
	_mouseSel = true;
	onUpdateSelected(true);
	if (e->button() == Qt::LeftButton) {
		if (_overDelete && _sel >= 0 && _sel < (_mrows->isEmpty() ? _hrows->size() : _recentInlineBotsInRows)) {
			_mousePos = mapToGlobal(e->pos());
			bool removed = false;
			if (_mrows->isEmpty()) {
				QString toRemove = _hrows->at(_sel);
				RecentHashtagPack &recent(cRefRecentWriteHashtags());
				for (RecentHashtagPack::iterator i = recent.begin(); i != recent.cend();) {
					if (i->first == toRemove) {
						i = recent.erase(i);
						removed = true;
					} else {
						++i;
					}
				}
			} else {
				UserData *toRemove = _mrows->at(_sel);
				RecentInlineBots &recent(cRefRecentInlineBots());
				qint32 index = recent.indexOf(toRemove);
				if (index >= 0) {
					recent.remove(index);
					removed = true;
				}
			}
			if (removed) {
				Local::writeRecentHashtagsAndBots();
			}
			_parent->updateFiltered();

			_mouseSel = true;
			onUpdateSelected(true);
		} else if (_srows->isEmpty()) {
			chooseSelected(FieldAutocomplete::ChooseMethod::ByClick);
		} else {
			_down = _sel;
			_previewTimer.start(QApplication::startDragTime());
		}
	}
}

void FieldAutocompleteInner::mouseReleaseEvent(QMouseEvent *e) {
	_previewTimer.stop();

	qint32 pressed = _down;
	_down = -1;

	_mousePos = mapToGlobal(e->pos());
	_mouseSel = true;
	onUpdateSelected(true);

	if (_previewShown) {
		_previewShown = false;
		return;
	}

	if (_sel < 0 || _sel != pressed || _srows->isEmpty()) return;

	chooseSelected(FieldAutocomplete::ChooseMethod::ByClick);
}

void FieldAutocompleteInner::enterEventHook(QEvent *e) {
	setMouseTracking(true);
	_mousePos = QCursor::pos();
	onUpdateSelected(true);
}

void FieldAutocompleteInner::leaveEventHook(QEvent *e) {
	setMouseTracking(false);
	if (_sel >= 0) {
		setSel(-1);
	}
}

void FieldAutocompleteInner::updateSelectedRow() {
	if (_sel >= 0) {
		if (_srows->isEmpty()) {
			update(0, _sel * st::mentionHeight, width(), st::mentionHeight);
		} else {
			qint32 row = _sel / _stickersPerRow, col = _sel % _stickersPerRow;
			update(st::stickerPanPadding + col * st::stickerPanSize.width(),
			       st::stickerPanPadding + row * st::stickerPanSize.height(), st::stickerPanSize.width(),
			       st::stickerPanSize.height());
		}
	}
}

void FieldAutocompleteInner::setSel(int sel, bool scroll) {
	updateSelectedRow();
	_sel = sel;
	updateSelectedRow();

	if (scroll && _sel >= 0) {
		if (_srows->isEmpty()) {
			emit mustScrollTo(_sel * st::mentionHeight, (_sel + 1) * st::mentionHeight);
		} else {
			qint32 row = _sel / _stickersPerRow;
			emit mustScrollTo(st::stickerPanPadding + row * st::stickerPanSize.height(),
			                  st::stickerPanPadding + (row + 1) * st::stickerPanSize.height());
		}
	}
}

void FieldAutocompleteInner::onUpdateSelected(bool force) {
	QPoint mouse(mapFromGlobal(_mousePos));
	if ((!force && !rect().contains(mouse)) || !_mouseSel) return;

	if (_down >= 0 && !_previewShown) return;

	qint32 sel = -1, maxSel = 0;
	if (!_srows->isEmpty()) {
		qint32 row = (mouse.y() >= st::stickerPanPadding) ?
		                 ((mouse.y() - st::stickerPanPadding) / st::stickerPanSize.height()) :
		                 -1;
		qint32 col = (mouse.x() >= st::stickerPanPadding) ?
		                 ((mouse.x() - st::stickerPanPadding) / st::stickerPanSize.width()) :
		                 -1;
		if (row >= 0 && col >= 0) {
			sel = row * _stickersPerRow + col;
		}
		maxSel = _srows->size();
		_overDelete = false;
	} else {
		sel = mouse.y() / qint32(st::mentionHeight);
		maxSel = _mrows->isEmpty() ? (_hrows->isEmpty() ? _brows->size() : _hrows->size()) : _mrows->size();
		_overDelete = (!_hrows->isEmpty() || (!_mrows->isEmpty() && sel < _recentInlineBotsInRows)) ?
		                  (mouse.x() >= width() - st::mentionHeight) :
		                  false;
	}
	if (sel < 0 || sel >= maxSel) {
		sel = -1;
	}
	if (sel != _sel) {
		setSel(sel);
		if (_down >= 0 && _sel >= 0 && _down != _sel) {
			_down = _sel;
			if (_down >= 0 && _down < _srows->size()) {
				Ui::showMediaPreview(_srows->at(_down));
			}
		}
	}
}

void FieldAutocompleteInner::onParentGeometryChanged() {
	_mousePos = QCursor::pos();
	if (rect().contains(mapFromGlobal(_mousePos))) {
		setMouseTracking(true);
		onUpdateSelected(true);
	}
}

void FieldAutocompleteInner::onPreview() {
	if (_down >= 0 && _down < _srows->size()) {
		Ui::showMediaPreview(_srows->at(_down));
		_previewShown = true;
	}
}

} // namespace internal
