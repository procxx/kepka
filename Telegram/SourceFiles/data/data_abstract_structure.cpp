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
#include "data/data_abstract_structure.h"
#include "core/utils.h"

namespace Data {
namespace {

using DataStructures = std::set<AbstractStructure **>;
NeverFreedPointer<DataStructures> structures;

} // namespace

namespace internal {

void registerAbstractStructure(AbstractStructure **p) {
	structures.createIfNull();
	structures->insert(p);
}

} // namespace internal

void clearGlobalStructures() {
	if (!structures) return;
	for (auto &p : *structures) {
		delete (*p);
		*p = nullptr;
	}
	structures.clear();
}

} // namespace Data
