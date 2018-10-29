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

#include "core/utils.h"
#include "ui/images.h"
#include <QByteArray>

namespace FFMpeg {
struct AVPacketDataWrap;
} // namespace FFMpeg

class AudioPlayerLoader {
public:
	AudioPlayerLoader(const FileLocation &file, const QByteArray &data, base::byte_vector &&bytes);
	virtual ~AudioPlayerLoader();

	virtual bool check(const FileLocation &file, const QByteArray &data);

	virtual bool open(qint64 &position) = 0;
	virtual qint64 samplesCount() = 0;
	virtual qint32 samplesFrequency() = 0;
	virtual qint32 format() = 0;

	enum class ReadResult {
		Error,
		NotYet,
		Ok,
		Wait,
		EndOfFile,
	};
	virtual ReadResult readMore(QByteArray &samples, qint64 &samplesCount) = 0;
	virtual void enqueuePackets(QQueue<FFMpeg::AVPacketDataWrap> &packets) {
		Unexpected("enqueuePackets() call on not ChildFFMpegLoader.");
	}

	void saveDecodedSamples(QByteArray *samples, qint64 *samplesCount);
	void takeSavedDecodedSamples(QByteArray *samples, qint64 *samplesCount);
	bool holdsSavedDecodedSamples() const;

protected:
	FileLocation _file;
	bool _access = false;
	QByteArray _data;
	base::byte_vector _bytes;

	QFile _f;
	int _dataPos = 0;

	bool openFile();

private:
	QByteArray _savedSamples;
	qint64 _savedSamplesCount = 0;
	bool _holdsSavedSamples = false;
};
