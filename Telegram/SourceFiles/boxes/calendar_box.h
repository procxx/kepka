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

#include "abstract_box.h"

namespace Ui {
class IconButton;
} // namespace Ui

class CalendarBox : public BoxContent {
public:
	CalendarBox(QWidget *, QDate month, QDate highlighted, Fn<void(QDate date)> callback);

	void setMinDate(QDate date);
	void setMaxDate(QDate date);

	~CalendarBox();

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;

private:
	void monthChanged(QDate month);

	bool isPreviousEnabled() const;
	bool isNextEnabled() const;

	class Context;
	std::unique_ptr<Context> _context;

	class Inner;
	object_ptr<Inner> _inner;

	class Title;
	object_ptr<Title> _title;
	object_ptr<Ui::IconButton> _previous;
	object_ptr<Ui::IconButton> _next;

	Fn<void(QDate date)> _callback;
};
