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
#include "codegen/common/cpp_file.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace codegen {
namespace common {
namespace {

void writeLicense(QTextStream &stream, const ProjectInfo &project) {
	stream << R"#(//
// WARNING! All changes made in this file will be lost!
// Created from ')#"
	       << project.source << "' by '" << project.name << R"#(
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
)#";
}

} // namespace

CppFile::CppFile(const QString &path, const ProjectInfo &project)
    : stream_(&content_)
    , forceReGenerate_(project.forceReGenerate) {
	stream_.setCodec("UTF-8");
	bool cpp = path.toLower().endsWith(".cpp");

	QFileInfo info(path);
	info.dir().mkpath(".");
	filepath_ = info.absoluteFilePath();

	writeLicense(stream_, project);
	if (cpp) {
		include(info.baseName() + ".h").newline();
	} else {
		stream() << "#pragma once";
		newline().newline();
	}
}

CppFile &CppFile::include(const QString &header, bool global) {
	if (global)
		stream() << QString("#include <%1>").arg(header);
	else
		stream() << QString("#include \"%1\"").arg(header);
	return newline();
}

CppFile &CppFile::pushNamespace(const QString &name) {
	namespaces_.push_back(name);

	stream() << "namespace";
	if (!name.isEmpty()) {
		stream() << ' ' << name;
	}
	stream() << " {";
	return newline();
}

CppFile &CppFile::popNamespace() {
	if (namespaces_.isEmpty()) {
		return *this;
	}
	auto name = namespaces_.back();
	namespaces_.pop_back();

	stream() << "} // namespace";
	if (!name.isEmpty()) {
		stream() << ' ' << name;
	}
	return newline();
}

bool CppFile::finalize() {
	while (!namespaces_.isEmpty()) {
		popNamespace();
	}
	stream_.flush();

	QFile file(filepath_);
	if (!forceReGenerate_ && file.open(QIODevice::ReadOnly)) {
		if (file.readAll() == content_) {
			file.close();
			return true;
		}
		file.close();
	}

	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	if (file.write(content_) != content_.size()) {
		return false;
	}
	return true;
}

} // namespace common
} // namespace codegen
