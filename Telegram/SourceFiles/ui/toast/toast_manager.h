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

#include "core/single_timer.h"
#include "ui/toast/toast.h"

namespace Ui {
namespace Toast {
namespace internal {

class Widget;
class Manager : public QObject {
	Q_OBJECT

public:
	Manager(const Manager &other) = delete;
	Manager &operator=(const Manager &other) = delete;

	static Manager *instance(QWidget *parent);

	void addToast(std::unique_ptr<Instance> &&toast);

	~Manager();

protected:
	bool eventFilter(QObject *o, QEvent *e);

private slots:
	void onHideTimeout();
	void onToastWidgetDestroyed(QObject *widget);

private:
	Manager(QWidget *parent);
	void startNextHideTimer();

	SingleTimer _hideTimer;
	TimeMs _nextHide = 0;

	QMultiMap<TimeMs, Instance *> _toastByHideTime;
	QMap<Widget *, Instance *> _toastByWidget;
	QList<Instance *> _toasts;
	std::set<QPointer<QWidget>> _toastParents;
};

} // namespace internal
} // namespace Toast
} // namespace Ui
