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
#include "codegen/lang/processor.h"

#include "codegen/common/cpp_file.h"
#include "codegen/lang/generator.h"
#include "codegen/lang/parsed_file.h"
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace codegen {
namespace lang {
namespace {

constexpr int kErrorCantWritePath = 821;

} // namespace

Processor::Processor(const Options &options)
    : parser_(std::make_unique<ParsedFile>(options))
    , options_(options) {}

int Processor::launch() {
	if (!parser_->read()) {
		return -1;
	}

	if (!write(parser_->getResult())) {
		return -1;
	}

	return 0;
}

bool Processor::write(const LangPack &langpack) const {
	bool forceReGenerate = false;
	QDir dir(options_.outputPath);
	if (!dir.mkpath(".")) {
		common::logError(kErrorCantWritePath, "Command Line")
		    << "can not open path for writing: " << dir.absolutePath().toStdString();
		return false;
	}

	QFileInfo srcFile(options_.inputPath);
	QString dstFilePath = dir.absolutePath() + "/lang_auto";

	common::ProjectInfo project = {"codegen_style", srcFile.fileName(), forceReGenerate};

	Generator generator(langpack, dstFilePath, project);
	if (!generator.writeHeader()) {
		return false;
	}
	if (!generator.writeSource()) {
		return false;
	}
	return true;
}

Processor::~Processor() = default;

} // namespace lang
} // namespace codegen
