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
// Copyright (c) 2019- Kepka Contributors, https://github.com/procxx
//
/// @file data/data_document.cpp Implementation and internals (and Qt QFixed)
/// @todo Get rid of QFixed.

#include "data/data_document.h"
#include "private/qfixed_p.h"
#include <QTextEdit>

void MessageCursor::fillFrom(const QTextEdit *edit) {
	QTextCursor c = edit->textCursor();
	position = c.position();
	anchor = c.anchor();
	QScrollBar *s = edit->verticalScrollBar();
	scroll = (s && (s->value() != s->maximum())) ? s->value() : QFIXED_MAX;
}

const int MessageCursor::kMaxScroll = QFIXED_MAX;
