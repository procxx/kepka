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

#include "storage/localimageloader.h"

namespace Storage {

class Uploader : public QObject, public RPCSender {
	Q_OBJECT

public:
	Uploader();
	void uploadMedia(const FullMsgId &msgId, const SendMediaReady &image);
	void upload(const FullMsgId &msgId, const FileLoadResultPtr &file);

	qint32 currentOffset(const FullMsgId &msgId) const; // -1 means file not found
	qint32 fullSize(const FullMsgId &msgId) const;

	void cancel(const FullMsgId &msgId);
	void pause(const FullMsgId &msgId);
	void confirm(const FullMsgId &msgId);

	void clear();

	~Uploader();

public slots:
	void unpause();
	void sendNext();
	void killSessions();

signals:
	void photoReady(const FullMsgId &msgId, bool silent, const MTPInputFile &file);
	void documentReady(const FullMsgId &msgId, bool silent, const MTPInputFile &file);
	void thumbDocumentReady(const FullMsgId &msgId, bool silent, const MTPInputFile &file, const MTPInputFile &thumb);

	void photoProgress(const FullMsgId &msgId);
	void documentProgress(const FullMsgId &msgId);

	void photoFailed(const FullMsgId &msgId);
	void documentFailed(const FullMsgId &msgId);

private:
	struct File {
		File(const SendMediaReady &media) : media(media), docSentParts(0) {
			partsCount = media.parts.size();
			if (type() == SendMediaType::File || type() == SendMediaType::Audio) {
				setDocSize(media.file.isEmpty() ? media.data.size() : media.filesize);
			} else {
				docSize = docPartSize = docPartsCount = 0;
			}
		}
		File(const FileLoadResultPtr &file) : file(file), docSentParts(0) {
			partsCount = (type() == SendMediaType::Photo) ? file->fileparts.size() : file->thumbparts.size();
			if (type() == SendMediaType::File || type() == SendMediaType::Audio) {
				setDocSize(file->filesize);
			} else {
				docSize = docPartSize = docPartsCount = 0;
			}
		}
		void setDocSize(qint32 size) {
			docSize = size;
			if (docSize >= 1024 * 1024 || !setPartSize(DocumentUploadPartSize0)) {
				if (docSize > 32 * 1024 * 1024 || !setPartSize(DocumentUploadPartSize1)) {
					if (!setPartSize(DocumentUploadPartSize2)) {
						if (!setPartSize(DocumentUploadPartSize3)) {
							if (!setPartSize(DocumentUploadPartSize4)) {
								LOG(("Upload Error: bad doc size: %1").arg(docSize));
							}
						}
					}
				}
			}
		}
		bool setPartSize(quint32 partSize) {
			docPartSize = partSize;
			docPartsCount = (docSize / docPartSize) + ((docSize % docPartSize) ? 1 : 0);
			return (docPartsCount <= DocumentMaxPartsCount);
		}

		FileLoadResultPtr file;
		SendMediaReady media;
		qint32 partsCount;
		mutable qint32 fileSentSize;

		quint64 id() const {
			return file ? file->id : media.id;
		}
		SendMediaType type() const {
			return file ? file->type : media.type;
		}
		quint64 thumbId() const {
			return file ? file->thumbId : media.thumbId;
		}
		const QString &filename() const {
			return file ? file->filename : media.filename;
		}

		HashMd5 md5Hash;

		QSharedPointer<QFile> docFile;
		qint32 docSentParts;
		qint32 docSize;
		qint32 docPartSize;
		qint32 docPartsCount;
	};
	typedef QMap<FullMsgId, File> Queue;

	void partLoaded(const MTPBool &result, mtpRequestId requestId);
	bool partFailed(const RPCError &err, mtpRequestId requestId);

	void currentFailed();

	QMap<mtpRequestId, QByteArray> requestsSent;
	QMap<mtpRequestId, qint32> docRequestsSent;
	QMap<mtpRequestId, qint32> dcMap;
	quint32 sentSize = 0;
	quint32 sentSizes[MTP::kUploadSessionsCount] = { 0 };

	FullMsgId uploading, _paused;
	Queue queue;
	Queue uploaded;
	QTimer nextTimer, killSessionsTimer;

};

} // namespace Storage
