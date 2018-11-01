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

#include "codegen/common/cpp_file.h"
#include "codegen/emoji/data.h"
#include "codegen/emoji/options.h"
#include "codegen/emoji/replaces.h"
#include <QtCore/QSet>
#include <QtCore/QString>
#include <memory>

namespace codegen {
namespace emoji {

class Generator {
public:
	Generator(const Options &options);
	Generator(const Generator &other) = delete;
	Generator &operator=(const Generator &other) = delete;

	int generate();

private:
#ifdef SUPPORT_IMAGE_GENERATION
	QImage generateImage(int variantIndex);
	bool writeImages();
#endif // SUPPORT_IMAGE_GENERATION

	bool writeSource();
	bool writeHeader();
	bool writeSuggestionsSource();
	bool writeSuggestionsHeader();

	template <typename Callback> bool enumerateWholeList(Callback callback);

	bool writeInitCode();
	bool writeSections();
	bool writeReplacements();
	bool writeGetSections();
	bool writeFindReplace();
	bool writeFind();
	bool writeFindFromDictionary(const std::map<QString, int, std::greater<QString>> &dictionary, QString dict_name,
	                             bool skipPostfixes = false);
	bool writeGetReplacements();
	void startBinary();
	bool writeStringBinary(common::CppFile *source, const QString &string);
	void writeIntBinary(common::CppFile *source, int data);
	void writeUintBinary(common::CppFile *source, quint32 data);

	const common::ProjectInfo &project_;
	int colorsCount_ = 0;
#ifdef SUPPORT_IMAGE_GENERATION
	bool writeImages_ = false;
#endif // SUPPORT_IMAGE_GENERATION
	QString outputPath_;
	QString spritePath_;
	std::unique_ptr<common::CppFile> source_;
	Data data_;

	QString suggestionsPath_;
	std::unique_ptr<common::CppFile> suggestionsSource_;
	Replaces replaces_;

	int _binaryFullLength = 0;
	int _binaryCount = 0;
};

} // namespace emoji
} // namespace codegen
