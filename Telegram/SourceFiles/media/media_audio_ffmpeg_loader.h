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

#include "media/media_audio.h"
#include "media/media_audio_loader.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
} // extern "C"

#include <AL/al.h>

class AbstractFFMpegLoader : public AudioPlayerLoader {
public:
	AbstractFFMpegLoader(const FileLocation &file, const QByteArray &data, base::byte_vector &&bytes) : AudioPlayerLoader(file, data, std::move(bytes)) {
	}

	bool open(int64_t &position) override;

	int64_t samplesCount() override {
		return _samplesCount;
	}

	int32_t samplesFrequency() override {
		return _samplesFrequency;
	}

	~AbstractFFMpegLoader();

protected:
	int32_t _samplesFrequency = Media::Player::kDefaultFrequency;
	int64_t _samplesCount = 0;

	uchar *ioBuffer = nullptr;
	AVIOContext *ioContext = nullptr;
	AVFormatContext *fmtContext = nullptr;
	AVCodec *codec = nullptr;
	int32_t streamId = 0;

	bool _opened = false;

private:
	static int _read_data(void *opaque, uint8_t *buf, int buf_size);
	static int64_t _seek_data(void *opaque, int64_t offset, int whence);
	static int _read_bytes(void *opaque, uint8_t *buf, int buf_size);
	static int64_t _seek_bytes(void *opaque, int64_t offset, int whence);
	static int _read_file(void *opaque, uint8_t *buf, int buf_size);
	static int64_t _seek_file(void *opaque, int64_t offset, int whence);

};

class FFMpegLoader : public AbstractFFMpegLoader {
public:
	FFMpegLoader(const FileLocation &file, const QByteArray &data, base::byte_vector &&bytes);

	bool open(int64_t &position) override;

	int32_t format() override {
		return fmt;
	}

	ReadResult readMore(QByteArray &result, int64_t &samplesAdded) override;

	~FFMpegLoader();

protected:
	int32_t sampleSize = 2 * sizeof(uint16_t);

private:
	ReadResult readFromReadyFrame(QByteArray &result, int64_t &samplesAdded);

	int32_t fmt = AL_FORMAT_STEREO16;
	int32_t srcRate = Media::Player::kDefaultFrequency;
	int32_t dstRate = Media::Player::kDefaultFrequency;
	int32_t maxResampleSamples = 1024;
	uint8_t **dstSamplesData = nullptr;

	AVCodecContext *codecContext = nullptr;
	AVPacket avpkt;
	AVSampleFormat inputFormat;
	AVFrame *frame = nullptr;

	SwrContext *swrContext = nullptr;

};
