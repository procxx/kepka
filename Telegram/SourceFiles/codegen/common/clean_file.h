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
#include <QtCore/QVector>

#include "codegen/common/logging.h"

namespace codegen {
namespace common {

// Reads a file removing all C-style comments.
class CleanFile {
public:
	explicit CleanFile(const QString &filepath);
	explicit CleanFile(const QByteArray &content, const QString &filepath = QString());
	CleanFile(const CleanFile &other) = delete;
	CleanFile &operator=(const CleanFile &other) = delete;

	bool read();
	QVector<QByteArray> singleLineComments() const;

	const char *data() const {
		return result_.constData();
	}
	const char *end() const {
		return result_.constEnd();
	}

	static constexpr int MaxSize = 10 * 1024 * 1024;

	// Log error to std::cerr with 'code' at line number 'line' in data().
	LogStream logError(int code, int line) const;

private:
	QString filepath_;
	QByteArray content_, result_;
	bool read_;

	QVector<QByteArray> singleLineComments_;
};

} // namespace common
} // namespace codegen
