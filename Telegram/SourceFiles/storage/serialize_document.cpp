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
#include "storage/serialize_document.h"

#include "storage/serialize_common.h"

namespace {

enum StickerSetType {
	StickerSetTypeEmpty = 0,
	StickerSetTypeID = 1,
	StickerSetTypeShortName = 2,
};

} // namespace

namespace Serialize {

void Document::writeToStream(QDataStream &stream, DocumentData *document) {
	stream << uint64_t(document->id) << uint64_t(document->_access) << int32_t(document->date);
	stream << int32_t(document->_version);
	stream << document->name << document->mime << int32_t(document->_dc) << int32_t(document->size);
	stream << int32_t(document->dimensions.width()) << int32_t(document->dimensions.height());
	stream << int32_t(document->type);
	if (auto sticker = document->sticker()) {
		stream << document->sticker()->alt;
		switch (document->sticker()->set.type()) {
		case mtpc_inputStickerSetID: {
			stream << int32_t(StickerSetTypeID);
		} break;
		case mtpc_inputStickerSetShortName: {
			stream << int32_t(StickerSetTypeShortName);
		} break;
		case mtpc_inputStickerSetEmpty:
		default: {
			stream << int32_t(StickerSetTypeEmpty);
		} break;
		}
		writeStorageImageLocation(stream, document->sticker()->loc);
	} else {
		stream << int32_t(document->duration());
		writeStorageImageLocation(stream, document->thumb->location());
	}
}

DocumentData *Document::readFromStreamHelper(int streamAppVersion, QDataStream &stream, const StickerSetInfo *info) {
	uint64_t id, access;
	QString name, mime;
	int32_t date, dc, size, width, height, type, version;
	stream >> id >> access >> date;
	if (streamAppVersion >= 9061) {
		stream >> version;
	} else {
		version = 0;
	}
	stream >> name >> mime >> dc >> size;
	stream >> width >> height;
	stream >> type;

	QVector<MTPDocumentAttribute> attributes;
	if (!name.isEmpty()) {
		attributes.push_back(MTP_documentAttributeFilename(MTP_string(name)));
	}

	int32_t duration = -1;
	StorageImageLocation thumb;
	if (type == StickerDocument) {
		QString alt;
		int32_t typeOfSet;
		stream >> alt >> typeOfSet;

		thumb = readStorageImageLocation(stream);

		if (typeOfSet == StickerSetTypeEmpty) {
			attributes.push_back(MTP_documentAttributeSticker(MTP_flags(0), MTP_string(alt), MTP_inputStickerSetEmpty(), MTPMaskCoords()));
		} else if (info) {
			if (info->setId == Stickers::DefaultSetId || info->setId == Stickers::CloudRecentSetId || info->setId == Stickers::FavedSetId || info->setId == Stickers::CustomSetId) {
				typeOfSet = StickerSetTypeEmpty;
			}

			switch (typeOfSet) {
			case StickerSetTypeID: {
				attributes.push_back(MTP_documentAttributeSticker(MTP_flags(0), MTP_string(alt), MTP_inputStickerSetID(MTP_long(info->setId), MTP_long(info->accessHash)), MTPMaskCoords()));
			} break;
			case StickerSetTypeShortName: {
				attributes.push_back(MTP_documentAttributeSticker(MTP_flags(0), MTP_string(alt), MTP_inputStickerSetShortName(MTP_string(info->shortName)), MTPMaskCoords()));
			} break;
			case StickerSetTypeEmpty:
			default: {
				attributes.push_back(MTP_documentAttributeSticker(MTP_flags(0), MTP_string(alt), MTP_inputStickerSetEmpty(), MTPMaskCoords()));
			} break;
			}
		}
	} else {
		stream >> duration;
		if (type == AnimatedDocument) {
			attributes.push_back(MTP_documentAttributeAnimated());
		}
		thumb = readStorageImageLocation(stream);
	}
	if (width > 0 && height > 0) {
		if (duration >= 0) {
			auto flags = MTPDdocumentAttributeVideo::Flags(0);
			if (type == RoundVideoDocument) {
				flags |= MTPDdocumentAttributeVideo::Flag::f_round_message;
			}
			attributes.push_back(MTP_documentAttributeVideo(MTP_flags(flags), MTP_int(duration), MTP_int(width), MTP_int(height)));
		} else {
			attributes.push_back(MTP_documentAttributeImageSize(MTP_int(width), MTP_int(height)));
		}
	}

	if (!dc && !access) {
		return nullptr;
	}
	return App::documentSet(id, nullptr, access, version, date, attributes, mime, thumb.isNull() ? ImagePtr() : ImagePtr(thumb), dc, size, thumb);
}

DocumentData *Document::readStickerFromStream(int streamAppVersion, QDataStream &stream, const StickerSetInfo &info) {
	return readFromStreamHelper(streamAppVersion, stream, &info);
}

DocumentData *Document::readFromStream(int streamAppVersion, QDataStream &stream) {
	return readFromStreamHelper(streamAppVersion, stream, nullptr);
}

int Document::sizeInStream(DocumentData *document) {
	int result = 0;

	// id + access + date + version
	result += sizeof(uint64_t) + sizeof(uint64_t) + sizeof(int32_t) + sizeof(int32_t);
	// + namelen + name + mimelen + mime + dc + size
	result += stringSize(document->name) + stringSize(document->mime) + sizeof(int32_t) + sizeof(int32_t);
	// + width + height
	result += sizeof(int32_t) + sizeof(int32_t);
	// + type
	result += sizeof(int32_t);

	if (auto sticker = document->sticker()) { // type == StickerDocument
		// + altlen + alt + type-of-set
		result += stringSize(sticker->alt) + sizeof(int32_t);
		// + thumb loc
		result += Serialize::storageImageLocationSize();
	} else {
		// + duration
		result += sizeof(int32_t);
		// + thumb loc
		result += Serialize::storageImageLocationSize();
	}

	return result;
}

} // namespace Serialize
