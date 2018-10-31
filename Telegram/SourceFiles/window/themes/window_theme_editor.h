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

#include "ui/twidget.h"
#include "ui/widgets/scroll_area.h"

#include <QPointer>

class QWidget;
class QString;
class QPaintEvent;
class QResizeEvent;
class QKeyEvent;
class QFocusEvent;

class BoxLayerTitleShadow;

namespace Ui {
class FlatButton;
class ScrollArea;
class CrossButton;
class MultiSelect;
} // namespace Ui

namespace Window {
namespace Theme {

class Editor : public TWidget {
	Q_OBJECT

public:
	Editor(QWidget *, const QString &path);

	static void Start();

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

	void focusInEvent(QFocusEvent *e) override;

private:
	void closeEditor();

	object_ptr<Ui::ScrollArea> _scroll;
	class Inner;
	QPointer<Inner> _inner;
	object_ptr<Ui::CrossButton> _close;
	object_ptr<Ui::MultiSelect> _select;
	object_ptr<BoxLayerTitleShadow> _leftShadow;
	object_ptr<BoxLayerTitleShadow> _topShadow;
	object_ptr<Ui::FlatButton> _export;
};

} // namespace Theme
} // namespace Window
