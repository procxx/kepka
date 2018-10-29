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
#include "core/single_timer.h"

#include "application.h"

SingleTimer::SingleTimer(QObject *parent)
    : QTimer(parent) {
	QTimer::setSingleShot(true);
	Sandbox::connect(SIGNAL(adjustSingleTimers()), this, SLOT(adjust()));
}

void SingleTimer::setTimeoutHandler(Fn<void()> handler) {
	if (_handler && !handler) {
		disconnect(this, SIGNAL(timeout()), this, SLOT(onTimeout()));
	} else if (handler && !_handler) {
		connect(this, SIGNAL(timeout()), this, SLOT(onTimeout()));
	}
	_handler = std::move(handler);
}

void SingleTimer::adjust() {
	auto n = getms(true);
	if (isActive()) {
		if (n >= _finishing) {
			start(0);
		} else {
			start(_finishing - n);
		}
	}
}

void SingleTimer::onTimeout() {
	if (_handler) {
		_handler();
	}
}

void SingleTimer::start(int msec) {
	_finishing = getms(true) + (msec < 0 ? 0 : msec);
	QTimer::start(msec);
}

void SingleTimer::startIfNotActive(int msec) {
	if (isActive()) {
		int remains = remainingTime();
		if (remains > msec) {
			start(msec);
		} else if (!remains) {
			start(1);
		}
	} else {
		start(msec);
	}
}
