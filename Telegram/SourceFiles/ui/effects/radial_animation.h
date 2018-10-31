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

namespace Ui {

class RadialAnimation {
public:
	RadialAnimation(AnimationCallbacks &&callbacks);

	double opacity() const {
		return _opacity;
	}
	bool animating() const {
		return _animation.animating();
	}

	void start(double prg);
	void update(double prg, bool finished, TimeMs ms);
	void stop();

	void step(TimeMs ms);
	void step() {
		step(getms());
	}

	void draw(Painter &p, const QRect &inner, qint32 thickness, style::color color);

private:
	TimeMs _firstStart = 0;
	TimeMs _lastStart = 0;
	TimeMs _lastTime = 0;
	double _opacity = 0.;
	anim::value a_arcEnd;
	anim::value a_arcStart;
	BasicAnimation _animation;
};

} // namespace Ui
