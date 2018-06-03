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
#include "ui/effects/cross_animation.h"
#include "ui/twidget.h"

#include <qmath.h>

namespace Ui {
namespace {

constexpr auto kPointCount = 12;

//
//     1         3
//    X X       X X
//   X   X     X   X
//  0     X   X     4
//   X     X X     X
//    X     2     X
//     X         X
//      X       X
//       11    5
//      X       X
//     X         X
//    X     8     X
//   X     X X     X
// 10     X   X     6
//   X   X     X   X
//    X X       X X
//     9         7
//

void transformLoadingCross(double loading, std::array<QPointF, kPointCount> &points, int &paintPointsCount) {
	auto moveTo = [](QPointF &point, QPointF &to, double ratio) {
		point = point * (1. - ratio) + to * ratio;
	};
	auto moveFrom = [](QPointF &point, QPointF &from, double ratio) {
		point = from * (1. - ratio) + point * ratio;
	};
	auto paintPoints = [&points, &paintPointsCount](std::initializer_list<int> &&indices) {
		auto index = 0;
		for (auto paintIndex : indices) {
			points[index++] = points[paintIndex];
		}
		paintPointsCount = indices.size();
	};

	if (loading < 0.125) {
		auto ratio = loading / 0.125;
		moveTo(points[6], points[5], ratio);
		moveTo(points[7], points[8], ratio);
	} else if (loading < 0.25) {
		auto ratio = (loading - 0.125) / 0.125;
		moveTo(points[9], points[8], ratio);
		moveTo(points[10], points[11], ratio);
		paintPoints({ 0, 1, 2, 3, 4, 9, 10, 11 });
	} else if (loading < 0.375) {
		auto ratio = (loading - 0.25) / 0.125;
		moveTo(points[0], points[11], ratio);
		moveTo(points[1], points[2], ratio);
		paintPoints({ 0, 1, 2, 3, 4, 8 });
	} else if (loading < 0.5) {
		auto ratio = (loading - 0.375) / 0.125;
		moveTo(points[8], points[4], ratio);
		moveTo(points[11], points[3], ratio);
		paintPoints({ 3, 4, 8, 11 });
	} else if (loading < 0.625) {
		auto ratio = (loading - 0.5) / 0.125;
		moveFrom(points[8], points[4], ratio);
		moveFrom(points[11], points[3], ratio);
		paintPoints({ 3, 4, 8, 11 });
	} else if (loading < 0.75) {
		auto ratio = (loading - 0.625) / 0.125;
		moveFrom(points[6], points[5], ratio);
		moveFrom(points[7], points[8], ratio);
		paintPoints({ 3, 4, 5, 6, 7, 11 });
	} else if (loading < 0.875) {
		auto ratio = (loading - 0.75) / 0.125;
		moveFrom(points[9], points[8], ratio);
		moveFrom(points[10], points[11], ratio);
		paintPoints({ 3, 4, 5, 6, 7, 8, 9, 10 });
	} else {
		auto ratio = (loading - 0.875) / 0.125;
		moveFrom(points[0], points[11], ratio);
		moveFrom(points[1], points[2], ratio);
	}
}

} // namespace

void CrossAnimation::paint(Painter &p, const style::CrossAnimation &st, style::color color, int x, int y, int outerWidth, double shown, double loading) {
	PainterHighQualityEnabler hq(p);

	auto sqrt2 = sqrt(2.);
	auto deleteScale = shown + st.minScale * (1. - shown);
	auto deleteSkip = (deleteScale * st.skip) + (1. - deleteScale) * (st.size / 2);
	auto deleteLeft = rtlpoint(x + deleteSkip, 0, outerWidth).x() + 0.;
	auto deleteTop = y + deleteSkip + 0.;
	auto deleteWidth = st.size - 2 * deleteSkip;
	auto deleteHeight = st.size - 2 * deleteSkip;
	auto deleteStroke = st.stroke / sqrt2;
	std::array<QPointF, kPointCount> pathDelete = { {
		{ deleteLeft, deleteTop + deleteStroke },
		{ deleteLeft + deleteStroke, deleteTop },
		{ deleteLeft + (deleteWidth / 2.), deleteTop + (deleteHeight / 2.) - deleteStroke },
		{ deleteLeft + deleteWidth - deleteStroke, deleteTop },
		{ deleteLeft + deleteWidth, deleteTop + deleteStroke },
		{ deleteLeft + (deleteWidth / 2.) + deleteStroke, deleteTop + (deleteHeight / 2.) },
		{ deleteLeft + deleteWidth, deleteTop + deleteHeight - deleteStroke },
		{ deleteLeft + deleteWidth - deleteStroke, deleteTop + deleteHeight },
		{ deleteLeft + (deleteWidth / 2.), deleteTop + (deleteHeight / 2.) + deleteStroke },
		{ deleteLeft + deleteStroke, deleteTop + deleteHeight },
		{ deleteLeft, deleteTop + deleteHeight - deleteStroke },
		{ deleteLeft + (deleteWidth / 2.) - deleteStroke, deleteTop + (deleteHeight / 2.) },
	} };
	auto pathDeleteSize = kPointCount;

	auto loadingArcLength = 0;
	if (loading > 0.) {
		transformLoadingCross(loading, pathDelete, pathDeleteSize);

		auto loadingArc = (loading >= 0.5) ? (loading - 1.) : loading;
		loadingArcLength = std::round(-loadingArc * 2 * FullArcLength);
	}

	if (shown < 1.) {
		auto alpha = -(shown - 1.) * M_PI_2;
		auto cosalpha = cos(alpha);
		auto sinalpha = sin(alpha);
		auto shiftx = deleteLeft + (deleteWidth / 2.);
		auto shifty = deleteTop + (deleteHeight / 2.);
		for (auto &point : pathDelete) {
			auto x = point.x() - shiftx;
			auto y = point.y() - shifty;
			point.setX(shiftx + x * cosalpha - y * sinalpha);
			point.setY(shifty + y * cosalpha + x * sinalpha);
		}
	}
	QPainterPath path;
	path.moveTo(pathDelete[0]);
	for (int i = 1; i != pathDeleteSize; ++i) {
		path.lineTo(pathDelete[i]);
	}
	path.lineTo(pathDelete[0]);
	p.fillPath(path, color);

	if (loadingArcLength != 0) {
		auto loadingArcStart = FullArcLength / 8;
		auto roundSkip = (st.size * (1 - sqrt2) + 2 * sqrt2 * deleteSkip + st.stroke) / 2;
		auto roundPart = QRectF(x + roundSkip, y + roundSkip, st.size - 2 * roundSkip, st.size - 2 * roundSkip);
		if (shown < 1.) {
			loadingArcStart -= std::round(-(shown - 1.) * FullArcLength / 4.);
		}
		p.setBrush(Qt::NoBrush);
		auto pen = color->p;
		pen.setWidthF(st.stroke);
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		if (loadingArcLength < 0) {
			loadingArcStart += loadingArcLength;
			loadingArcLength = -loadingArcLength;
		}
		p.drawArc(roundPart, loadingArcStart, loadingArcLength);
	}
}

} // namespace Ui
