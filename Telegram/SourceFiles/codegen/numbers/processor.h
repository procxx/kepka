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

#include "codegen/numbers/options.h"
#include <QtCore/QString>
#include <memory>

namespace codegen {
namespace numbers {
class ParsedFile;
struct Rules;

// Walks through a file, parses it and generates number formatter.
class Processor {
public:
	explicit Processor(const Options &options);
	Processor(const Processor &other) = delete;
	Processor &operator=(const Processor &other) = delete;

	// Returns 0 on success.
	int launch();

	~Processor();

private:
	bool write(const Rules &rules) const;

	std::unique_ptr<ParsedFile> parser_;
	const Options &options_;
};

} // namespace numbers
} // namespace codegen
