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
#include "ui/widgets/shadow.h"

namespace Ui {

void Shadow::paint(Painter &p, const QRect &box, int outerWidth, const style::Shadow &st, RectParts sides) {
	auto left = (sides & RectPart::Left);
	auto top = (sides & RectPart::Top);
	auto right = (sides & RectPart::Right);
	auto bottom = (sides & RectPart::Bottom);
	if (left) {
		auto from = box.y();
		auto to = from + box.height();
		if (top && !st.topLeft.empty()) {
			st.topLeft.paint(p, box.x() - st.extend.left(), box.y() - st.extend.top(), outerWidth);
			from += st.topLeft.height() - st.extend.top();
		}
		if (bottom && !st.bottomLeft.empty()) {
			st.bottomLeft.paint(p, box.x() - st.extend.left(),
			                    box.y() + box.height() + st.extend.bottom() - st.bottomLeft.height(), outerWidth);
			to -= st.bottomLeft.height() - st.extend.bottom();
		}
		if (to > from && !st.left.empty()) {
			st.left.fill(p, rtlrect(box.x() - st.extend.left(), from, st.left.width(), to - from, outerWidth));
		}
	}
	if (right) {
		auto from = box.y();
		auto to = from + box.height();
		if (top && !st.topRight.empty()) {
			st.topRight.paint(p, box.x() + box.width() + st.extend.right() - st.topRight.width(),
			                  box.y() - st.extend.top(), outerWidth);
			from += st.topRight.height() - st.extend.top();
		}
		if (bottom && !st.bottomRight.empty()) {
			st.bottomRight.paint(p, box.x() + box.width() + st.extend.right() - st.bottomRight.width(),
			                     box.y() + box.height() + st.extend.bottom() - st.bottomRight.height(), outerWidth);
			to -= st.bottomRight.height() - st.extend.bottom();
		}
		if (to > from && !st.right.empty()) {
			st.right.fill(p, rtlrect(box.x() + box.width() + st.extend.right() - st.right.width(), from,
			                         st.right.width(), to - from, outerWidth));
		}
	}
	if (top && !st.top.empty()) {
		auto from = box.x();
		auto to = from + box.width();
		if (left && !st.topLeft.empty()) from += st.topLeft.width() - st.extend.left();
		if (right && !st.topRight.empty()) to -= st.topRight.width() - st.extend.right();
		if (to > from) {
			st.top.fill(p, rtlrect(from, box.y() - st.extend.top(), to - from, st.top.height(), outerWidth));
		}
	}
	if (bottom && !st.bottom.empty()) {
		auto from = box.x();
		auto to = from + box.width();
		if (left && !st.bottomLeft.empty()) from += st.bottomLeft.width() - st.extend.left();
		if (right && !st.bottomRight.empty()) to -= st.bottomRight.width() - st.extend.right();
		if (to > from) {
			st.bottom.fill(p, rtlrect(from, box.y() + box.height() + st.extend.bottom() - st.bottom.height(), to - from,
			                          st.bottom.height(), outerWidth));
		}
	}
}

QPixmap Shadow::grab(TWidget *target, const style::Shadow &shadow, RectParts sides) {
	myEnsureResized(target);
	auto rect = target->rect();
	auto extend = QMargins(
	    (sides & RectPart::Left) ? shadow.extend.left() : 0, (sides & RectPart::Top) ? shadow.extend.top() : 0,
	    (sides & RectPart::Right) ? shadow.extend.right() : 0, (sides & RectPart::Bottom) ? shadow.extend.bottom() : 0);
	auto full =
	    QRect(0, 0, extend.left() + rect.width() + extend.right(), extend.top() + rect.height() + extend.bottom());
	auto result = QPixmap(full.size() * cIntRetinaFactor());
	result.setDevicePixelRatio(cRetinaFactor());
	result.fill(Qt::transparent);
	{
		Painter p(&result);
		Ui::Shadow::paint(p, full.marginsRemoved(extend), full.width(), shadow);
		target->grabStart();
		target->render(&p, QPoint(extend.left(), extend.top()), rect, QWidget::DrawChildren | QWidget::IgnoreMask);
		target->grabFinish();
	}
	return result;
}

} // namespace Ui
