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
#include "codegen/common/checked_utf8_string.h"

#include <QtCore/QTextCodec>
#include <iostream>

#include "codegen/common/const_utf8_string.h"

namespace codegen {
namespace common {

CheckedUtf8String::CheckedUtf8String(const char *string, int size) {
	if (size < 0) {
		size = strlen(string);
	}
	if (!size) { // Valid empty string
		return;
	}

	QTextCodec::ConverterState state;
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
	string_ = codec->toUnicode(string, size, &state);
	if (state.invalidChars > 0) {
		valid_ = false;
	}
}

CheckedUtf8String::CheckedUtf8String(const QByteArray &string)
    : CheckedUtf8String(string.constData(), string.size()) {}

CheckedUtf8String::CheckedUtf8String(const ConstUtf8String &string)
    : CheckedUtf8String(string.data(), string.size()) {}

} // namespace common
} // namespace codegen
