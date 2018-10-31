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

#include "codegen/common/basic_tokenized_file.h"
#include "codegen/lang/options.h"
#include <QImage>
#include <array>
#include <functional>
#include <memory>
#include <string>

namespace codegen {
namespace lang {

constexpr auto kPluralPartCount = 6;
extern const std::array<QString, kPluralPartCount> kPluralParts;
extern const QString kPluralTag;
QString ComputePluralKey(const QString &base, int index);

struct LangPack {
	struct Tag {
		QString tag;
	};
	struct Entry {
		QString key;
		QString value;
		QString keyBase; // Empty for not plural entries.
		std::vector<Tag> tags;
	};
	std::vector<Entry> entries;
	std::vector<Tag> tags;
};

inline bool operator==(const LangPack::Tag &a, const LangPack::Tag &b) {
	return a.tag == b.tag;
}

inline bool operator!=(const LangPack::Tag &a, const LangPack::Tag &b) {
	return !(a == b);
}

// Parses an input file to the internal struct.
class ParsedFile {
public:
	explicit ParsedFile(const Options &options);
	ParsedFile(const ParsedFile &other) = delete;
	ParsedFile &operator=(const ParsedFile &other) = delete;

	bool read();

	LangPack getResult() {
		return result_;
	}

private:
	bool failed() const {
		return failed_ || file_.failed();
	}

	// Log error to std::cerr with 'code' at the current position in file.
	common::LogStream logError(int code) {
		failed_ = true;
		return file_.logError(code);
	}
	common::LogStream logErrorUnexpectedToken() {
		failed_ = true;
		return file_.logErrorUnexpectedToken();
	}
	common::LogStream logErrorBadString();
	common::LogStream logAssert(bool assertion) {
		if (!assertion) {
			return logError(common::kErrorInternal) << "internal - ";
		}
		return common::LogStream(common::LogStream::Null);
	}

	// Read next token and fire unexpected token error if it is not of "type".
	using BasicToken = common::BasicTokenizedFile::Token;
	BasicToken assertNextToken(BasicToken::Type type);

	void addEntity(QString key, const QString &value);
	QString extractTagsData(const QString &value, LangPack *to);
	QString extractTagData(const QString &tag, LangPack *to);

	void fillPluralTags();

	QString filePath_;
	common::BasicTokenizedFile file_;
	Options options_;
	bool failed_ = false;
	LangPack result_;
};

} // namespace lang
} // namespace codegen
