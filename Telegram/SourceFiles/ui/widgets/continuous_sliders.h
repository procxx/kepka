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
#pragma once

#include "styles/style_widgets.h"

namespace Ui {

class ContinuousSlider : public TWidget {
public:
	ContinuousSlider(QWidget *parent);

	enum class Direction {
		Horizontal,
		Vertical,
	};
	void setDirection(Direction direction) {
		_direction = direction;
		update();
	}

	double value() const;
	void setValue(double value);
	void setFadeOpacity(double opacity);
	void setDisabled(bool disabled);
	bool isDisabled() const {
		return _disabled;
	}

	using Callback = base::lambda<void(double)>;
	void setChangeProgressCallback(Callback &&callback) {
		_changeProgressCallback = std::move(callback);
	}
	void setChangeFinishedCallback(Callback &&callback) {
		_changeFinishedCallback = std::move(callback);
	}
	bool isChanging() const {
		return _mouseDown;
	}

	void setMoveByWheel(bool move);

protected:
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	void enterEventHook(QEvent *e) override;
	void leaveEventHook(QEvent *e) override;

	double fadeOpacity() const {
		return _fadeOpacity;
	}
	double getCurrentValue() {
		return _mouseDown ? _downValue : _value;
	}
	double getCurrentOverFactor(TimeMs ms) {
		return _disabled ? 0. : _a_over.current(ms, _over ? 1. : 0.);
	}
	Direction getDirection() const {
		return _direction;
	}
	bool isHorizontal() const {
		return (_direction == Direction::Horizontal);
	}

private:
	virtual QRect getSeekRect() const = 0;
	virtual double getOverDuration() const = 0;

	bool moveByWheel() const {
		return _byWheelFinished != nullptr;
	}

	void setOver(bool over);
	double computeValue(const QPoint &pos) const;
	void updateDownValueFromPos(const QPoint &pos);

	Direction _direction = Direction::Horizontal;
	bool _disabled = false;

	std::unique_ptr<SingleTimer> _byWheelFinished;

	Callback _changeProgressCallback;
	Callback _changeFinishedCallback;

	bool _over = false;
	Animation _a_over;

	double _value = 0.;

	bool _mouseDown = false;
	double _downValue = 0.;

	double _fadeOpacity = 1.;

};

class FilledSlider : public ContinuousSlider {
public:
	FilledSlider(QWidget *parent, const style::FilledSlider &st);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	QRect getSeekRect() const override;
	double getOverDuration() const override;

	const style::FilledSlider &_st;

};

class MediaSlider : public ContinuousSlider {
public:
	MediaSlider(QWidget *parent, const style::MediaSlider &st);

	void setAlwaysDisplayMarker(bool alwaysDisplayMarker) {
		_alwaysDisplayMarker = alwaysDisplayMarker;
		update();
	}

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	QRect getSeekRect() const override;
	double getOverDuration() const override;

	const style::MediaSlider &_st;
	bool _alwaysDisplayMarker = false;

};

} // namespace Ui
