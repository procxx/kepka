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
#include "codegen/style/structure_types.h"
#include <QtCore/QSet>
#include <QtCore/QString>
#include <functional>
#include <map>
#include <memory>

namespace codegen {
namespace style {
namespace structure {
class Module;
} // namespace structure

class Generator {
public:
	Generator(const structure::Module &module, const QString &destBasePath, const common::ProjectInfo &project,
	          bool isPalette);
	Generator(const Generator &other) = delete;
	Generator &operator=(const Generator &other) = delete;

	bool writeHeader();
	bool writeSource();

private:
	QString typeToString(structure::Type type) const;
	QString typeToDefaultValue(structure::Type type) const;
	QString valueAssignmentCode(structure::Value value) const;

	bool writeHeaderStyleNamespace();
	bool writeStructsForwardDeclarations();
	bool writeStructsDefinitions();
	bool writePaletteDefinition();
	bool writeRefsDeclarations();

	bool writeIncludesInSource();
	bool writeVariableDefinitions();
	bool writeRefsDefinition();
	bool writeSetPaletteColor();
	bool writeVariableInit();
	bool writePxValuesInit();
	bool writeFontFamiliesInit();
	bool writeIconValues();
	bool writeIconsInit();

	bool collectUniqueValues();

	const structure::Module &module_;
	QString basePath_, baseName_;
	const common::ProjectInfo &project_;
	std::unique_ptr<common::CppFile> source_, header_;
	bool isPalette_ = false;

	QMap<int, bool> pxValues_;
	QMap<std::string, int> fontFamilies_;
	QMap<QString, int> iconMasks_; // icon file -> index
	std::map<QString, int, std::greater<QString>> paletteIndices_;

	std::vector<int> _scales = {4, 5, 6, 8}; // scale / 4 gives our 1.00, 1.25, 1.50, 2.00
	std::vector<const char *> _scaleNames = {"dbisOne", "dbisOneAndQuarter", "dbisOneAndHalf", "dbisTwo"};
};

} // namespace style
} // namespace codegen
