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
#include "ui/widgets/continuous_sliders.h"

namespace Media {
namespace Player {
struct TrackState;
} // namespace Player

namespace Clip {

class Playback {
public:
	Playback();

	void setValueChangedCallback(Fn<void(double)> callback) {
		_valueChanged = std::move(callback);
	}
	void setInLoadingStateChangedCallback(Fn<void(bool)> callback) {
		_inLoadingStateChanged = std::move(callback);
	}
	void setValue(double value, bool animated);
	double value() const;
	double value(TimeMs ms);

	void updateState(const Player::TrackState &state);
	void updateLoadingState(double progress);

private:
	void step_value(double ms, bool timer);

	// This can animate for a very long time (like in music playing),
	// so it should be a BasicAnimation, not an Animation.
	anim::value a_value;
	BasicAnimation _a_value;
	Fn<void(double)> _valueChanged;

	bool _inLoadingState = false;
	Fn<void(bool)> _inLoadingStateChanged;

	qint64 _position = 0;
	qint64 _length = 0;

	bool _playing = false;
};

} // namespace Clip
} // namespace Media
