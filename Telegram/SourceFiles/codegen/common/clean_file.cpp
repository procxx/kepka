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
#include "codegen/common/clean_file.h"

#include <QtCore/QDir>
#include <iostream>

#include "codegen/common/logging.h"

namespace codegen {
namespace common {
namespace {

bool readFile(const QString &filepath, QByteArray *outResult) {
	QFile f(filepath);
	if (!f.exists()) {
		logError(kErrorFileNotFound, filepath) << ": error: file does not exist.";
		return false;
	}
	auto limit = CleanFile::MaxSize;
	if (f.size() > limit) {
		logError(kErrorFileTooLarge, filepath) << "' is too large, size=" << f.size() << " > maxsize=" << limit;
		return false;
	}
	if (!f.open(QIODevice::ReadOnly)) {
		logError(kErrorFileNotOpened, filepath) << "' for read.";
		return false;
	}
	*outResult = f.readAll();
	return true;
}

} // namespace


CleanFile::CleanFile(const QString &filepath)
    : filepath_(filepath)
    , read_(true) {}

CleanFile::CleanFile(const QByteArray &content, const QString &filepath)
    : filepath_(filepath)
    , content_(content)
    , read_(false) {}

bool CleanFile::read() {
	if (read_) {
		if (!readFile(filepath_, &content_)) {
			return false;
		}
	}
	filepath_ = QFileInfo(filepath_).absoluteFilePath();

	enum class InsideComment {
		None,
		SingleLine,
		MultiLine,
	};
	auto insideComment = InsideComment::None;
	bool insideString = false;

	const char *begin = content_.cbegin(), *end = content_.cend(), *offset = begin;
	auto feedContent = [this, &offset, end](const char *ch) {
		if (ch > offset) {
			if (result_.isEmpty()) result_.reserve(end - offset - 2);
			result_.append(offset, ch - offset);
			offset = ch;
		}
	};

	auto lineNumber = 0;
	auto feedComment = [this, &offset, end, &lineNumber](const char *ch, bool save = false) {
		if (ch > offset) {
			if (save) {
				singleLineComments_.resize(lineNumber + 1);
				singleLineComments_[lineNumber] = QByteArray(offset, ch - offset);
			}
			if (result_.isEmpty()) {
				result_.reserve(end - offset - 2);
			}
			result_.append(' ');
			offset = ch;
		}
	};
	for (const char *ch = offset; ch != end;) {
		char currentChar = *ch;
		char nextChar = (ch + 1 == end) ? 0 : *(ch + 1);

		if (insideComment == InsideComment::None && currentChar == '"') {
			bool escaped = ((ch > begin) && *(ch - 1) == '\\') && ((ch - 1 < begin) || *(ch - 2) != '\\');
			if (!escaped) {
				insideString = !insideString;
			}
		}
		if (insideString) {
			if (currentChar == '\n') {
				++lineNumber;
			}
			++ch;
			continue;
		}

		if (insideComment == InsideComment::None && currentChar == '/' && nextChar == '/') {
			feedContent(ch);
			insideComment = InsideComment::SingleLine;
			ch += 2;
		} else if (insideComment == InsideComment::SingleLine && currentChar == '\r' && nextChar == '\n') {
			feedComment(ch, true);
			ch += 2;
			++lineNumber;
			insideComment = InsideComment::None;
		} else if (insideComment == InsideComment::SingleLine && currentChar == '\n') {
			feedComment(ch, true);
			++ch;
			++lineNumber;
			insideComment = InsideComment::None;
		} else if (insideComment == InsideComment::None && currentChar == '/' && nextChar == '*') {
			feedContent(ch);
			ch += 2;
			insideComment = InsideComment::MultiLine;
		} else if (insideComment == InsideComment::MultiLine && currentChar == '*' && nextChar == '/') {
			ch += 2;
			feedComment(ch);
			insideComment = InsideComment::None;
		} else if (insideComment == InsideComment::MultiLine && currentChar == '\r' && nextChar == '\n') {
			feedComment(ch);
			ch += 2;
			++lineNumber;
			feedContent(ch);
		} else if (insideComment == InsideComment::MultiLine && currentChar == '\n') {
			feedComment(ch);
			++ch;
			++lineNumber;
			feedContent(ch);
		} else {
			if (currentChar == '\n') {
				++lineNumber;
			}
			++ch;
		}
	}
	singleLineComments_.resize(lineNumber + 1);

	if (insideComment == InsideComment::MultiLine) {
		common::logError(kErrorUnexpectedEndOfFile, filepath_);
		return false;
	}
	if (insideComment == InsideComment::None && end > offset) {
		if (result_.isEmpty()) {
			result_ = content_;
		} else {
			result_.append(offset, end - offset);
		}
	}
	return true;
}

QVector<QByteArray> CleanFile::singleLineComments() const {
	return singleLineComments_;
}

LogStream CleanFile::logError(int code, int line) const {
	return common::logError(code, filepath_, line);
}

} // namespace common
} // namespace codegen
