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
#include <QPixmap>

class Painter;

namespace Ui {

class SlideAnimation {
public:
	void setSnapshots(QPixmap leftSnapshot, QPixmap rightSnapshot);

	void setOverflowHidden(bool hidden) {
		_overflowHidden = hidden;
	}

	template <typename Lambda> void start(bool slideLeft, Lambda &&updateCallback, double duration);

	void paintFrame(Painter &p, int x, int y, int outerWidth, TimeMs ms);

	bool animating() const {
		return _animation.animating();
	}

private:
	Animation _animation;
	QPixmap _leftSnapshot;
	QPixmap _rightSnapshot;
	bool _slideLeft = false;
	bool _overflowHidden = true;
	int _leftSnapshotWidth = 0;
	int _leftSnapshotHeight = 0;
	int _rightSnapshotWidth = 0;
};

template <typename Lambda> void SlideAnimation::start(bool slideLeft, Lambda &&updateCallback, double duration) {
	_slideLeft = slideLeft;
	if (_slideLeft) {
		std::swap(_leftSnapshot, _rightSnapshot);
	}
	_leftSnapshotWidth = _leftSnapshot.width() / cIntRetinaFactor();
	_leftSnapshotHeight = _leftSnapshot.height() / cIntRetinaFactor();
	_rightSnapshotWidth = _rightSnapshot.width() / cIntRetinaFactor();
	_animation.start(std::forward<Lambda>(updateCallback), 0., 1., duration);
}

} // namespace Ui
