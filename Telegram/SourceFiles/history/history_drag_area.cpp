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
#include "history/history_drag_area.h"

#include "apiwrap.h"
#include "boxes/confirm_box.h"
#include "boxes/sticker_set_box.h"
#include "dialogs/dialogs_layout.h"
#include "history/history_widget.h"
#include "inline_bots/inline_bot_layout_item.h"
#include "inline_bots/inline_bot_result.h"
#include "lang/lang_keys.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "storage/localstorage.h"
#include "styles/style_boxes.h"
#include "styles/style_chat_helpers.h"

DragArea::DragArea(QWidget *parent)
    : TWidget(parent) {
	setMouseTracking(true);
	setAcceptDrops(true);
}

bool DragArea::overlaps(const QRect &globalRect) {
	if (isHidden() || _a_opacity.animating()) {
		return false;
	}

	auto inner = innerRect();
	auto testRect = QRect(mapFromGlobal(globalRect.topLeft()), globalRect.size());
	return inner.marginsRemoved(QMargins(st::boxRadius, 0, st::boxRadius, 0)).contains(testRect) ||
	       inner.marginsRemoved(QMargins(0, st::boxRadius, 0, st::boxRadius)).contains(testRect);
}


void DragArea::mouseMoveEvent(QMouseEvent *e) {
	if (_hiding) return;

	auto in =
	    QRect(st::dragPadding.left(), st::dragPadding.top(), width() - st::dragPadding.left() - st::dragPadding.right(),
	          height() - st::dragPadding.top() - st::dragPadding.bottom())
	        .contains(e->pos());
	setIn(in);
}

void DragArea::dragMoveEvent(QDragMoveEvent *e) {
	QRect r(st::dragPadding.left(), st::dragPadding.top(), width() - st::dragPadding.left() - st::dragPadding.right(),
	        height() - st::dragPadding.top() - st::dragPadding.bottom());
	setIn(r.contains(e->pos()));
	e->setDropAction(_in ? Qt::CopyAction : Qt::IgnoreAction);
	e->accept();
}

void DragArea::setIn(bool in) {
	if (_in != in) {
		_in = in;
		_a_in.start([this] { update(); }, _in ? 0. : 1., _in ? 1. : 0., st::boxDuration);
	}
}

void DragArea::setText(const QString &text, const QString &subtext) {
	_text = text;
	_subtext = subtext;
	update();
}

void DragArea::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();
	auto opacity = _a_opacity.current(ms, _hiding ? 0. : 1.);
	if (!_a_opacity.animating() && _hiding) {
		return;
	}
	p.setOpacity(opacity);
	auto inner = innerRect();

	if (!_cache.isNull()) {
		p.drawPixmapLeft(inner.x() - st::boxRoundShadow.extend.left(), inner.y() - st::boxRoundShadow.extend.top(),
		                 width(), _cache);
		return;
	}

	Ui::Shadow::paint(p, inner, width(), st::boxRoundShadow);
	App::roundRect(p, inner, st::boxBg, BoxCorners);

	p.setPen(anim::pen(st::dragColor, st::dragDropColor, _a_in.current(ms, _in ? 1. : 0.)));

	p.setFont(st::dragFont);
	p.drawText(QRect(0, (height() - st::dragHeight) / 2, width(), st::dragFont->height), _text,
	           QTextOption(style::al_top));

	p.setFont(st::dragSubfont);
	p.drawText(
	    QRect(0, (height() + st::dragHeight) / 2 - st::dragSubfont->height, width(), st::dragSubfont->height * 2),
	    _subtext, QTextOption(style::al_top));
}

void DragArea::dragEnterEvent(QDragEnterEvent *e) {
	static_cast<HistoryWidget *>(parentWidget())->dragEnterEvent(e);
	e->setDropAction(Qt::IgnoreAction);
	e->accept();
}

void DragArea::dragLeaveEvent(QDragLeaveEvent *e) {
	static_cast<HistoryWidget *>(parentWidget())->dragLeaveEvent(e);
	setIn(false);
}

void DragArea::dropEvent(QDropEvent *e) {
	static_cast<HistoryWidget *>(parentWidget())->dropEvent(e);
	if (e->isAccepted() && _droppedCallback) {
		_droppedCallback(e->mimeData());
	}
}

void DragArea::otherEnter() {
	showStart();
}

void DragArea::otherLeave() {
	hideStart();
}

void DragArea::hideFast() {
	_a_opacity.finish();
	hide();
}

void DragArea::hideStart() {
	if (_hiding || isHidden()) {
		return;
	}
	if (_cache.isNull()) {
		_cache = myGrab(this, innerRect().marginsAdded(st::boxRoundShadow.extend));
	}
	_hiding = true;
	setIn(false);
	_a_opacity.start([this] { opacityAnimationCallback(); }, 1., 0., st::boxDuration);
}

void DragArea::hideFinish() {
	hide();
	_in = false;
	_a_in.finish();
}

void DragArea::showStart() {
	if (!_hiding && !isHidden()) {
		return;
	}
	_hiding = false;
	if (_cache.isNull()) {
		_cache = myGrab(this, innerRect().marginsAdded(st::boxRoundShadow.extend));
	}
	show();
	_a_opacity.start([this] { opacityAnimationCallback(); }, 0., 1., st::boxDuration);
}

void DragArea::opacityAnimationCallback() {
	update();
	if (!_a_opacity.animating()) {
		_cache = QPixmap();
		if (_hiding) {
			hideFinish();
		}
	}
}
