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

#include "base/variant.h"

enum class CompressConfirm {
	Auto,
	Yes,
	No,
	None,
};

enum class SendMediaType {
	Photo,
	Audio,
	File,
};

struct SendMediaPrepare {
	SendMediaPrepare(const QString &file, const PeerId &peer, SendMediaType type, MsgId replyTo) : id(rand_value<PhotoId>()), file(file), peer(peer), type(type), replyTo(replyTo) {
	}
	SendMediaPrepare(const QImage &img, const PeerId &peer, SendMediaType type, MsgId replyTo) : id(rand_value<PhotoId>()), img(img), peer(peer), type(type), replyTo(replyTo) {
	}
	SendMediaPrepare(const QByteArray &data, const PeerId &peer, SendMediaType type, MsgId replyTo) : id(rand_value<PhotoId>()), data(data), peer(peer), type(type), replyTo(replyTo) {
	}
	SendMediaPrepare(const QByteArray &data, int duration, const PeerId &peer, SendMediaType type, MsgId replyTo) : id(rand_value<PhotoId>()), data(data), peer(peer), type(type), duration(duration), replyTo(replyTo) {
	}
	PhotoId id;
	QString file;
	QImage img;
	QByteArray data;
	PeerId peer;
	SendMediaType type;
	int duration = 0;
	MsgId replyTo;

};
using SendMediaPrepareList = QList<SendMediaPrepare>;

using UploadFileParts =  QMap<int, QByteArray>;
struct SendMediaReady {
	SendMediaReady() = default; // temp
	SendMediaReady(SendMediaType type, const QString &file, const QString &filename, qint32 filesize, const QByteArray &data, const quint64 &id, const quint64 &thumbId, const QString &thumbExt, const PeerId &peer, const MTPPhoto &photo, const PreparedPhotoThumbs &photoThumbs, const MTPDocument &document, const QByteArray &jpeg, MsgId replyTo)
		: replyTo(replyTo)
		, type(type)
		, file(file)
		, filename(filename)
		, filesize(filesize)
		, data(data)
		, thumbExt(thumbExt)
		, id(id)
		, thumbId(thumbId)
		, peer(peer)
		, photo(photo)
		, document(document)
		, photoThumbs(photoThumbs) {
		if (!jpeg.isEmpty()) {
			qint32 size = jpeg.size();
			for (qint32 i = 0, part = 0; i < size; i += UploadPartSize, ++part) {
				parts.insert(part, jpeg.mid(i, UploadPartSize));
			}
			jpeg_md5.resize(32);
			hashMd5Hex(jpeg.constData(), jpeg.size(), jpeg_md5.data());
		}
	}
	MsgId replyTo;
	SendMediaType type;
	QString file, filename;
	qint32 filesize;
	QByteArray data;
	QString thumbExt;
	quint64 id, thumbId; // id always file-id of media, thumbId is file-id of thumb ( == id for photos)
	PeerId peer;

	MTPPhoto photo;
	MTPDocument document;
	PreparedPhotoThumbs photoThumbs;
	UploadFileParts parts;
	QByteArray jpeg_md5;

	QString caption;

};

using TaskId = void*; // no interface, just id

class Task {
public:
	virtual void process() = 0; // is executed in a separate thread
	virtual void finish() = 0; // is executed in the same as TaskQueue thread
	virtual ~Task() = default;

	TaskId id() const {
		return static_cast<TaskId>(const_cast<Task*>(this));
	}

};
using TaskPtr = QSharedPointer<Task>;
using TasksList = QList<TaskPtr>;

class TaskQueueWorker;
class TaskQueue : public QObject {
	Q_OBJECT

public:
	TaskQueue(QObject *parent, qint32 stopTimeoutMs = 0); // <= 0 - never stop worker

	TaskId addTask(TaskPtr task);
	void addTasks(const TasksList &tasks);
	void cancelTask(TaskId id); // this task finish() won't be called

	~TaskQueue();

signals:
	void taskAdded();

public slots:
	void onTaskProcessed();
	void stop();

private:
	friend class TaskQueueWorker;

	void wakeThread();

	TasksList _tasksToProcess, _tasksToFinish;
	QMutex _tasksToProcessMutex, _tasksToFinishMutex;
	QThread *_thread;
	TaskQueueWorker *_worker;
	QTimer *_stopTimer;

};

class TaskQueueWorker : public QObject {
	Q_OBJECT

public:
	TaskQueueWorker(TaskQueue *queue) : _queue(queue) {
	}

signals:
	void taskProcessed();

public slots:
	void onTaskAdded();

private:
	TaskQueue *_queue;
	bool _inTaskAdded = false;

};

struct FileLoadTo {
	FileLoadTo(const PeerId &peer, bool silent, MsgId replyTo)
		: peer(peer)
		, silent(silent)
		, replyTo(replyTo) {
	}
	PeerId peer;
	bool silent;
	MsgId replyTo;
};

struct FileLoadResult {
	FileLoadResult(const quint64 &id, const FileLoadTo &to, const QString &caption)
		: id(id)
		, to(to)
		, caption(caption) {
	}

	quint64 id;
	FileLoadTo to;
	SendMediaType type = SendMediaType::File;
	QString filepath;
	QByteArray content;

	QString filename;
	QString filemime;
	qint32 filesize = 0;
	UploadFileParts fileparts;
	QByteArray filemd5;
	qint32 partssize;

	quint64 thumbId = 0; // id is always file-id of media, thumbId is file-id of thumb ( == id for photos)
	QString thumbname;
	UploadFileParts thumbparts;
	QByteArray thumbmd5;
	QPixmap thumb;

	MTPPhoto photo;
	MTPDocument document;

	PreparedPhotoThumbs photoThumbs;
	QString caption;

	void setFileData(const QByteArray &filedata) {
		if (filedata.isEmpty()) {
			partssize = 0;
		} else {
			partssize = filedata.size();
			for (qint32 i = 0, part = 0; i < partssize; i += UploadPartSize, ++part) {
				fileparts.insert(part, filedata.mid(i, UploadPartSize));
			}
			filemd5.resize(32);
			hashMd5Hex(filedata.constData(), filedata.size(), filemd5.data());
		}
	}
	void setThumbData(const QByteArray &thumbdata) {
		if (!thumbdata.isEmpty()) {
			qint32 size = thumbdata.size();
			for (qint32 i = 0, part = 0; i < size; i += UploadPartSize, ++part) {
				thumbparts.insert(part, thumbdata.mid(i, UploadPartSize));
			}
			thumbmd5.resize(32);
			hashMd5Hex(thumbdata.constData(), thumbdata.size(), thumbmd5.data());
		}
	}
};
typedef QSharedPointer<FileLoadResult> FileLoadResultPtr;

class FileLoadTask final : public Task {
public:
	struct Image {
		QImage data;
		bool animated = false;
	};
	struct Song {
		int duration = -1;
		QString title;
		QString performer;
		QImage cover;
	};
	struct Video {
		bool isGifv = false;
		int duration = -1;
		QImage thumbnail;
	};
	struct MediaInformation {
		QString filemime;
		base::variant<Image, Song, Video> media;
	};
	static std::unique_ptr<MediaInformation> ReadMediaInformation(const QString &filepath, const QByteArray &content, const QString &filemime);

	FileLoadTask(const QString &filepath, std::unique_ptr<MediaInformation> information, SendMediaType type, const FileLoadTo &to, const QString &caption);
	FileLoadTask(const QByteArray &content, const QImage &image, SendMediaType type, const FileLoadTo &to, const QString &caption);
	FileLoadTask(const QByteArray &voice, qint32 duration, const VoiceWaveform &waveform, const FileLoadTo &to, const QString &caption);

	quint64 fileid() const {
		return _id;
	}

	void process();
	void finish();

private:
	static bool CheckForSong(const QString &filepath, const QByteArray &content, std::unique_ptr<MediaInformation> &result);
	static bool CheckForVideo(const QString &filepath, const QByteArray &content, std::unique_ptr<MediaInformation> &result);
	static bool CheckForImage(const QString &filepath, const QByteArray &content, std::unique_ptr<MediaInformation> &result);

	template <typename Mimes, typename Extensions>
	static bool CheckMimeOrExtensions(const QString &filepath, const QString &filemime, Mimes &mimes, Extensions &extensions);

	std::unique_ptr<MediaInformation> readMediaInformation(const QString &filemime) const {
		return ReadMediaInformation(_filepath, _content, filemime);
	}

	quint64 _id;
	FileLoadTo _to;
	QString _filepath;
	QByteArray _content;
	std::unique_ptr<MediaInformation> _information;
	QImage _image;
	qint32 _duration = 0;
	VoiceWaveform _waveform;
	SendMediaType _type;
	QString _caption;

	FileLoadResultPtr _result;

};
