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

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <string>

namespace codegen {
namespace common {

// This is a simple wrapper around (const char*, size).
// Not null-terminated! It does not hold any ownership.
class ConstUtf8String {
public:
	explicit ConstUtf8String(const char *string, int size = -1)
	    : string_(string) {
		if (size < 0) {
			size = strlen(string);
		}
		size_ = size;
	}
	ConstUtf8String(const char *string, const char *end)
	    : ConstUtf8String(string, end - string) {}

	QByteArray toByteArray() const {
		return QByteArray(string_, size_);
	}
	std::string toStdString() const {
		return std::string(string_, size_);
	}
	QString toStringUnchecked() const {
		return QString::fromUtf8(string_, size_);
	}
	bool empty() const {
		return size_ == 0;
	}
	const char *data() const {
		return string_;
	}
	int size() const {
		return size_;
	}
	const char *end() const {
		return data() + size();
	}
	ConstUtf8String mid(int pos, int size = -1) {
		return ConstUtf8String(string_ + pos, std::max(std::min(size, size_ - pos), 0));
	}

private:
	const char *string_;
	int size_;
};

} // namespace common
} // namespace codegen
