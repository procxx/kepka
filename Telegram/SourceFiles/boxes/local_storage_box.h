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

#include "boxes/abstract_box.h"

namespace Ui {
class LinkButton;
} // namespace Ui

class LocalStorageBox : public BoxContent {
	Q_OBJECT

public:
	LocalStorageBox(QWidget *);

private slots:
	void onTempDirCleared(int task);
	void onTempDirClearFailed(int task);

protected:
	void prepare() override;

	void paintEvent(QPaintEvent *e) override;

private:
	void clearStorage();
	void updateControls();
	void checkLocalStoredCounts();

	enum class State {
		Normal,
		Clearing,
		Cleared,
		ClearFailed,
	};
	State _state = State::Normal;

	object_ptr<Ui::LinkButton> _clear;

	int _imagesCount = -1;
	int _audiosCount = -1;
};
