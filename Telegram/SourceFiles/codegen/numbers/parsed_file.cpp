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
#include "codegen/numbers/parsed_file.h"

#include "codegen/common/basic_tokenized_file.h"
#include "codegen/common/checked_utf8_string.h"
#include "codegen/common/clean_file_reader.h"
#include "codegen/common/logging.h"
#include <QtCore/QDir>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <iostream>

using BasicToken = codegen::common::BasicTokenizedFile::Token;
using BasicType = BasicToken::Type;

namespace codegen {
namespace numbers {
namespace {

QByteArray replaceStrings(const QString &filepath) {
	common::CleanFileReader reader(filepath);
	if (!reader.read()) {
		return QByteArray();
	}
	common::CheckedUtf8String string(reader.currentPtr(), reader.charsLeft());
	if (!string.isValid()) {
		return QByteArray();
	}

	QStringList lines = string.toString().split('\n');
	for (auto &line : lines) {
		auto match = QRegularExpression("^(\\d+;[A-Z]+;)([^;]+)(;.*)?$").match(line);
		if (match.hasMatch()) {
			line = match.captured(1) + '"' + match.captured(2) + '"' + match.captured(3);
		}
	}
	return lines.join('\n').toUtf8();
}

} // namespace

ParsedFile::ParsedFile(const Options &options)
    : content_(replaceStrings(options.inputPath))
    , file_(content_, options.inputPath)
    , options_(options) {}

bool ParsedFile::read() {
	if (content_.isEmpty() || !file_.read()) {
		return false;
	}

	auto filepath = QFileInfo(options_.inputPath).absoluteFilePath();
	do {
		if (auto code = file_.getToken(BasicType::Int)) {
			if (!file_.getToken(BasicType::Semicolon)) {
				logErrorUnexpectedToken() << "';'";
				return false;
			}
			if (!file_.getToken(BasicType::Name)) {
				logErrorUnexpectedToken() << "country code";
				return false;
			}
			if (!file_.getToken(BasicType::Semicolon)) {
				logErrorUnexpectedToken() << "';'";
				return false;
			}
			if (!file_.getToken(BasicType::String)) {
				logErrorUnexpectedToken() << "country name";
				return false;
			}
			if (file_.getToken(BasicType::Semicolon)) {
				if (auto firstPart = file_.getToken(BasicType::Int)) {
					if (firstPart.original.toByteArray() != code.original.toByteArray()) {
						file_.putBack();
						result_.data.insert(code.original.toStringUnchecked(), Rule());
						continue;
					}

					Rule rule;
					while (auto part = file_.getToken(BasicType::Name)) {
						rule.push_back(part.original.size());
					}
					result_.data.insert(code.original.toStringUnchecked(), rule);
					if (rule.isEmpty()) {
						logErrorUnexpectedToken() << "bad phone pattern";
						return false;
					}

					if (!file_.getToken(BasicType::Semicolon)) {
						logErrorUnexpectedToken() << "';'";
						return false;
					}
					if (!file_.getToken(BasicType::Int)) {
						logErrorUnexpectedToken() << "country phone len";
						return false;
					}
					file_.getToken(BasicType::Semicolon);
					continue;
				} else {
					logErrorUnexpectedToken() << "country phone pattern";
					return false;
				}
			} else if (file_.getToken(BasicType::Int)) {
				file_.putBack();
				result_.data.insert(code.original.toStringUnchecked(), Rule());
				continue;
			} else {
				logErrorUnexpectedToken() << "country phone pattern";
				return false;
			}
		}
		if (file_.atEnd()) {
			break;
		}
		logErrorUnexpectedToken() << "numbers rule";
	} while (!failed());

	if (failed()) {
		result_.data.clear();
	}
	return !failed();
}

} // namespace numbers
} // namespace codegen
