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
#include "mtproto/core_types.h"

#include "scheme.h"
#include "zlib.h"

quint32 MTPstring::innerLength() const {
	quint32 l = v.length();
	if (l < 254) {
		l += 1;
	} else {
		l += 4;
	}
	quint32 d = l & 0x03;
	if (d) l += (4 - d);
	return l;
}

void MTPstring::read(const mtpPrime *&from, const mtpPrime *end, mtpTypeId cons) {
	if (from + 1 > end) throw mtpErrorInsufficient();
	if (cons != mtpc_string) throw mtpErrorUnexpected(cons, "MTPstring");

	quint32 l;
	const uchar *buf = (const uchar *)from;
	if (buf[0] == 254) {
		l = (quint32)buf[1] + ((quint32)buf[2] << 8) + ((quint32)buf[3] << 16);
		buf += 4;
		from += ((l + 4) >> 2) + (((l + 4) & 0x03) ? 1 : 0);
	} else {
		l = (quint32)buf[0];
		++buf;
		from += ((l + 1) >> 2) + (((l + 1) & 0x03) ? 1 : 0);
	}
	if (from > end) throw mtpErrorInsufficient();

	v = QByteArray(reinterpret_cast<const char *>(buf), l);
}

void MTPstring::write(mtpBuffer &to) const {
	quint32 l = v.length(), s = l + ((l < 254) ? 1 : 4), was = to.size();
	if (s & 0x03) {
		s += 4;
	}
	s >>= 2;
	to.resize(was + s);
	char *buf = (char *)&to[was];
	if (l < 254) {
		uchar sl = (uchar)l;
		*(buf++) = *(char *)(&sl);
	} else {
		*(buf++) = (char)254;
		*(buf++) = (char)(l & 0xFF);
		*(buf++) = (char)((l >> 8) & 0xFF);
		*(buf++) = (char)((l >> 16) & 0xFF);
	}
	memcpy(buf, v.constData(), l);
}

quint32 mtpRequest::innerLength() const { // for template MTP requests and MTPBoxed instanciation
	mtpRequestData *value = data();
	if (!value || value->size() < 9) return 0;
	return value->at(7);
}

void mtpRequest::write(mtpBuffer &to) const {
	mtpRequestData *value = data();
	if (!value || value->size() < 9) return;
	quint32 was = to.size(), s = innerLength() / sizeof(mtpPrime);
	to.resize(was + s);
	memcpy(to.data() + was, value->constData() + 8, s * sizeof(mtpPrime));
}

bool mtpRequestData::isSentContainer(const mtpRequest &request) { // "request-like" wrap for msgIds vector
	if (request->size() < 9) return false;
	return (!request->msDate && !(*request)[6]); // msDate = 0, seqNo = 0
}

bool mtpRequestData::isStateRequest(const mtpRequest &request) {
	if (request->size() < 9) return false;
	return (mtpTypeId((*request)[8]) == mtpc_msgs_state_req);
}

bool mtpRequestData::needAckByType(mtpTypeId type) {
	switch (type) {
	case mtpc_msg_container:
	case mtpc_msgs_ack:
	case mtpc_http_wait:
	case mtpc_bad_msg_notification:
	case mtpc_msgs_all_info:
	case mtpc_msgs_state_info:
	case mtpc_msg_detailed_info:
	case mtpc_msg_new_detailed_info: return false;
	}
	return true;
}

mtpRequest mtpRequestData::prepare(quint32 requestSize, quint32 maxSize) {
	if (!maxSize) maxSize = requestSize;
	mtpRequest result(new mtpRequestData(true));
	result->reserve(8 + maxSize + _padding(maxSize)); // 2: salt, 2: session_id, 2: msg_id, 1: seq_no, 1: message_length
	result->resize(7);
	result->push_back(requestSize << 2);
	return result;
}

void mtpRequestData::padding(mtpRequest &request) {
	if (request->size() < 9) return;

	quint32 requestSize = (request.innerLength() >> 2), padding = _padding(requestSize),
	        fullSize = 8 + requestSize + padding; // 2: salt, 2: session_id, 2: msg_id, 1: seq_no, 1: message_length
	if (quint32(request->size()) != fullSize) {
		request->resize(fullSize);
		if (padding) {
			memset_rand(request->data() + (fullSize - padding), padding * sizeof(mtpPrime));
		}
	}
}

quint32 mtpRequestData::_padding(quint32 requestSize) {
#ifdef TDESKTOP_MTPROTO_OLD
	return ((8 + requestSize) & 0x03) ? (4 - ((8 + requestSize) & 0x03)) : 0;
#else // TDESKTOP_MTPROTO_OLD
	auto result = ((8 + requestSize) & 0x03) ? (4 - ((8 + requestSize) & 0x03)) : 0;

	// At least 12 bytes of random padding.
	if (result < 3) {
		result += 4;
	}

	return result;
#endif // TDESKTOP_MTPROTO_OLD
}

void mtpTextSerializeCore(MTPStringLogger &to, const mtpPrime *&from, const mtpPrime *end, mtpTypeId cons,
                          quint32 level, mtpPrime vcons) {
	switch (mtpTypeId(cons)) {
	case mtpc_int: {
		MTPint value;
		value.read(from, end, cons);
		to.add(QString::number(value.v)).add(" [INT]");
	} break;

	case mtpc_long: {
		MTPlong value;
		value.read(from, end, cons);
		to.add(QString::number(value.v)).add(" [LONG]");
	} break;

	case mtpc_int128: {
		MTPint128 value;
		value.read(from, end, cons);
		to.add(QString::number(value.h)).add(" * 2^64 + ").add(QString::number(value.l)).add(" [INT128]");
	} break;

	case mtpc_int256: {
		MTPint256 value;
		value.read(from, end, cons);
		to.add(QString::number(value.h.h))
		    .add(" * 2^192 + ")
		    .add(QString::number(value.h.l))
		    .add(" * 2^128 + ")
		    .add(QString::number(value.l.h))
		    .add(" * 2 ^ 64 + ")
		    .add(QString::number(value.l.l))
		    .add(" [INT256]");
	} break;

	case mtpc_double: {
		MTPdouble value;
		value.read(from, end, cons);
		to.add(QString::number(value.v)).add(" [DOUBLE]");
	} break;

	case mtpc_string: {
		MTPstring value;
		value.read(from, end, cons);
		auto strUtf8 = value.v;
		auto str = QString::fromUtf8(strUtf8);
		if (str.toUtf8() == strUtf8) {
			to.add("\"").add(str.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n")).add("\" [STRING]");
		} else if (strUtf8.size() < 64) {
			to.add(Logs::mb(strUtf8.constData(), strUtf8.size()).str())
			    .add(" [")
			    .add(QString::number(strUtf8.size()))
			    .add(" BYTES]");
		} else {
			to.add(Logs::mb(strUtf8.constData(), 16).str())
			    .add("... [")
			    .add(QString::number(strUtf8.size()))
			    .add(" BYTES]");
		}
	} break;

	case mtpc_vector: {
		if (from >= end) {
			throw Exception("from >= end in vector");
		}
		qint32 cnt = *(from++);
		to.add("[ vector<0x").add(QString::number(vcons, 16)).add(">");
		if (cnt) {
			to.add("\n").addSpaces(level);
			for (qint32 i = 0; i < cnt; ++i) {
				to.add("  ");
				mtpTextSerializeType(to, from, end, vcons, level + 1);
				to.add(",\n").addSpaces(level);
			}
		} else {
			to.add(" ");
		}
		to.add("]");
	} break;

	case mtpc_gzip_packed: {
		MTPstring packed;
		packed.read(from, end); // read packed string as serialized mtp string type
		quint32 packedLen = packed.v.size(), unpackedChunk = packedLen;
		mtpBuffer result; // * 4 because of mtpPrime type
		result.resize(0);

		z_stream stream;
		stream.zalloc = 0;
		stream.zfree = 0;
		stream.opaque = 0;
		stream.avail_in = 0;
		stream.next_in = 0;
		int res = inflateInit2(&stream, 16 + MAX_WBITS);
		if (res != Z_OK) {
			throw Exception(QString("ungzip init, code: %1").arg(res));
		}
		stream.avail_in = packedLen;
		stream.next_in = reinterpret_cast<Bytef *>(packed.v.data());
		stream.avail_out = 0;
		while (!stream.avail_out) {
			result.resize(result.size() + unpackedChunk);
			stream.avail_out = unpackedChunk * sizeof(mtpPrime);
			stream.next_out = (Bytef *)&result[result.size() - unpackedChunk];
			int res = inflate(&stream, Z_NO_FLUSH);
			if (res != Z_OK && res != Z_STREAM_END) {
				inflateEnd(&stream);
				throw Exception(QString("ungzip unpack, code: %1").arg(res));
			}
		}
		if (stream.avail_out & 0x03) {
			quint32 badSize = result.size() * sizeof(mtpPrime) - stream.avail_out;
			throw Exception(QString("ungzip bad length, size: %1").arg(badSize));
		}
		result.resize(result.size() - (stream.avail_out >> 2));
		inflateEnd(&stream);

		if (!result.size()) {
			throw Exception("ungzip void data");
		}
		const mtpPrime *newFrom = result.constData(), *newEnd = result.constData() + result.size();
		to.add("[GZIPPED] ");
		mtpTextSerializeType(to, newFrom, newEnd, 0, level);
	} break;

	default: {
		for (quint32 i = 1; i < mtpLayerMaxSingle; ++i) {
			if (cons == mtpLayers[i]) {
				to.add("[LAYER").add(QString::number(i + 1)).add("] ");
				mtpTextSerializeType(to, from, end, 0, level);
				return;
			}
		}
		if (cons == mtpc_invokeWithLayer) {
			if (from >= end) {
				throw Exception("from >= end in invokeWithLayer");
			}
			qint32 layer = *(from++);
			to.add("[LAYER").add(QString::number(layer)).add("] ");
			mtpTextSerializeType(to, from, end, 0, level);
			return;
		}
		throw Exception(QString("unknown cons 0x%1").arg(cons, 0, 16));
	} break;
	}
}
