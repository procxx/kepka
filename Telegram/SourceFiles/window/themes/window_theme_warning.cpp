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
#include "window/themes/window_theme_warning.h"

#include "app.h"
#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/shadow.h"
#include "window/themes/window_theme.h"

namespace Window {
namespace Theme {
namespace {

constexpr int kWaitBeforeRevertMs = 15999;

} // namespace

WarningWidget::WarningWidget(QWidget *parent)
    : TWidget(parent)
    , _secondsLeft(kWaitBeforeRevertMs / 1000)
    , _keepChanges(this, langFactory(lng_theme_keep_changes), st::defaultBoxButton)
    , _revert(this, langFactory(lng_theme_revert), st::defaultBoxButton) {
	_keepChanges->setClickedCallback([] { Window::Theme::KeepApplied(); });
	_revert->setClickedCallback([] { Window::Theme::Revert(); });
	_timer.setTimeoutHandler([this] { handleTimer(); });
	updateText();
}

void WarningWidget::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Escape) {
		Window::Theme::Revert();
	}
}

void WarningWidget::paintEvent(QPaintEvent *e) {
	Painter p(this);

	if (!_cache.isNull()) {
		if (!_animation.animating(getms())) {
			if (isHidden()) {
				return;
			}
		}
		p.setOpacity(_animation.current(_hiding ? 0. : 1.));
		p.drawPixmap(_outer.topLeft(), _cache);
		if (!_animation.animating()) {
			_cache = QPixmap();
			showChildren();
			_started = getms(true);
			_timer.start(100);
		}
		return;
	}

	Ui::Shadow::paint(p, _inner, width(), st::boxRoundShadow);
	App::roundRect(p, _inner, st::boxBg, BoxCorners);

	p.setFont(st::boxTitleFont);
	p.setPen(st::boxTitleFg);
	p.drawTextLeft(_inner.x() + st::boxTitlePosition.x(), _inner.y() + st::boxTitlePosition.y(), width(),
	               lang(lng_theme_sure_keep));

	p.setFont(st::boxTextFont);
	p.setPen(st::boxTextFg);
	p.drawTextLeft(_inner.x() + st::boxTitlePosition.x(), _inner.y() + st::themeWarningTextTop, width(), _text);
}

void WarningWidget::resizeEvent(QResizeEvent *e) {
	_inner = QRect((width() - st::themeWarningWidth) / 2, (height() - st::themeWarningHeight) / 2,
	               st::themeWarningWidth, st::themeWarningHeight);
	_outer = _inner.marginsAdded(st::boxRoundShadow.extend);
	updateControlsGeometry();
	update();
}

void WarningWidget::updateControlsGeometry() {
	auto left = _inner.x() + _inner.width() - st::boxButtonPadding.right() - _keepChanges->width();
	_keepChanges->moveToLeft(left,
	                         _inner.y() + _inner.height() - st::boxButtonPadding.bottom() - _keepChanges->height());
	_revert->moveToLeft(left - st::boxButtonPadding.left() - _revert->width(), _keepChanges->y());
}

void WarningWidget::refreshLang() {
	InvokeQueued(this, [this] { updateControlsGeometry(); });
}

void WarningWidget::handleTimer() {
	auto msPassed = getms(true) - _started;
	setSecondsLeft((kWaitBeforeRevertMs - msPassed) / 1000);
}

void WarningWidget::setSecondsLeft(int secondsLeft) {
	if (secondsLeft <= 0) {
		Window::Theme::Revert();
	} else {
		if (_secondsLeft != secondsLeft) {
			_secondsLeft = secondsLeft;
			updateText();
			update();
		}
		_timer.start(100);
	}
}

void WarningWidget::updateText() {
	_text = lng_theme_reverting(lt_count, _secondsLeft);
}

void WarningWidget::showAnimated() {
	startAnimation(false);
	show();
	setFocus();
}

void WarningWidget::hideAnimated() {
	startAnimation(true);
}

void WarningWidget::startAnimation(bool hiding) {
	_timer.stop();
	_hiding = hiding;
	if (_cache.isNull()) {
		showChildren();
		myEnsureResized(this);
		_cache = myGrab(this, _outer);
	}
	hideChildren();
	_animation.start(
	    [this] {
		    update();
		    if (_hiding) {
			    hide();
			    if (_hiddenCallback) {
				    _hiddenCallback();
			    }
		    }
	    },
	    _hiding ? 1. : 0., _hiding ? 0. : 1., st::boxDuration);
}

} // namespace Theme
} // namespace Window
