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

#include "base/observer.h"
#include "core/basic_types.h"
#include "structs.h"
#include "ui/twidget.h"

class HistoryItem;

namespace Overview {
namespace Layout {
class Document;
} // namespace Layout
} // namespace Overview

namespace Media {
namespace Player {

class ListWidget : public TWidget, private base::Subscriber {
public:
	ListWidget(QWidget *parent);

	void ui_repaintHistoryItem(not_null<const HistoryItem *> item);

	QRect getCurrentTrackGeometry() const;

	~ListWidget();

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

	int resizeGetHeight(int newWidth) override;

private:
	void itemRemoved(HistoryItem *item);
	int marginTop() const;
	void repaintItem(const HistoryItem *item);
	void playlistUpdated();

	using Layout = Overview::Layout::Document;
	using Layouts = QMap<FullMsgId, Layout *>;
	Layouts _layouts;

	using List = QList<Layout *>;
	List _list;

	style::cursor _cursor = style::cur_default;
};

} // namespace Player
} // namespace Media
