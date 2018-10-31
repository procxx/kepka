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
#include "codegen/lang/options.h"

#include "codegen/common/logging.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <ostream>

namespace codegen {
namespace lang {
namespace {

constexpr int kErrorOutputPathExpected = 902;
constexpr int kErrorInputPathExpected = 903;
constexpr int kErrorSingleInputPathExpected = 904;
constexpr int kErrorWorkingPathExpected = 905;

} // namespace

using common::logError;

Options parseOptions() {
	Options result;
	auto args = QCoreApplication::instance()->arguments();
	for (int i = 1, count = args.size(); i < count; ++i) { // skip first
		auto &arg = args.at(i);

		// Output path
		if (arg == "-o") {
			if (++i == count) {
				logError(kErrorOutputPathExpected, "Command Line") << "output path expected after -o";
				return Options();
			} else {
				result.outputPath = args.at(i);
			}
		} else if (arg.startsWith("-o")) {
			result.outputPath = arg.mid(2);

			// Working path
		} else if (arg == "-w") {
			if (++i == count) {
				logError(kErrorWorkingPathExpected, "Command Line") << "working path expected after -w";
				return Options();
			} else {
				common::logSetWorkingPath(args.at(i));
			}
		} else if (arg.startsWith("-w")) {
			common::logSetWorkingPath(arg.mid(2));

			// Input path
		} else {
			if (result.inputPath.isEmpty()) {
				result.inputPath = arg;
			} else {
				logError(kErrorSingleInputPathExpected, "Command Line") << "only one input path expected";
				return Options();
			}
		}
	}
	if (result.inputPath.isEmpty()) {
		logError(kErrorInputPathExpected, "Command Line") << "input path expected";
		return Options();
	}
	return result;
}

} // namespace lang
} // namespace codegen
