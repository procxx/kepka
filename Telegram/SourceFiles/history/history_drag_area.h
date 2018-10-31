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

#include "ui/animation.h"
#include "ui/twidget.h"
#include <QMimeData>

class DragArea : public TWidget {
	Q_OBJECT

public:
	DragArea(QWidget *parent);

	void setText(const QString &text, const QString &subtext);

	void otherEnter();
	void otherLeave();

	bool overlaps(const QRect &globalRect);

	void hideFast();

	void setDroppedCallback(Fn<void(const QMimeData *data)> callback) {
		_droppedCallback = std::move(callback);
	}

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void dragEnterEvent(QDragEnterEvent *e) override;
	void dragLeaveEvent(QDragLeaveEvent *e) override;
	void dropEvent(QDropEvent *e) override;
	void dragMoveEvent(QDragMoveEvent *e) override;

public slots:
	void hideStart();
	void hideFinish();

	void showStart();

private:
	void setIn(bool in);
	void opacityAnimationCallback();
	QRect innerRect() const {
		return QRect(st::dragPadding.left(), st::dragPadding.top(),
		             width() - st::dragPadding.left() - st::dragPadding.right(),
		             height() - st::dragPadding.top() - st::dragPadding.bottom());
	}

	bool _hiding = false;
	bool _in = false;
	QPixmap _cache;
	Fn<void(const QMimeData *data)> _droppedCallback;

	Animation _a_opacity;
	Animation _a_in;

	QString _text, _subtext;
};
