/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

#include "ui/images.h"
#include "mtproto/auth_key.h"

namespace Serialize {

inline int stringSize(const QString &str) {
	return sizeof(uint32_t) + str.size() * sizeof(ushort);
}

inline int bytearraySize(const QByteArray &arr) {
	return sizeof(uint32_t) + arr.size();
}

inline int bytesSize(base::const_byte_span bytes) {
	return sizeof(uint32_t) + bytes.size();
}

struct ReadBytesVectorWrap {
	base::byte_vector &bytes;
};

inline ReadBytesVectorWrap bytes(base::byte_vector &bytes) {
	return ReadBytesVectorWrap { bytes };
}

// Compatible with QDataStream &operator>>(QDataStream &, QByteArray &);
inline QDataStream &operator>>(QDataStream &stream, ReadBytesVectorWrap data) {
	auto &bytes = data.bytes;
	bytes.clear();
	uint32_t len;
	stream >> len;
	if (stream.status() != QDataStream::Ok || len == 0xFFFFFFFF) {
		return stream;
	}

	constexpr auto kStep = uint32_t(1024 * 1024);
	for (auto allocated = uint32_t(0); allocated < len;) {
		auto blockSize = qMin(kStep, len - allocated);
		bytes.resize(allocated + blockSize);
		if (stream.readRawData(reinterpret_cast<char*>(bytes.data()) + allocated, blockSize) != blockSize) {
			bytes.clear();
			stream.setStatus(QDataStream::ReadPastEnd);
			return stream;
		}
		allocated += blockSize;
	}

	return stream;
}

struct WriteBytesWrap {
	base::const_byte_span bytes;
};

inline WriteBytesWrap bytes(base::const_byte_span bytes) {
	return WriteBytesWrap { bytes };
}

inline QDataStream &operator<<(QDataStream &stream, WriteBytesWrap data) {
	auto bytes = data.bytes;
	if (bytes.empty()) {
		stream << uint32_t(0xFFFFFFFF);
	} else {
		auto size = uint32_t(bytes.size());
		stream << size;
		stream.writeRawData(reinterpret_cast<const char*>(bytes.data()), size);
	}
	return stream;
}

inline QDataStream &operator<<(QDataStream &stream, ReadBytesVectorWrap data) {
	return stream << WriteBytesWrap { data.bytes };
}

inline int dateTimeSize() {
	return (sizeof(int64_t) + sizeof(uint32_t) + sizeof(qint8));
}

void writeStorageImageLocation(QDataStream &stream, const StorageImageLocation &loc);
StorageImageLocation readStorageImageLocation(QDataStream &stream);
int storageImageLocationSize();

template <typename T>
inline T read(QDataStream &stream) {
	auto result = T();
	stream >> result;
	return result;
}

template <>
inline MTP::AuthKey::Data read<MTP::AuthKey::Data>(QDataStream &stream) {
	auto result = MTP::AuthKey::Data();
	stream.readRawData(reinterpret_cast<char*>(result.data()), result.size());
	return result;
}

} // namespace Serialize
