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
#include "media/player/media_player_widget.h"
#include "app.h"
#include "facades.h"
#include "lang/lang_keys.h"
#include "media/media_audio.h"
#include "media/player/media_player_button.h"
#include "media/player/media_player_instance.h"
#include "media/player/media_player_volume_controller.h"
#include "media/view/media_clip_playback.h"
#include "styles/style_media_player.h"
#include "styles/style_mediaview.h"
#include "ui/effects/ripple_animation.h"
#include "ui/twidget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/shadow.h"
#include <QWindow>

namespace Media {
namespace Player {

using ButtonState = PlayButtonLayout::State;

class Widget::PlayButton : public Ui::RippleButton {
public:
	PlayButton(QWidget *parent);

	void setState(PlayButtonLayout::State state) {
		_layout.setState(state);
	}
	void finishTransform() {
		_layout.finishTransform();
	}

protected:
	void paintEvent(QPaintEvent *e) override;

	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

private:
	PlayButtonLayout _layout;
};

Widget::PlayButton::PlayButton(QWidget *parent)
    : Ui::RippleButton(parent, st::mediaPlayerButton.ripple)
    , _layout(st::mediaPlayerButton, [this] { update(); }) {
	resize(st::mediaPlayerButtonSize);
	setCursor(style::cur_pointer);
}

void Widget::PlayButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	paintRipple(p, st::mediaPlayerButton.rippleAreaPosition.x(), st::mediaPlayerButton.rippleAreaPosition.y(), getms());
	p.translate(st::mediaPlayerButtonPosition.x(), st::mediaPlayerButtonPosition.y());
	_layout.paint(p, st::mediaPlayerActiveFg);
}

QImage Widget::PlayButton::prepareRippleMask() const {
	auto size = QSize(st::mediaPlayerButton.rippleAreaSize, st::mediaPlayerButton.rippleAreaSize);
	return Ui::RippleAnimation::ellipseMask(size);
}

QPoint Widget::PlayButton::prepareRippleStartPosition() const {
	return QPoint(mapFromGlobal(QCursor::pos()) - st::mediaPlayerButton.rippleAreaPosition);
}

Widget::Widget(QWidget *parent)
    : TWidget(parent)
    , _nameLabel(this, st::mediaPlayerName)
    , _timeLabel(this, st::mediaPlayerTime)
    , _playPause(this)
    , _volumeToggle(this, st::mediaPlayerVolumeToggle)
    , _repeatTrack(this, st::mediaPlayerRepeatButton)
    , _close(this, st::mediaPlayerClose)
    , _shadow(this, st::shadowFg)
    , _playbackSlider(this, st::mediaPlayerPlayback)
    , _playback(std::make_unique<Clip::Playback>()) {
	setAttribute(Qt::WA_OpaquePaintEvent);
	setMouseTracking(true);
	resize(width(), st::mediaPlayerHeight + st::lineWidth);

	_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	_timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	_playback->setInLoadingStateChangedCallback([this](bool loading) { _playbackSlider->setDisabled(loading); });
	_playback->setValueChangedCallback([this](double value) { _playbackSlider->setValue(value); });
	_playbackSlider->setChangeProgressCallback([this](double value) {
		if (_type != AudioMsgId::Type::Song) {
			return; // Round video seek is not supported for now :(
		}
		_playback->setValue(value, false);
		handleSeekProgress(value);
	});
	_playbackSlider->setChangeFinishedCallback([this](double value) {
		if (_type != AudioMsgId::Type::Song) {
			return; // Round video seek is not supported for now :(
		}
		_playback->setValue(value, false);
		handleSeekFinished(value);
	});
	_playPause->setClickedCallback([this] { instance()->playPauseCancelClicked(_type); });

	updateVolumeToggleIcon();
	_volumeToggle->setClickedCallback([] {
		Global::SetSongVolume((Global::SongVolume() > 0) ? 0. : Global::RememberedSongVolume());
		mixer()->setSongVolume(Global::SongVolume());
		Global::RefSongVolumeChanged().notify();
	});
	subscribe(Global::RefSongVolumeChanged(), [this] { updateVolumeToggleIcon(); });

	updateRepeatTrackIcon();
	_repeatTrack->setClickedCallback([] { instance()->toggleRepeat(AudioMsgId::Type::Song); });

	subscribe(instance()->repeatChangedNotifier(), [this](AudioMsgId::Type type) {
		if (type == _type) {
			updateRepeatTrackIcon();
		}
	});
	subscribe(instance()->playlistChangedNotifier(), [this](AudioMsgId::Type type) {
		if (type == _type) {
			handlePlaylistUpdate();
		}
	});
	subscribe(instance()->updatedNotifier(), [this](const TrackState &state) { handleSongUpdate(state); });
	subscribe(instance()->trackChangedNotifier(), [this](AudioMsgId::Type type) {
		if (type == _type) {
			handleSongChange();
		}
	});
	subscribe(instance()->tracksFinishedNotifier(), [this](AudioMsgId::Type type) {
		if (type == AudioMsgId::Type::Voice) {
			_voiceIsActive = false;
			auto currentSong = instance()->current(AudioMsgId::Type::Song);
			auto songState = mixer()->currentState(AudioMsgId::Type::Song);
			if (currentSong == songState.id && !IsStoppedOrStopping(songState.state)) {
				setType(AudioMsgId::Type::Song);
			}
		}
	});
	setType(AudioMsgId::Type::Song);
	_playPause->finishTransform();
}

void Widget::updateVolumeToggleIcon() {
	auto icon = []() -> const style::icon * {
		auto volume = Global::SongVolume();
		if (volume > 0) {
			if (volume < 1 / 3.) {
				return &st::mediaPlayerVolumeIcon1;
			} else if (volume < 2 / 3.) {
				return &st::mediaPlayerVolumeIcon2;
			}
			return &st::mediaPlayerVolumeIcon3;
		}
		return nullptr;
	};
	_volumeToggle->setIconOverride(icon());
}

void Widget::setCloseCallback(Fn<void()> callback) {
	_closeCallback = std::move(callback);
	_close->setClickedCallback([this] { stopAndClose(); });
}

void Widget::stopAndClose() {
	_voiceIsActive = false;
	if (_type == AudioMsgId::Type::Voice) {
		auto songData = instance()->current(AudioMsgId::Type::Song);
		auto songState = mixer()->currentState(AudioMsgId::Type::Song);
		if (songData == songState.id && !IsStoppedOrStopping(songState.state)) {
			instance()->stop(AudioMsgId::Type::Voice);
			return;
		}
	}
	if (_closeCallback) {
		_closeCallback();
	}
}

void Widget::setShadowGeometryToLeft(int x, int y, int w, int h) {
	_shadow->setGeometryToLeft(x, y, w, h);
}

void Widget::showShadow() {
	_shadow->show();
	_playbackSlider->setVisible(_type == AudioMsgId::Type::Song);
}

void Widget::hideShadow() {
	_shadow->hide();
	_playbackSlider->hide();
}

QPoint Widget::getPositionForVolumeWidget() const {
	auto x = _volumeToggle->x();
	x += (_volumeToggle->width() - st::mediaPlayerVolumeSize.width()) / 2;
	if (rtl()) x = width() - x - st::mediaPlayerVolumeSize.width();
	return QPoint(x, height());
}

void Widget::volumeWidgetCreated(VolumeWidget *widget) {
	_volumeToggle->installEventFilter(widget);
}

Widget::~Widget() = default;

void Widget::handleSeekProgress(double progress) {
	if (!_lastDurationMs) return;

	auto positionMs = snap(static_cast<TimeMs>(progress * _lastDurationMs), Q_INT64_C(0), _lastDurationMs);
	if (_seekPositionMs != positionMs) {
		_seekPositionMs = positionMs;
		updateTimeLabel();

		instance()->startSeeking(_type);
	}
}

void Widget::handleSeekFinished(double progress) {
	if (!_lastDurationMs) return;

	_seekPositionMs = -1;

	auto state = mixer()->currentState(_type);
	if (state.id && state.length) {
		mixer()->seek(_type, std::round(progress * state.length));
	}

	instance()->stopSeeking(_type);
}

void Widget::resizeEvent(QResizeEvent *e) {
	auto right = st::mediaPlayerCloseRight;
	_close->moveToRight(right, st::mediaPlayerPlayTop);
	right += _close->width();
	_repeatTrack->moveToRight(right, st::mediaPlayerPlayTop);
	right += _repeatTrack->width();
	_volumeToggle->moveToRight(right, st::mediaPlayerPlayTop);
	right += _volumeToggle->width();

	updatePlayPrevNextPositions();

	_playbackSlider->setGeometry(0, height() - st::mediaPlayerPlayback.fullWidth, width(),
	                             st::mediaPlayerPlayback.fullWidth);
}

void Widget::paintEvent(QPaintEvent *e) {
	Painter p(this);
	auto fill = e->rect().intersected(QRect(0, 0, width(), st::mediaPlayerHeight));
	if (!fill.isEmpty()) {
		p.fillRect(fill, st::mediaPlayerBg);
	}
}

void Widget::leaveEventHook(QEvent *e) {
	updateOverLabelsState(false);
}

void Widget::mouseMoveEvent(QMouseEvent *e) {
	updateOverLabelsState(e->pos());
}

void Widget::mousePressEvent(QMouseEvent *e) {
	_labelsDown = _labelsOver;
}

void Widget::mouseReleaseEvent(QMouseEvent *e) {
	if (auto downLabels = base::take(_labelsDown)) {
		if (_labelsOver == downLabels) {
			if (_type == AudioMsgId::Type::Voice) {
				auto current = instance()->current(_type);
				if (auto item = App::histItemById(current.contextId())) {
					Ui::showPeerHistoryAtItem(item);
				}
			}
		}
	}
}

void Widget::updateOverLabelsState(QPoint pos) {
	auto left = getLabelsLeft();
	auto right = getLabelsRight();
	auto labels = myrtlrect(left, 0, width() - right - left, height() - st::mediaPlayerPlayback.fullWidth);
	auto over = labels.contains(pos);
	updateOverLabelsState(over);
}

void Widget::updateOverLabelsState(bool over) {
	_labelsOver = over;
	auto pressShowsItem = _labelsOver && (_type == AudioMsgId::Type::Voice);
	setCursor(pressShowsItem ? style::cur_pointer : style::cur_default);
	auto showPlaylist = over && (_type == AudioMsgId::Type::Song);
	instance()->playerWidgetOver().notify(showPlaylist, true);
}

void Widget::updatePlayPrevNextPositions() {
	auto left = st::mediaPlayerPlayLeft;
	auto top = st::mediaPlayerPlayTop;
	if (_previousTrack) {
		_previousTrack->moveToLeft(left, top);
		left += _previousTrack->width() + st::mediaPlayerPlaySkip;
		_playPause->moveToLeft(left, top);
		left += _playPause->width() + st::mediaPlayerPlaySkip;
		_nextTrack->moveToLeft(left, top);
	} else {
		_playPause->moveToLeft(left, top);
	}
	updateLabelsGeometry();
}

int Widget::getLabelsLeft() const {
	auto result = st::mediaPlayerPlayLeft + _playPause->width();
	if (_previousTrack) {
		result += _previousTrack->width() + st::mediaPlayerPlaySkip + _nextTrack->width() + st::mediaPlayerPlaySkip;
	}
	result += st::mediaPlayerPadding;
	return result;
}

int Widget::getLabelsRight() const {
	auto result = st::mediaPlayerCloseRight + _close->width();
	if (_type == AudioMsgId::Type::Song) {
		result += _repeatTrack->width() + _volumeToggle->width();
	}
	result += st::mediaPlayerPadding;
	return result;
}

void Widget::updateLabelsGeometry() {
	auto left = getLabelsLeft();
	auto right = getLabelsRight();

	auto widthForName = width() - left - right;
	widthForName -= _timeLabel->width() + 2 * st::normalFont->spacew;
	_nameLabel->resizeToWidth(widthForName);

	_nameLabel->moveToLeft(left, st::mediaPlayerNameTop - st::mediaPlayerName.style.font->ascent);
	_timeLabel->moveToRight(right, st::mediaPlayerNameTop - st::mediaPlayerTime.font->ascent);
}

void Widget::updateRepeatTrackIcon() {
	auto repeating = instance()->repeatEnabled(AudioMsgId::Type::Song);
	_repeatTrack->setIconOverride(repeating ? nullptr : &st::mediaPlayerRepeatDisabledIcon,
	                              repeating ? nullptr : &st::mediaPlayerRepeatDisabledIconOver);
	_repeatTrack->setRippleColorOverride(repeating ? nullptr : &st::mediaPlayerRepeatDisabledRippleBg);
}

void Widget::checkForTypeChange() {
	auto hasActiveType = [](AudioMsgId::Type type) {
		auto current = instance()->current(type);
		auto state = mixer()->currentState(type);
		return (current == state.id && !IsStoppedOrStopping(state.state));
	};
	if (hasActiveType(AudioMsgId::Type::Voice)) {
		_voiceIsActive = true;
		setType(AudioMsgId::Type::Voice);
	} else if (!_voiceIsActive && hasActiveType(AudioMsgId::Type::Song)) {
		setType(AudioMsgId::Type::Song);
	}
}

void Widget::setType(AudioMsgId::Type type) {
	if (_type != type) {
		_type = type;
		_repeatTrack->setVisible(_type == AudioMsgId::Type::Song);
		_volumeToggle->setVisible(_type == AudioMsgId::Type::Song);
		if (!_shadow->isHidden()) {
			_playbackSlider->setVisible(_type == AudioMsgId::Type::Song);
		}
		updateLabelsGeometry();
		handleSongChange();
		handleSongUpdate(mixer()->currentState(_type));
		updateOverLabelsState(_labelsOver);
	}
}

void Widget::handleSongUpdate(const TrackState &state) {
	checkForTypeChange();
	if (state.id.type() != _type || !state.id.audio()) {
		return;
	}

	if (state.id.audio()->loading()) {
		_playback->updateLoadingState(state.id.audio()->progress());
	} else {
		_playback->updateState(state);
	}

	auto stopped = IsStoppedOrStopping(state.state);
	auto showPause =
	    !stopped && (state.state == State::Playing || state.state == State::Resuming || state.state == State::Starting);
	if (instance()->isSeeking(_type)) {
		showPause = true;
	}
	auto buttonState = [audio = state.id.audio(), showPause] {
		if (audio->loading()) {
			return ButtonState::Cancel;
		} else if (showPause) {
			return ButtonState::Pause;
		}
		return ButtonState::Play;
	};
	_playPause->setState(buttonState());

	updateTimeText(state);
}

void Widget::updateTimeText(const TrackState &state) {
	QString time;
	qint64 display = 0;
	auto frequency = state.frequency;
	if (!IsStoppedOrStopping(state.state)) {
		display = state.position;
	} else if (state.length) {
		display = state.length;
	} else if (state.id.audio()->song()) {
		display = (state.id.audio()->song()->duration * frequency);
	}

	_lastDurationMs = (state.length * 1000LL) / frequency;

	if (state.id.audio()->loading()) {
		_time = QString::number(std::round(state.id.audio()->progress() * 100)) + '%';
		_playbackSlider->setDisabled(true);
	} else {
		display = display / frequency;
		_time = formatDurationText(display);
		_playbackSlider->setDisabled(false);
	}
	if (_seekPositionMs < 0) {
		updateTimeLabel();
	}
}

void Widget::updateTimeLabel() {
	auto timeLabelWidth = _timeLabel->width();
	if (_seekPositionMs >= 0) {
		auto playAlready = _seekPositionMs / 1000LL;
		_timeLabel->setText(formatDurationText(playAlready));
	} else {
		_timeLabel->setText(_time);
	}
	if (timeLabelWidth != _timeLabel->width()) {
		updateLabelsGeometry();
	}
}

void Widget::handleSongChange() {
	auto current = instance()->current(_type);
	if (!current || !current.audio()) {
		return;
	}

	TextWithEntities textWithEntities;
	if (current.audio()->voice() || current.audio()->isRoundVideo()) {
		if (auto item = App::histItemById(current.contextId())) {
			auto name = App::peerName(item->fromOriginal());
			auto date = [item] {
				auto date = item->date.date();
				auto time = item->date.time().toString(cTimeFormat());
				auto today = QDateTime::currentDateTime().date();
				if (date == today) {
					return lng_player_message_today(lt_time, time);
				} else if (date.addDays(1) == today) {
					return lng_player_message_yesterday(lt_time, time);
				}
				return lng_player_message_date(lt_date, langDayOfMonthFull(date), lt_time, time);
			};

			textWithEntities.text = name + ' ' + date();
			textWithEntities.entities.append({EntityInTextBold, 0, name.size(), QString()});
		} else {
			textWithEntities.text = lang(lng_media_audio);
		}
	} else {
		auto song = current.audio()->song();
		if (!song || song->performer.isEmpty()) {
			textWithEntities.text =
			    (!song || song->title.isEmpty()) ?
			        (current.audio()->filename().isEmpty() ? qsl("Unknown Track") : current.audio()->filename()) :
			        song->title;
		} else {
			auto title = song->title.isEmpty() ? qsl("Unknown Track") : TextUtilities::Clean(song->title);
			auto dash = QString::fromUtf8(" \xe2\x80\x93 ");
			textWithEntities.text = song->performer + dash + title;
			textWithEntities.entities.append({EntityInTextBold, 0, song->performer.size(), QString()});
		}
	}
	_nameLabel->setMarkedText(textWithEntities);

	handlePlaylistUpdate();
}

void Widget::handlePlaylistUpdate() {
	auto current = instance()->current(_type);
	auto playlist = instance()->playlist(_type);
	auto index = playlist.indexOf(current.contextId());
	if (!current || index < 0) {
		destroyPrevNextButtons();
	} else {
		createPrevNextButtons();
		auto previousEnabled = (index > 0);
		auto nextEnabled = (index + 1 < playlist.size());
		_previousTrack->setIconOverride(previousEnabled ? nullptr : &st::mediaPlayerPreviousDisabledIcon);
		_previousTrack->setRippleColorOverride(previousEnabled ? nullptr : &st::mediaPlayerBg);
		_previousTrack->setCursor(previousEnabled ? style::cur_pointer : style::cur_default);
		_nextTrack->setIconOverride(nextEnabled ? nullptr : &st::mediaPlayerNextDisabledIcon);
		_nextTrack->setRippleColorOverride(nextEnabled ? nullptr : &st::mediaPlayerBg);
		_nextTrack->setCursor(nextEnabled ? style::cur_pointer : style::cur_default);
	}
}

void Widget::createPrevNextButtons() {
	if (!_previousTrack) {
		_previousTrack.create(this, st::mediaPlayerPreviousButton);
		_previousTrack->show();
		_previousTrack->setClickedCallback([]() { instance()->previous(); });
		_nextTrack.create(this, st::mediaPlayerNextButton);
		_nextTrack->show();
		_nextTrack->setClickedCallback([]() { instance()->next(); });
		updatePlayPrevNextPositions();
	}
}

void Widget::destroyPrevNextButtons() {
	if (_previousTrack) {
		_previousTrack.destroy();
		_nextTrack.destroy();
		updatePlayPrevNextPositions();
	}
}

} // namespace Player
} // namespace Media
