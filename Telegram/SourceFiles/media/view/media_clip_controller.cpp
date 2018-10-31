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
#include "media/view/media_clip_controller.h"
#include "app.h"
#include "facades.h"
#include "media/media_audio.h"
#include "media/view/media_clip_playback.h"
#include "media/view/media_clip_volume_controller.h"
#include "styles/style_mediaview.h"
#include "ui/effects/widget_fade_wrap.h"
#include "ui/twidget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include <QWindow>

namespace Media {
namespace Clip {

Controller::Controller(QWidget *parent)
    : TWidget(parent)
    , _playPauseResume(this, st::mediaviewPlayButton)
    , _playbackSlider(this, st::mediaviewPlayback)
    , _playback(std::make_unique<Playback>())
    , _volumeController(this)
    , _fullScreenToggle(this, st::mediaviewFullScreenButton)
    , _playedAlready(this, st::mediaviewPlayProgressLabel)
    , _toPlayLeft(this, st::mediaviewPlayProgressLabel)
    , _fadeAnimation(std::make_unique<Ui::FadeAnimation>(this)) {
	_fadeAnimation->show();
	_fadeAnimation->setFinishedCallback([this] { fadeFinished(); });
	_fadeAnimation->setUpdatedCallback([this](double opacity) { fadeUpdated(opacity); });

	_volumeController->setVolume(Global::VideoVolume());

	connect(_playPauseResume, SIGNAL(clicked()), this, SIGNAL(playPressed()));
	connect(_fullScreenToggle, SIGNAL(clicked()), this, SIGNAL(toFullScreenPressed()));
	connect(_volumeController, SIGNAL(volumeChanged(double)), this, SIGNAL(volumeChanged(double)));

	_playback->setInLoadingStateChangedCallback([this](bool loading) { _playbackSlider->setDisabled(loading); });
	_playback->setValueChangedCallback([this](double value) { _playbackSlider->setValue(value); });
	_playbackSlider->setChangeProgressCallback([this](double value) {
		_playback->setValue(value, false);
		handleSeekProgress(value); // This may destroy Controller.
	});
	_playbackSlider->setChangeFinishedCallback([this](double value) {
		_playback->setValue(value, false);
		handleSeekFinished(value);
	});
}

void Controller::handleSeekProgress(double progress) {
	if (!_lastDurationMs) return;

	auto positionMs = snap(static_cast<TimeMs>(progress * _lastDurationMs), Q_INT64_C(0), _lastDurationMs);
	if (_seekPositionMs != positionMs) {
		_seekPositionMs = positionMs;
		refreshTimeTexts();
		emit seekProgress(positionMs); // This may destroy Controller.
	}
}

void Controller::handleSeekFinished(double progress) {
	if (!_lastDurationMs) return;

	auto positionMs = snap(static_cast<TimeMs>(progress * _lastDurationMs), Q_INT64_C(0), _lastDurationMs);
	_seekPositionMs = -1;
	emit seekFinished(positionMs);
	refreshTimeTexts();
}

void Controller::showAnimated() {
	startFading([this]() { _fadeAnimation->fadeIn(st::mediaviewShowDuration); });
}

void Controller::hideAnimated() {
	startFading([this]() { _fadeAnimation->fadeOut(st::mediaviewHideDuration); });
}

template <typename Callback> void Controller::startFading(Callback start) {
	start();
	_playbackSlider->show();
}

void Controller::fadeFinished() {
	fadeUpdated(1.);
}

void Controller::fadeUpdated(double opacity) {
	_playbackSlider->setFadeOpacity(opacity);
}

void Controller::updatePlayback(const Player::TrackState &state) {
	updatePlayPauseResumeState(state);
	_playback->updateState(state);
	updateTimeTexts(state);
}

void Controller::updatePlayPauseResumeState(const Player::TrackState &state) {
	auto showPause =
	    (state.state == Player::State::Playing || state.state == Player::State::Resuming || _seekPositionMs >= 0);
	if (showPause != _showPause) {
		disconnect(_playPauseResume, SIGNAL(clicked()), this,
		           _showPause ? SIGNAL(pausePressed()) : SIGNAL(playPressed()));
		_showPause = showPause;
		connect(_playPauseResume, SIGNAL(clicked()), this, _showPause ? SIGNAL(pausePressed()) : SIGNAL(playPressed()));

		_playPauseResume->setIconOverride(_showPause ? &st::mediaviewPauseIcon : nullptr,
		                                  _showPause ? &st::mediaviewPauseIconOver : nullptr);
	}
}

void Controller::updateTimeTexts(const Player::TrackState &state) {
	qint64 position = 0;

	if (Player::IsStoppedAtEnd(state.state)) {
		position = state.length;
	} else if (!Player::IsStoppedOrStopping(state.state)) {
		position = state.position;
	} else {
		position = 0;
	}
	auto playFrequency = state.frequency;
	auto playAlready = position / playFrequency;
	auto playLeft = (state.length / playFrequency) - playAlready;

	_lastDurationMs = (state.length * 1000LL) / playFrequency;

	_timeAlready = formatDurationText(playAlready);
	auto minus = QChar(8722);
	_timeLeft = minus + formatDurationText(playLeft);

	if (_seekPositionMs < 0) {
		refreshTimeTexts();
	}
}

void Controller::refreshTimeTexts() {
	auto alreadyChanged = false, leftChanged = false;
	auto timeAlready = _timeAlready;
	auto timeLeft = _timeLeft;
	if (_seekPositionMs >= 0) {
		auto playAlready = _seekPositionMs / 1000LL;
		auto playLeft = (_lastDurationMs / 1000LL) - playAlready;

		timeAlready = formatDurationText(playAlready);
		auto minus = QChar(8722);
		timeLeft = minus + formatDurationText(playLeft);
	}

	_playedAlready->setText(timeAlready, &alreadyChanged);
	_toPlayLeft->setText(timeLeft, &leftChanged);
	if (alreadyChanged || leftChanged) {
		resizeEvent(nullptr);
		_fadeAnimation->refreshCache();
	}
}

void Controller::setInFullScreen(bool inFullScreen) {
	_fullScreenToggle->setIconOverride(inFullScreen ? &st::mediaviewFullScreenOutIcon : nullptr,
	                                   inFullScreen ? &st::mediaviewFullScreenOutIconOver : nullptr);
	disconnect(_fullScreenToggle, SIGNAL(clicked()), this, SIGNAL(toFullScreenPressed()));
	disconnect(_fullScreenToggle, SIGNAL(clicked()), this, SIGNAL(fromFullScreenPressed()));

	auto handler = inFullScreen ? SIGNAL(fromFullScreenPressed()) : SIGNAL(toFullScreenPressed());
	connect(_fullScreenToggle, SIGNAL(clicked()), this, handler);
}

void Controller::grabStart() {
	showChildren();
	_playbackSlider->hide();
}

void Controller::grabFinish() {
	hideChildren();
	_playbackSlider->show();
}

void Controller::resizeEvent(QResizeEvent *e) {
	int playTop = (height() - _playPauseResume->height()) / 2;
	_playPauseResume->moveToLeft(st::mediaviewPlayPauseLeft, playTop);

	int fullScreenTop = (height() - _fullScreenToggle->height()) / 2;
	_fullScreenToggle->moveToRight(st::mediaviewFullScreenLeft, fullScreenTop);

	_volumeController->moveToRight(st::mediaviewFullScreenLeft + _fullScreenToggle->width() + st::mediaviewVolumeLeft,
	                               (height() - _volumeController->height()) / 2);

	auto playbackWidth = width() - st::mediaviewPlayPauseLeft - _playPauseResume->width() - playTop - fullScreenTop -
	                     _volumeController->width() - st::mediaviewVolumeLeft - _fullScreenToggle->width() -
	                     st::mediaviewFullScreenLeft;
	_playbackSlider->resize(playbackWidth, st::mediaviewPlayback.seekSize.height());
	_playbackSlider->moveToLeft(st::mediaviewPlayPauseLeft + _playPauseResume->width() + playTop,
	                            st::mediaviewPlaybackTop);

	_playedAlready->moveToLeft(st::mediaviewPlayPauseLeft + _playPauseResume->width() + playTop,
	                           st::mediaviewPlayProgressTop);
	_toPlayLeft->moveToRight(width() - (st::mediaviewPlayPauseLeft + _playPauseResume->width() + playTop) -
	                             playbackWidth,
	                         st::mediaviewPlayProgressTop);
}

void Controller::paintEvent(QPaintEvent *e) {
	Painter p(this);

	if (_fadeAnimation->paint(p)) {
		return;
	}

	App::roundRect(p, rect(), st::mediaviewSaveMsgBg, MediaviewSaveCorners);
}

void Controller::mousePressEvent(QMouseEvent *e) {
	e->accept(); // Don't pass event to the MediaView.
}

Controller::~Controller() = default;

} // namespace Clip
} // namespace Media
