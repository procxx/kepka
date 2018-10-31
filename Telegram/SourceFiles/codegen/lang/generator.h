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
#include "codegen/lang/parsed_file.h"
#include <QtCore/QSet>
#include <QtCore/QString>
#include <functional>
#include <map>
#include <memory>
#include <set>

namespace codegen {
namespace lang {

class Generator {
public:
	Generator(const LangPack &langpack, const QString &destBasePath, const common::ProjectInfo &project);
	Generator(const Generator &other) = delete;
	Generator &operator=(const Generator &other) = delete;

	bool writeHeader();
	bool writeSource();

private:
	QString getFullKey(const LangPack::Entry &entry);

	template <typename ComputeResult>
	void writeSetSearch(const std::set<QString, std::greater<QString>> &set, ComputeResult computeResult,
	                    const QString &invalidResult);

	const LangPack &langpack_;
	QString basePath_, baseName_;
	const common::ProjectInfo &project_;
	std::unique_ptr<common::CppFile> source_, header_;
};

} // namespace lang
} // namespace codegen
