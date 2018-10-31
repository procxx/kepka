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
#include "media/media_child_ffmpeg_loader.h"

constexpr AVSampleFormat AudioToFormat = AV_SAMPLE_FMT_S16;
constexpr int64_t AudioToChannelLayout = AV_CH_LAYOUT_STEREO;
constexpr qint32 AudioToChannels = 2;

VideoSoundData::~VideoSoundData() {
	if (context) {
		avcodec_close(context);
		avcodec_free_context(&context);
		context = nullptr;
	}
}

ChildFFMpegLoader::ChildFFMpegLoader(std::unique_ptr<VideoSoundData> &&data)
    : AudioPlayerLoader(FileLocation(), QByteArray(), base::byte_vector())
    , _parentData(std::move(data)) {
	_frame = av_frame_alloc();
}

bool ChildFFMpegLoader::open(qint64 &position) {
	int res = 0;
	char err[AV_ERROR_MAX_STRING_SIZE] = {0};

	auto layout = _parentData->context->channel_layout;
	if (!layout) {
		auto channelsCount = _parentData->context->channels;
		switch (channelsCount) {
		case 1: layout = AV_CH_LAYOUT_MONO; break;
		case 2: layout = AV_CH_LAYOUT_STEREO; break;
		default: LOG(("Audio Error: Unknown channel layout for %1 channels.").arg(channelsCount)); break;
		}
	}
	_inputFormat = _parentData->context->sample_fmt;
	switch (layout) {
	case AV_CH_LAYOUT_MONO:
		switch (_inputFormat) {
		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_U8P:
			_format = AL_FORMAT_MONO8;
			_sampleSize = 1;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			_format = AL_FORMAT_MONO16;
			_sampleSize = sizeof(quint16);
			break;
		default:
			_sampleSize = -1; // convert needed
			break;
		}
		break;
	case AV_CH_LAYOUT_STEREO:
		switch (_inputFormat) {
		case AV_SAMPLE_FMT_U8:
			_format = AL_FORMAT_STEREO8;
			_sampleSize = 2;
			break;
		case AV_SAMPLE_FMT_S16:
			_format = AL_FORMAT_STEREO16;
			_sampleSize = 2 * sizeof(quint16);
			break;
		default:
			_sampleSize = -1; // convert needed
			break;
		}
		break;
	default:
		_sampleSize = -1; // convert needed
		break;
	}
	if (_parentData->frequency != 44100 && _parentData->frequency != 48000) {
		_sampleSize = -1; // convert needed
	}

	if (_sampleSize < 0) {
		_swrContext = swr_alloc();
		if (!_swrContext) {
			LOG(("Audio Error: Unable to swr_alloc for file '%1', data size '%2'").arg(_file.name()).arg(_data.size()));
			return false;
		}
		int64_t src_ch_layout = layout, dst_ch_layout = AudioToChannelLayout;
		_srcRate = _parentData->frequency;
		AVSampleFormat src_sample_fmt = _inputFormat, dst_sample_fmt = AudioToFormat;
		_dstRate = (_parentData->frequency != 44100 && _parentData->frequency != 48000) ?
		               Media::Player::kDefaultFrequency :
		               _parentData->frequency;

		av_opt_set_int(_swrContext, "in_channel_layout", src_ch_layout, 0);
		av_opt_set_int(_swrContext, "in_sample_rate", _srcRate, 0);
		av_opt_set_sample_fmt(_swrContext, "in_sample_fmt", src_sample_fmt, 0);
		av_opt_set_int(_swrContext, "out_channel_layout", dst_ch_layout, 0);
		av_opt_set_int(_swrContext, "out_sample_rate", _dstRate, 0);
		av_opt_set_sample_fmt(_swrContext, "out_sample_fmt", dst_sample_fmt, 0);

		if ((res = swr_init(_swrContext)) < 0) {
			LOG(("Audio Error: Unable to swr_init for file '%1', data size '%2', error %3, %4")
			        .arg(_file.name())
			        .arg(_data.size())
			        .arg(res)
			        .arg(av_make_error_string(err, sizeof(err), res)));
			return false;
		}

		_sampleSize = AudioToChannels * sizeof(short);
		_parentData->frequency = _dstRate;
		_parentData->length = av_rescale_rnd(_parentData->length, _dstRate, _srcRate, AV_ROUND_UP);
		position = av_rescale_rnd(position, _dstRate, _srcRate, AV_ROUND_DOWN);
		_format = AL_FORMAT_STEREO16;

		_maxResampleSamples = av_rescale_rnd(AVBlockSize / _sampleSize, _dstRate, _srcRate, AV_ROUND_UP);
		if ((res = av_samples_alloc_array_and_samples(&_dstSamplesData, 0, AudioToChannels, _maxResampleSamples,
		                                              AudioToFormat, 0)) < 0) {
			LOG(("Audio Error: Unable to av_samples_alloc for file '%1', data size '%2', error %3, %4")
			        .arg(_file.name())
			        .arg(_data.size())
			        .arg(res)
			        .arg(av_make_error_string(err, sizeof(err), res)));
			return false;
		}
	}

	return true;
}

AudioPlayerLoader::ReadResult ChildFFMpegLoader::readMore(QByteArray &result, qint64 &samplesAdded) {
	int res;

	av_frame_unref(_frame);
	res = avcodec_receive_frame(_parentData->context, _frame);
	if (res >= 0) {
		return readFromReadyFrame(result, samplesAdded);
	}

	if (res == AVERROR_EOF) {
		return ReadResult::EndOfFile;
	} else if (res != AVERROR(EAGAIN)) {
		char err[AV_ERROR_MAX_STRING_SIZE] = {0};
		LOG(("Audio Error: Unable to avcodec_receive_frame() file '%1', data size '%2', error %3, %4")
		        .arg(_file.name())
		        .arg(_data.size())
		        .arg(res)
		        .arg(av_make_error_string(err, sizeof(err), res)));
		return ReadResult::Error;
	}

	if (_queue.isEmpty()) {
		return _eofReached ? ReadResult::EndOfFile : ReadResult::Wait;
	}

	AVPacket packet;
	FFMpeg::packetFromDataWrap(packet, _queue.dequeue());

	_eofReached = FFMpeg::isNullPacket(packet);
	if (_eofReached) {
		avcodec_send_packet(_parentData->context, nullptr); // drain
		return ReadResult::Ok;
	}

	res = avcodec_send_packet(_parentData->context, &packet);
	if (res < 0) {
		FFMpeg::freePacket(&packet);

		char err[AV_ERROR_MAX_STRING_SIZE] = {0};
		LOG(("Audio Error: Unable to avcodec_send_packet() file '%1', data size '%2', error %3, %4")
		        .arg(_file.name())
		        .arg(_data.size())
		        .arg(res)
		        .arg(av_make_error_string(err, sizeof(err), res)));
		// There is a sample voice message where skipping such packet
		// results in a crash (read_access to nullptr) in swr_convert().
		if (res == AVERROR_INVALIDDATA) {
			return ReadResult::NotYet; // try to skip bad packet
		}
		return ReadResult::Error;
	}
	FFMpeg::freePacket(&packet);
	return ReadResult::Ok;
}

AudioPlayerLoader::ReadResult ChildFFMpegLoader::readFromReadyFrame(QByteArray &result, qint64 &samplesAdded) {
	int res = 0;

	if (_dstSamplesData) { // convert needed
		int64_t dstSamples =
		    av_rescale_rnd(swr_get_delay(_swrContext, _srcRate) + _frame->nb_samples, _dstRate, _srcRate, AV_ROUND_UP);
		if (dstSamples > _maxResampleSamples) {
			_maxResampleSamples = dstSamples;
			av_freep(&_dstSamplesData[0]);
			if ((res = av_samples_alloc(_dstSamplesData, 0, AudioToChannels, _maxResampleSamples, AudioToFormat, 1)) <
			    0) {
				char err[AV_ERROR_MAX_STRING_SIZE] = {0};
				LOG(("Audio Error: Unable to av_samples_alloc for file '%1', data size '%2', error %3, %4")
				        .arg(_file.name())
				        .arg(_data.size())
				        .arg(res)
				        .arg(av_make_error_string(err, sizeof(err), res)));
				return ReadResult::Error;
			}
		}
		if ((res = swr_convert(_swrContext, _dstSamplesData, dstSamples, (const uint8_t **)_frame->extended_data,
		                       _frame->nb_samples)) < 0) {
			char err[AV_ERROR_MAX_STRING_SIZE] = {0};
			LOG(("Audio Error: Unable to swr_convert for file '%1', data size '%2', error %3, %4")
			        .arg(_file.name())
			        .arg(_data.size())
			        .arg(res)
			        .arg(av_make_error_string(err, sizeof(err), res)));
			return ReadResult::Error;
		}
		qint32 resultLen = av_samples_get_buffer_size(0, AudioToChannels, res, AudioToFormat, 1);
		result.append((const char *)_dstSamplesData[0], resultLen);
		samplesAdded += resultLen / _sampleSize;
	} else {
		result.append((const char *)_frame->extended_data[0], _frame->nb_samples * _sampleSize);
		samplesAdded += _frame->nb_samples;
	}
	return ReadResult::Ok;
}

void ChildFFMpegLoader::enqueuePackets(QQueue<FFMpeg::AVPacketDataWrap> &packets) {
	_queue += std::move(packets);
	packets.clear();
}

ChildFFMpegLoader::~ChildFFMpegLoader() {
	auto queue = base::take(_queue);
	for (auto &packetData : queue) {
		AVPacket packet;
		FFMpeg::packetFromDataWrap(packet, packetData);
		FFMpeg::freePacket(&packet);
	}
	if (_swrContext) swr_free(&_swrContext);
	if (_dstSamplesData) {
		if (_dstSamplesData[0]) {
			av_freep(&_dstSamplesData[0]);
		}
		av_freep(&_dstSamplesData);
	}
	av_frame_free(&_frame);
}
