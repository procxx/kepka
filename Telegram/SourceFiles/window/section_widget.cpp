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
#include "window/section_widget.h"

#include "application.h"
#include "facades.h"
#include "ui/twidget.h"
#include "window_slide_animation.h"

#include <QEvent>
#include <QPaintEvent>

namespace Window {

SectionWidget::SectionWidget(QWidget *parent, not_null<Window::Controller *> controller)
    : AbstractSectionWidget(parent, controller) {}

void SectionWidget::setGeometryWithTopMoved(const QRect &newGeometry, int topDelta) {
	_topDelta = topDelta;
	bool willBeResized = (size() != newGeometry.size());
	if (geometry() != newGeometry) {
		setGeometry(newGeometry);
	}
	if (!willBeResized) {
		resizeEvent(nullptr);
	}
	_topDelta = 0;
}

void SectionWidget::showAnimated(SlideDirection direction, const SectionSlideParams &params) {
	if (_showAnimation) return;

	showChildren();
	auto myContentCache = grabForShowAnimation(params);
	hideChildren();
	showAnimatedHook();

	_showAnimation = std::make_unique<SlideAnimation>();
	_showAnimation->setDirection(direction);
	_showAnimation->setRepaintCallback([this] { update(); });
	_showAnimation->setFinishedCallback([this] { showFinished(); });
	_showAnimation->setPixmaps(params.oldContentCache, myContentCache);
	_showAnimation->setTopBarShadow(params.withTopBarShadow);
	_showAnimation->start();

	show();
}

void SectionWidget::showFast() {
	show();
	showFinished();
}

void SectionWidget::paintEvent(QPaintEvent *e) {
	if (Ui::skipPaintEvent(this, e)) return;

	if (_showAnimation) {
		Painter p(this);
		_showAnimation->paintContents(p, e->rect());
	}
}

void SectionWidget::showFinished() {
	_showAnimation.reset();
	if (isHidden()) return;

	showChildren();
	showFinishedHook();

	setInnerFocus();
}

} // namespace Window
