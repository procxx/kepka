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
#include "media/view/media_clip_volume_controller.h"
#include "app.h"
#include "facades.h"
#include "styles/style_mediaview.h"
#include "ui/twidget.h"
#include <QMouseEvent>
#include <QWindow>

namespace Media {
namespace Clip {

VolumeController::VolumeController(QWidget *parent)
    : TWidget(parent) {
	resize(st::mediaviewVolumeSize);
	setCursor(style::cur_pointer);
	setMouseTracking(true);
}

void VolumeController::setVolume(double volume) {
	_volume = volume;
	update();
}

void VolumeController::paintEvent(QPaintEvent *e) {
	Painter p(this);

	qint32 top = st::mediaviewVolumeIconTop;
	qint32 left = (width() - st::mediaviewVolumeIcon.width()) / 2;
	qint32 mid = left + std::round(st::mediaviewVolumeIcon.width() * _volume);
	qint32 right = left + st::mediaviewVolumeIcon.width();

	if (mid > left) {
		p.setClipRect(rtlrect(left, top, mid - left, st::mediaviewVolumeIcon.height(), width()));
		auto over = _a_over.current(getms(), _over ? 1. : 0.);
		if (over < 1.) {
			st::mediaviewVolumeOnIcon.paint(p, QPoint(left, top), width());
		}
		if (over > 0.) {
			p.setOpacity(over);
			st::mediaviewVolumeOnIconOver.paint(p, QPoint(left, top), width());
			p.setOpacity(1.);
		}
	}
	if (right > mid) {
		p.setClipRect(rtlrect(mid, top, right - mid, st::mediaviewVolumeIcon.height(), width()));
		st::mediaviewVolumeIcon.paint(p, QPoint(left, top), width());
	}
}

void VolumeController::mouseMoveEvent(QMouseEvent *e) {
	if (_downCoord < 0) {
		return;
	}
	int delta = e->pos().x() - _downCoord;
	int left = (width() - st::mediaviewVolumeIcon.width()) / 2;
	double startFrom = snap((_downCoord - left) / double(st::mediaviewVolumeIcon.width()), 0., 1.);
	double add = delta / double(4 * st::mediaviewVolumeIcon.width());
	auto newVolume = snap(startFrom + add, 0., 1.);
	changeVolume(newVolume);
}

void VolumeController::mousePressEvent(QMouseEvent *e) {
	_downCoord = snap(e->pos().x(), 0, width());
	int left = (width() - st::mediaviewVolumeIcon.width()) / 2;
	auto newVolume = snap((_downCoord - left) / double(st::mediaviewVolumeIcon.width()), 0., 1.);
	changeVolume(newVolume);
}

void VolumeController::changeVolume(double newVolume) {
	if (newVolume != _volume) {
		setVolume(newVolume);
		emit volumeChanged(_volume);
	}
}

void VolumeController::mouseReleaseEvent(QMouseEvent *e) {
	_downCoord = -1;
}

void VolumeController::enterEventHook(QEvent *e) {
	setOver(true);
}

void VolumeController::leaveEventHook(QEvent *e) {
	setOver(false);
}

void VolumeController::setOver(bool over) {
	if (_over == over) return;

	_over = over;
	auto from = _over ? 0. : 1., to = _over ? 1. : 0.;
	_a_over.start([this] { update(); }, from, to, st::mediaviewOverDuration);
}

} // namespace Clip
} // namespace Media
