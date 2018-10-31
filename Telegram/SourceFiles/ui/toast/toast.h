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
#include "core/utils.h"
#include "ui/animation.h"
#include <QMargins>
#include <QString>

class QWidget;

class Config;

namespace Ui {
namespace Toast {

namespace internal {
class Manager;
class Widget;
} // namespace internal

static constexpr const int DefaultDuration = 1500;
struct Config {
	QString text;
	int durationMs = DefaultDuration;
	int maxWidth = 0;
	QMargins padding;
};
void Show(QWidget *parent, const Config &config);
void Show(const Config &config);
void Show(const QString &text);

class Instance {
	struct Private {};

public:
	Instance(const Config &config, QWidget *widgetParent, const Private &);
	Instance(const Instance &other) = delete;
	Instance &operator=(const Instance &other) = delete;

	void hideAnimated();
	void hide();

private:
	void opacityAnimationCallback();

	bool _hiding = false;
	Animation _a_opacity;

	const TimeMs _hideAtMs;

	// ToastManager should reset _widget pointer if _widget is destroyed.
	friend class internal::Manager;
	friend void Show(QWidget *parent, const Config &config);
	std::unique_ptr<internal::Widget> _widget;
};

} // namespace Toast
} // namespace Ui
