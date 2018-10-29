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

#include "base/build_config.h"
#include "base/functors.h"
#include "base/unique_function.h"
#include <QLatin1String>
#include <QString>
#include <cstdint>
#include <ctime>
#include <exception>
#include <functional>
#include <gsl/gsl>
#include <memory>
#include <string>

namespace func = base::functors;

using gsl::not_null;

template <typename Signature> using Fn = std::function<Signature>;

template <typename Signature> using FnMut = base::unique_function<Signature>;

#define qsl(s) QStringLiteral(s)
#define qstr(s) QLatin1String((s), static_cast<int>(sizeof(s) - 1))
