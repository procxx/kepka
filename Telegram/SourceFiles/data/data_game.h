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
// Copyright (c) 2019- Kepka Contributors, https://github.com/procxx
//
/// @file data/data_game.h Data_type for Telegram Games.

#pragma once

#include "data/data_types.h"

class PhotoData;
class DocumentData;

struct GameData {
	GameData(const GameId &id)
	    : id(id) {}
	GameData(const GameId &id, const quint64 &accessHash, const QString &shortName, const QString &title,
	         const QString &description, PhotoData *photo, DocumentData *document);

	void forget();

	GameId id = 0;
	quint64 accessHash = 0;
	QString shortName;
	QString title;
	QString description;
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
};
