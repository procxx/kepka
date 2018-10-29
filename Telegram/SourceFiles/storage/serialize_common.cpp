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
#include "storage/serialize_common.h"

namespace Serialize {

void writeStorageImageLocation(QDataStream &stream, const StorageImageLocation &loc) {
	stream << qint32(loc.width()) << qint32(loc.height());
	stream << qint32(loc.dc()) << quint64(loc.volume()) << qint32(loc.local()) << quint64(loc.secret());
}

StorageImageLocation readStorageImageLocation(QDataStream &stream) {
	qint32 width, height, dc, local;
	quint64 volume, secret;
	stream >> width >> height >> dc >> volume >> local >> secret;
	return StorageImageLocation(width, height, dc, volume, local, secret);
}

int storageImageLocationSize() {
	// width + height + dc + volume + local + secret
	return sizeof(qint32) + sizeof(qint32) + sizeof(qint32) + sizeof(quint64) + sizeof(qint32) + sizeof(quint64);
}

} // namespace Serialize
