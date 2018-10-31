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

#include "styles/style_widgets.h"
#include "ui/twidget.h"
#include <QPaintEvent>

namespace Ui {

class PlainShadow : public TWidget {
public:
	PlainShadow(QWidget *parent, style::color color)
	    : TWidget(parent)
	    , _color(color) {}

protected:
	void paintEvent(QPaintEvent *e) override {
		Painter(this).fillRect(e->rect(), _color->b);
	}

private:
	style::color _color;
};

class Shadow : public TWidget {
public:
	Shadow(QWidget *parent, const style::Shadow &st,
	       RectParts sides = RectPart::Left | RectPart::Top | RectPart::Right | RectPart::Bottom)
	    : TWidget(parent)
	    , _st(st)
	    , _sides(sides) {}

	static void paint(Painter &p, const QRect &box, int outerWidth, const style::Shadow &st,
	                  RectParts sides = RectPart::Left | RectPart::Top | RectPart::Right | RectPart::Bottom);

	static QPixmap grab(TWidget *target, const style::Shadow &shadow,
	                    RectParts sides = RectPart::Left | RectPart::Top | RectPart::Right | RectPart::Bottom);

protected:
	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		paint(p, rect().marginsRemoved(_st.extend), width(), _st, _sides);
	}

private:
	const style::Shadow &_st;
	RectParts _sides;
};

} // namespace Ui
