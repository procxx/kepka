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

class TWidget;

#include "styles/style_widgets.h"

namespace Ui {

class FadeAnimation {
public:
	FadeAnimation(TWidget *widget, bool scaled = false);

	bool paint(Painter &p);
	void refreshCache();

	using FinishedCallback = base::lambda<void()>;
	void setFinishedCallback(FinishedCallback &&callback);

	using UpdatedCallback = base::lambda<void(double)>;
	void setUpdatedCallback(UpdatedCallback &&callback);

	void show();
	void hide();

	void fadeIn(int duration);
	void fadeOut(int duration);

	void finish() {
		_animation.finish();
	}

	bool animating() const {
		return _animation.animating();
	}
	bool visible() const {
		return _visible;
	}

private:
	void startAnimation(int duration);
	void stopAnimation();

	void updateCallback();
	QPixmap grabContent();

	TWidget *_widget = nullptr;
	bool _scaled = false;

	Animation _animation;
	QSize _size;
	QPixmap _cache;
	bool _visible = false;

	FinishedCallback _finishedCallback;
	UpdatedCallback _updatedCallback;

};

template <typename Widget>
class WidgetFadeWrap;

template <>
class WidgetFadeWrap<TWidget> : public TWidget {
public:
	WidgetFadeWrap(QWidget *parent
		, object_ptr<TWidget> entity
		, int duration = st::widgetFadeDuration
		, base::lambda<void()> updateCallback = base::lambda<void()>()
		, bool scaled = false);

	void showAnimated() {
		toggleAnimated(true);
	}
	void hideAnimated() {
		toggleAnimated(false);
	}
	void toggleAnimated(bool visible) {
		if (visible) {
			_animation.fadeIn(_duration);
		} else {
			_animation.fadeOut(_duration);
		}
	}
	void showFast() {
		toggleFast(true);
	}
	void hideFast() {
		toggleFast(false);
	}
	void toggleFast(bool visible) {
		if (visible) {
			_animation.show();
		} else {
			_animation.hide();
		}
		if (_updateCallback) {
			_updateCallback();
		}
	}
	void finishAnimation() {
		_animation.finish();
	}

	bool isHiddenOrHiding() const {
		return !_animation.visible();
	}

	TWidget *entity() {
		return _entity;
	}

	const TWidget *entity() const {
		return _entity;
	}

	QMargins getMargins() const override {
		return _entity->getMargins();
	}
	int naturalWidth() const override {
		return _entity->naturalWidth();
	}

	bool animating() const {
		return _animation.animating();
	}
	void setUpdateCallback(base::lambda<void()> callback) {
		_updateCallback = std::move(callback);
		installCallbacks();
	}

protected:
	bool eventFilter(QObject *object, QEvent *event) override;
	void paintEvent(QPaintEvent *e) override;

private:
	void installCallbacks();

	object_ptr<TWidget> _entity;
	int _duration;
	base::lambda<void()> _updateCallback;

	FadeAnimation _animation;

};

template <typename Widget>
class WidgetFadeWrap : public WidgetFadeWrap<TWidget> {
public:
	WidgetFadeWrap(QWidget *parent
		, object_ptr<Widget> entity
		, int duration = st::widgetFadeDuration
		, base::lambda<void()> updateCallback = base::lambda<void()>()
		, bool scaled = false) : WidgetFadeWrap<TWidget>(parent
			, std::move(entity)
			, duration
			, std::move(updateCallback)
			, scaled) {
	}
	Widget *entity() {
		return static_cast<Widget*>(WidgetFadeWrap<TWidget>::entity());
	}
	const Widget *entity() const {
		return static_cast<const Widget*>(WidgetFadeWrap<TWidget>::entity());
	}

};

template <typename Widget>
class WidgetScaledFadeWrap : public WidgetFadeWrap<Widget> {
public:
	WidgetScaledFadeWrap(QWidget *parent
		, object_ptr<Widget> entity
		, int duration = st::widgetFadeDuration
		, base::lambda<void()> updateCallback = base::lambda<void()>()) : WidgetFadeWrap<Widget>(parent
			, std::move(entity)
			, duration
			, std::move(updateCallback)
			, true) {
	}

};

} // namespace Ui
