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

#include "styles/style_media_player.h"
#include "ui/abstract_button.h"
#include "ui/animation.h"

namespace Media {
namespace Player {

class PlayButtonLayout {
public:
	enum class State {
		Play,
		Pause,
		Cancel,
	};
	PlayButtonLayout(const style::MediaPlayerButton &st, Fn<void()> callback);

	void setState(State state);
	void finishTransform();
	void paint(Painter &p, const QBrush &brush);

private:
	void animationCallback();
	void startTransform(double from, double to);

	void paintPlay(Painter &p, const QBrush &brush);
	void paintPlayToPause(Painter &p, const QBrush &brush, double progress);
	void paintPlayToCancel(Painter &p, const QBrush &brush, double progress);
	void paintPauseToCancel(Painter &p, const QBrush &brush, double progress);

	const style::MediaPlayerButton &_st;

	State _state = State::Play;
	State _oldState = State::Play;
	State _nextState = State::Play;
	Animation _transformProgress;
	bool _transformBackward = false;

	Fn<void()> _callback;
};

} // namespace Player
} // namespace Media
