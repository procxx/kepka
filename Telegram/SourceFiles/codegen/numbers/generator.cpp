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
#include "codegen/numbers/generator.h"

#include <QtCore/QDir>
#include <QtCore/QSet>
#include <functional>

namespace codegen {
namespace numbers {
namespace {} // namespace

Generator::Generator(const Rules &rules, const QString &destBasePath, const common::ProjectInfo &project)
    : rules_(rules)
    , basePath_(destBasePath)
    , project_(project) {}

bool Generator::writeHeader() {
	header_ = std::make_unique<common::CppFile>(basePath_ + ".h", project_);

	header_->include("QString", true);
	header_->stream() << "QVector<int> phoneNumberParse(const QString &number);\n";

	return header_->finalize();
}

bool Generator::writeSource() {
	source_ = std::make_unique<common::CppFile>(basePath_ + ".cpp", project_);
	source_->include("map", true);
	source_->include("QVector", true);

	source_->stream() << R"code(const std::map<QString, QVector<int>> RulesMap = {
)code";
	for (auto rule = rules_.data.cbegin(), e = rules_.data.cend(); rule != e; ++rule) {
		auto k = rule.key();
		source_->stream() << "{\"" << k << "\",{";
		for (auto &c : rule.key()) {
			source_->stream() << c.toLatin1() << ",";
		}
		source_->stream() << "}},\n";
	}
	source_->stream() << "\
};";

	source_->stream() << R"code(

QVector<int> phoneNumberParse(const QString &number) {
    return RulesMap.find(number) != RulesMap.end() ? RulesMap.at(number) : QVector<int>();
}
)code";

	return source_->finalize();
}

} // namespace numbers
} // namespace codegen
