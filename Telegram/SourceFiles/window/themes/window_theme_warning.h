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

#include "base/object_ptr.h"
#include "core/single_timer.h"
#include "ui/twidget.h"

#include "ui/animation.h"
#include <QPixmap>

namespace Ui {
class RoundButton;
} // namespace Ui

namespace Window {
namespace Theme {

class WarningWidget : public TWidget {
public:
	WarningWidget(QWidget *parent);

	void setHiddenCallback(Fn<void()> callback) {
		_hiddenCallback = std::move(callback);
	}

	void showAnimated();
	void hideAnimated();

protected:
	void keyPressEvent(QKeyEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void refreshLang();
	void updateControlsGeometry();
	void setSecondsLeft(int secondsLeft);
	void startAnimation(bool hiding);
	void updateText();
	void handleTimer();

	bool _hiding = false;
	Animation _animation;
	QPixmap _cache;
	QRect _inner, _outer;

	SingleTimer _timer;
	TimeMs _started = 0;
	int _secondsLeft = 0;
	QString _text;

	object_ptr<Ui::RoundButton> _keepChanges;
	object_ptr<Ui::RoundButton> _revert;

	Fn<void()> _hiddenCallback;
};

} // namespace Theme
} // namespace Window
