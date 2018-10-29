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

#include "ui/text/text.h"
#include "ui/toast/toast.h"
#include "ui/twidget.h"

namespace Ui {
namespace Toast {
namespace internal {

class Widget : public TWidget {
public:
	Widget(QWidget *parent, const Config &config);

	// shownLevel=1 completely visible, shownLevel=0 completely invisible
	void setShownLevel(double shownLevel);

	void onParentResized();

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	double _shownLevel = 0;
	bool _multiline = false;
	int _maxWidth = 0;
	QMargins _padding;

	int _maxTextWidth = 0;
	int _textWidth = 0;
	Text _text;
};

} // namespace internal
} // namespace Toast
} // namespace Ui
