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
#include <QWindow>

#include "app.h"
#include "facades.h"

#include "base/task_queue.h"
#include "media/media_audio.h"
#include "media/media_audio_ffmpeg_loader.h"
#include "media/media_audio_loaders.h"
#include "media/media_audio_track.h"
#include "media/media_child_ffmpeg_loader.h"
#include "platform/platform_audio.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <numeric>

Q_DECLARE_METATYPE(AudioMsgId);
Q_DECLARE_METATYPE(VoiceWaveform);

namespace {

QMutex AudioMutex;
ALCdevice *AudioDevice = nullptr;
ALCcontext *AudioContext = nullptr;

constexpr auto kSuppressRatioAll = 0.2;
constexpr auto kSuppressRatioSong = 0.05;

auto VolumeMultiplierAll = 1.;
auto VolumeMultiplierSong = 1.;

} // namespace

namespace Media {
namespace Audio {
namespace {

Player::Mixer *MixerInstance = nullptr;

// Thread: Any.
bool ContextErrorHappened() {
	ALenum errCode;
	if ((errCode = alcGetError(AudioDevice)) != ALC_NO_ERROR) {
		LOG(("Audio Context Error: %1, %2").arg(errCode).arg((const char *)alcGetString(AudioDevice, errCode)));
		return true;
	}
	return false;
}

// Thread: Any.
bool PlaybackErrorHappened() {
	ALenum errCode;
	if ((errCode = alGetError()) != AL_NO_ERROR) {
		LOG(("Audio Playback Error: %1, %2").arg(errCode).arg((const char *)alGetString(errCode)));
		return true;
	}
	return false;
}

void EnumeratePlaybackDevices() {
	auto deviceNames = QStringList();
	auto devices = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
	Assert(devices != nullptr);
	while (*devices != 0) {
		auto deviceName8Bit = QByteArray(devices);
		auto deviceName = QString::fromLocal8Bit(deviceName8Bit);
		deviceNames.append(deviceName);
		devices += deviceName8Bit.size() + 1;
	}
	LOG(("Audio Playback Devices: %1").arg(deviceNames.join(';')));

	if (auto device = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER)) {
		LOG(("Audio Playback Default Device: %1").arg(QString::fromLocal8Bit(device)));
	} else {
		LOG(("Audio Playback Default Device: (null)"));
	}
}

void EnumerateCaptureDevices() {
	auto deviceNames = QStringList();
	auto devices = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
	Assert(devices != nullptr);
	while (*devices != 0) {
		auto deviceName8Bit = QByteArray(devices);
		auto deviceName = QString::fromLocal8Bit(deviceName8Bit);
		deviceNames.append(deviceName);
		devices += deviceName8Bit.size() + 1;
	}
	LOG(("Audio Capture Devices: %1").arg(deviceNames.join(';')));

	if (auto device = alcGetString(nullptr, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER)) {
		LOG(("Audio Capture Default Device: %1").arg(QString::fromLocal8Bit(device)));
	} else {
		LOG(("Audio Capture Default Device: (null)"));
	}
}

// Thread: Any. Must be locked: AudioMutex.
void DestroyPlaybackDevice() {
	if (AudioContext) {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(AudioContext);
		AudioContext = nullptr;
	}

	if (AudioDevice) {
		alcCloseDevice(AudioDevice);
		AudioDevice = nullptr;
	}
}

// Thread: Any. Must be locked: AudioMutex.
bool CreatePlaybackDevice() {
	if (AudioDevice) return true;

	AudioDevice = alcOpenDevice(nullptr);
	if (!AudioDevice) {
		LOG(("Audio Error: Could not create default playback device, enumerating.."));
		EnumeratePlaybackDevices();
		return false;
	}

	ALCint attributes[] = {ALC_STEREO_SOURCES, 128, 0};
	AudioContext = alcCreateContext(AudioDevice, attributes);
	alcMakeContextCurrent(AudioContext);
	if (ContextErrorHappened()) {
		DestroyPlaybackDevice();
		return false;
	}

	ALfloat v[] = {0.f, 0.f, -1.f, 0.f, 1.f, 0.f};
	alListener3f(AL_POSITION, 0.f, 0.f, 0.f);
	alListener3f(AL_VELOCITY, 0.f, 0.f, 0.f);
	alListenerfv(AL_ORIENTATION, v);

	alDistanceModel(AL_NONE);

	return true;
}

// Thread: Main. Must be locked: AudioMutex.
void ClosePlaybackDevice() {
	if (!AudioDevice) return;

	LOG(("Audio Info: Closing audio playback device."));
	if (Player::mixer()) {
		Player::mixer()->detachTracks();
	}
	Current().detachTracks();

	DestroyPlaybackDevice();
}

} // namespace

// Thread: Main.
void Start() {
	Assert(AudioDevice == nullptr);

	qRegisterMetaType<AudioMsgId>();
	qRegisterMetaType<VoiceWaveform>();

	auto loglevel = getenv("ALSOFT_LOGLEVEL");
	LOG(("OpenAL Logging Level: %1").arg(loglevel ? loglevel : "(not set)"));

	EnumeratePlaybackDevices();
	EnumerateCaptureDevices();

	MixerInstance = new Player::Mixer();

	Platform::Audio::Init();
}

// Thread: Main.
void Finish() {
	Platform::Audio::DeInit();

	// MixerInstance variable should be modified under AudioMutex protection.
	// So it is modified in the ~Mixer() destructor after all tracks are cleared.
	delete MixerInstance;

	// No sync required already.
	ClosePlaybackDevice();
}

// Thread: Main. Locks: AudioMutex.
bool IsAttachedToDevice() {
	QMutexLocker lock(&AudioMutex);
	return (AudioDevice != nullptr);
}

// Thread: Any. Must be locked: AudioMutex.
bool AttachToDevice() {
	if (AudioDevice) {
		return true;
	}
	LOG(("Audio Info: recreating audio device and reattaching the tracks"));

	CreatePlaybackDevice();
	if (!AudioDevice) {
		return false;
	}

	if (auto m = Player::mixer()) {
		m->reattachTracks();
		emit m->faderOnTimer();
	}

	base::TaskQueue::Main().Put([] { Current().reattachTracks(); });
	return true;
}

void ScheduleDetachFromDeviceSafe() {
	base::TaskQueue::Main().Put([] { Current().scheduleDetachFromDevice(); });
}

void ScheduleDetachIfNotUsedSafe() {
	base::TaskQueue::Main().Put([] { Current().scheduleDetachIfNotUsed(); });
}

void StopDetachIfNotUsedSafe() {
	base::TaskQueue::Main().Put([] { Current().stopDetachIfNotUsed(); });
}

} // namespace Audio

namespace Player {
namespace {

constexpr auto kVolumeRound = 10000;
constexpr auto kPreloadSamples = 2LL * 48000; // preload next part if less than 2 seconds remains
constexpr auto kFadeDuration = TimeMs(500);
constexpr auto kCheckPlaybackPositionTimeout = TimeMs(100); // 100ms per check audio position
constexpr auto kCheckPlaybackPositionDelta = 2400LL; // update position called each 2400 samples
constexpr auto kCheckFadingTimeout = TimeMs(7); // 7ms

base::Observable<AudioMsgId> UpdatedObservable;

} // namespace

base::Observable<AudioMsgId> &Updated() {
	return UpdatedObservable;
}

// Thread: Any. Must be locked: AudioMutex.
double ComputeVolume(AudioMsgId::Type type) {
	switch (type) {
	case AudioMsgId::Type::Voice: return VolumeMultiplierAll;
	case AudioMsgId::Type::Song: return VolumeMultiplierSong * mixer()->getSongVolume();
	case AudioMsgId::Type::Video: return mixer()->getVideoVolume();
	}
	return 1.;
}

Mixer *mixer() {
	return Audio::MixerInstance;
}

void Mixer::Track::createStream() {
	alGenSources(1, &stream.source);
	alSourcef(stream.source, AL_PITCH, 1.f);
	alSource3f(stream.source, AL_POSITION, 0, 0, 0);
	alSource3f(stream.source, AL_VELOCITY, 0, 0, 0);
	alSourcei(stream.source, AL_LOOPING, 0);
	alSourcei(stream.source, AL_DIRECT_CHANNELS_SOFT, 1);
	alGenBuffers(3, stream.buffers);
}

void Mixer::Track::destroyStream() {
	if (isStreamCreated()) {
		alDeleteBuffers(3, stream.buffers);
		alDeleteSources(1, &stream.source);
	}
	stream.source = 0;
	for (auto i = 0; i != 3; ++i) {
		stream.buffers[i] = 0;
	}
}

void Mixer::Track::reattach(AudioMsgId::Type type) {
	if (isStreamCreated() || !samplesCount[0]) {
		return;
	}

	createStream();
	for (auto i = 0; i != kBuffersCount; ++i) {
		if (!samplesCount[i]) {
			break;
		}
		alBufferData(stream.buffers[i], format, bufferSamples[i].constData(), bufferSamples[i].size(), frequency);
		alSourceQueueBuffers(stream.source, 1, stream.buffers + i);
	}

	alSourcei(stream.source, AL_SAMPLE_OFFSET, std::max(state.position - bufferedPosition, Q_INT64_C(0)));
	if (!IsStopped(state.state) && state.state != State::PausedAtEnd) {
		alSourcef(stream.source, AL_GAIN, ComputeVolume(type));
		alSourcePlay(stream.source);
		if (IsPaused(state.state)) {
			// We must always start the source if we want the AL_SAMPLE_OFFSET to be applied.
			// Otherwise it won't be read by alGetSource and we'll get a corrupt position.
			// So in case of a paused source we start it and then immediately pause it.
			alSourcePause(stream.source);
		}
	}
}

void Mixer::Track::detach() {
	resetStream();
	destroyStream();
}

void Mixer::Track::clear() {
	detach();

	state = TrackState();
	file = FileLocation();
	data = QByteArray();
	bufferedPosition = 0;
	bufferedLength = 0;
	loading = false;
	loaded = false;
	fadeStartPosition = 0;

	format = 0;
	frequency = kDefaultFrequency;
	for (int i = 0; i != kBuffersCount; ++i) {
		samplesCount[i] = 0;
		bufferSamples[i] = QByteArray();
	}

	videoData = nullptr;
	lastUpdateWhen = 0;
	lastUpdateCorrectedMs = 0;
}

void Mixer::Track::started() {
	resetStream();

	bufferedPosition = 0;
	bufferedLength = 0;
	loaded = false;
	fadeStartPosition = 0;

	format = 0;
	frequency = kDefaultFrequency;
	for (auto i = 0; i != kBuffersCount; ++i) {
		samplesCount[i] = 0;
		bufferSamples[i] = QByteArray();
	}
}

bool Mixer::Track::isStreamCreated() const {
	return alIsSource(stream.source);
}

void Mixer::Track::ensureStreamCreated() {
	if (!isStreamCreated()) {
		createStream();
	}
}

int Mixer::Track::getNotQueuedBufferIndex() {
	// See if there are no free buffers right now.
	while (samplesCount[kBuffersCount - 1] != 0) {
		// Try to unqueue some buffer.
		ALint processed = 0;
		alGetSourcei(stream.source, AL_BUFFERS_PROCESSED, &processed);
		if (processed < 1) { // No processed buffers, wait.
			return -1;
		}

		// Unqueue some processed buffer.
		ALuint buffer = 0;
		alSourceUnqueueBuffers(stream.source, 1, &buffer);

		// Find it in the list and clear it.
		bool found = false;
		for (auto i = 0; i != kBuffersCount; ++i) {
			if (stream.buffers[i] == buffer) {
				auto samplesInBuffer = samplesCount[i];
				bufferedPosition += samplesInBuffer;
				bufferedLength -= samplesInBuffer;
				for (auto j = i + 1; j != kBuffersCount; ++j) {
					samplesCount[j - 1] = samplesCount[j];
					stream.buffers[j - 1] = stream.buffers[j];
					bufferSamples[j - 1] = bufferSamples[j];
				}
				samplesCount[kBuffersCount - 1] = 0;
				stream.buffers[kBuffersCount - 1] = buffer;
				bufferSamples[kBuffersCount - 1] = QByteArray();
				found = true;
				break;
			}
		}
		if (!found) {
			LOG(("Audio Error: Could not find the unqueued buffer! Buffer %1 in source %2 with processed count %3")
			        .arg(buffer)
			        .arg(stream.source)
			        .arg(processed));
			return -1;
		}
	}

	for (auto i = 0; i != kBuffersCount; ++i) {
		if (!samplesCount[i]) {
			return i;
		}
	}
	return -1;
}

void Mixer::Track::resetStream() {
	if (isStreamCreated()) {
		alSourceStop(stream.source);
		alSourcei(stream.source, AL_BUFFER, AL_NONE);
	}
}

Mixer::Track::~Track() = default;

Mixer::Mixer()
    : _volumeVideo(kVolumeRound)
    , _volumeSong(kVolumeRound)
    , _fader(new Fader(&_faderThread))
    , _loader(new Loaders(&_loaderThread)) {
	connect(this, SIGNAL(faderOnTimer()), _fader, SLOT(onTimer()), Qt::QueuedConnection);
	connect(this, SIGNAL(suppressSong()), _fader, SLOT(onSuppressSong()));
	connect(this, SIGNAL(unsuppressSong()), _fader, SLOT(onUnsuppressSong()));
	connect(this, SIGNAL(suppressAll(qint64)), _fader, SLOT(onSuppressAll(qint64)));
	subscribe(Global::RefSongVolumeChanged(), [this] { QMetaObject::invokeMethod(_fader, "onSongVolumeChanged"); });
	subscribe(Global::RefVideoVolumeChanged(), [this] { QMetaObject::invokeMethod(_fader, "onVideoVolumeChanged"); });
	connect(this, SIGNAL(loaderOnStart(const AudioMsgId &, qint64)), _loader,
	        SLOT(onStart(const AudioMsgId &, qint64)));
	connect(this, SIGNAL(loaderOnCancel(const AudioMsgId &)), _loader, SLOT(onCancel(const AudioMsgId &)));
	connect(_loader, SIGNAL(needToCheck()), _fader, SLOT(onTimer()));
	connect(_loader, SIGNAL(error(const AudioMsgId &)), this, SLOT(onError(const AudioMsgId &)));
	connect(_fader, SIGNAL(needToPreload(const AudioMsgId &)), _loader, SLOT(onLoad(const AudioMsgId &)));
	connect(_fader, SIGNAL(playPositionUpdated(const AudioMsgId &)), this, SIGNAL(updated(const AudioMsgId &)));
	connect(_fader, SIGNAL(audioStopped(const AudioMsgId &)), this, SLOT(onStopped(const AudioMsgId &)));
	connect(_fader, SIGNAL(error(const AudioMsgId &)), this, SLOT(onError(const AudioMsgId &)));
	connect(this, SIGNAL(stoppedOnError(const AudioMsgId &)), this, SIGNAL(updated(const AudioMsgId &)),
	        Qt::QueuedConnection);
	connect(this, SIGNAL(updated(const AudioMsgId &)), this, SLOT(onUpdated(const AudioMsgId &)));

	_loaderThread.start();
	_faderThread.start();
}

// Thread: Main. Locks: AudioMutex.
Mixer::~Mixer() {
	{
		QMutexLocker lock(&AudioMutex);

		for (auto i = 0; i != kTogetherLimit; ++i) {
			trackForType(AudioMsgId::Type::Voice, i)->clear();
			trackForType(AudioMsgId::Type::Song, i)->clear();
		}
		_videoTrack.clear();

		Audio::ClosePlaybackDevice();
		Audio::MixerInstance = nullptr;
	}

	_faderThread.quit();
	_loaderThread.quit();
	_faderThread.wait();
	_loaderThread.wait();
}

void Mixer::onUpdated(const AudioMsgId &audio) {
	if (audio.playId()) {
		videoSoundProgress(audio);
	}
	Media::Player::Updated().notify(audio);
}

void Mixer::onError(const AudioMsgId &audio) {
	emit stoppedOnError(audio);

	QMutexLocker lock(&AudioMutex);
	auto type = audio.type();
	if (type == AudioMsgId::Type::Voice) {
		if (auto current = trackForType(type)) {
			if (current->state.id == audio) {
				emit unsuppressSong();
			}
		}
	}
}

void Mixer::onStopped(const AudioMsgId &audio) {
	emit updated(audio);

	QMutexLocker lock(&AudioMutex);
	auto type = audio.type();
	if (type == AudioMsgId::Type::Voice) {
		if (auto current = trackForType(type)) {
			if (current->state.id == audio) {
				emit unsuppressSong();
			}
		}
	}
}

Mixer::Track *Mixer::trackForType(AudioMsgId::Type type, int index) {
	if (index < 0) {
		if (auto indexPtr = currentIndex(type)) {
			index = *indexPtr;
		} else {
			return nullptr;
		}
	}
	switch (type) {
	case AudioMsgId::Type::Voice: return &_audioTracks[index];
	case AudioMsgId::Type::Song: return &_songTracks[index];
	case AudioMsgId::Type::Video: return &_videoTrack;
	}
	return nullptr;
}

const Mixer::Track *Mixer::trackForType(AudioMsgId::Type type, int index) const {
	return const_cast<Mixer *>(this)->trackForType(type, index);
}

int *Mixer::currentIndex(AudioMsgId::Type type) {
	switch (type) {
	case AudioMsgId::Type::Voice: return &_audioCurrent;
	case AudioMsgId::Type::Song: return &_songCurrent;
	case AudioMsgId::Type::Video: {
		static int videoIndex = 0;
		return &videoIndex;
	}
	}
	return nullptr;
}

const int *Mixer::currentIndex(AudioMsgId::Type type) const {
	return const_cast<Mixer *>(this)->currentIndex(type);
}

void Mixer::resetFadeStartPosition(AudioMsgId::Type type, int positionInBuffered) {
	auto track = trackForType(type);
	if (!track) return;

	if (positionInBuffered < 0) {
		Audio::AttachToDevice();
		if (track->isStreamCreated()) {
			ALint currentPosition = 0;
			alGetSourcei(track->stream.source, AL_SAMPLE_OFFSET, &currentPosition);
			if (Audio::PlaybackErrorHappened()) {
				setStoppedState(track, State::StoppedAtError);
				onError(track->state.id);
				return;
			}

			if (currentPosition == 0 && !internal::CheckAudioDeviceConnected()) {
				track->fadeStartPosition = track->state.position;
				return;
			}

			positionInBuffered = currentPosition;
		} else {
			positionInBuffered = 0;
		}
	}
	auto fullPosition = track->bufferedPosition + positionInBuffered;
	track->state.position = fullPosition;
	track->fadeStartPosition = fullPosition;
}

bool Mixer::fadedStop(AudioMsgId::Type type, bool *fadedStart) {
	auto current = trackForType(type);
	if (!current) return false;

	switch (current->state.state) {
	case State::Starting:
	case State::Resuming:
	case State::Playing: {
		current->state.state = State::Stopping;
		resetFadeStartPosition(type);
		if (fadedStart) *fadedStart = true;
	} break;
	case State::Pausing: {
		current->state.state = State::Stopping;
		if (fadedStart) *fadedStart = true;
	} break;
	case State::Paused:
	case State::PausedAtEnd: {
		setStoppedState(current);
	}
		return true;
	}
	return false;
}

void Mixer::play(const AudioMsgId &audio, qint64 position) {
	setSongVolume(Global::SongVolume());
	play(audio, nullptr, position);
}

void Mixer::play(const AudioMsgId &audio, std::unique_ptr<VideoSoundData> videoData, qint64 position) {
	Expects(!videoData || audio.playId() != 0);

	auto type = audio.type();
	AudioMsgId stopped;
	auto notLoadedYet = false;
	{
		QMutexLocker lock(&AudioMutex);
		Audio::AttachToDevice();
		if (!AudioDevice) return;

		auto fadedStart = false;
		auto current = trackForType(type);
		if (!current) return;

		if (type == AudioMsgId::Type::Video) {
			auto pauseType = [this](AudioMsgId::Type type) {
				auto current = trackForType(type);
				switch (current->state.state) {
				case State::Starting:
				case State::Resuming:
				case State::Playing: {
					current->state.state = State::Pausing;
					resetFadeStartPosition(type);
				} break;
				case State::Stopping: {
					current->state.state = State::Pausing;
				} break;
				}
			};

			pauseType(AudioMsgId::Type::Song);
			pauseType(AudioMsgId::Type::Voice);
		}

		if (current->state.id != audio) {
			if (fadedStop(type, &fadedStart)) {
				stopped = current->state.id;
			}
			if (current->state.id) {
				emit loaderOnCancel(current->state.id);
				emit faderOnTimer();
			}
			if (type == AudioMsgId::Type::Video) {
				current->clear();
			} else {
				auto foundCurrent = currentIndex(type);
				auto index = 0;
				for (; index != kTogetherLimit; ++index) {
					if (trackForType(type, index)->state.id == audio) {
						*foundCurrent = index;
						break;
					}
				}
				if (index == kTogetherLimit && ++*foundCurrent >= kTogetherLimit) {
					*foundCurrent -= kTogetherLimit;
				}
				current = trackForType(type);
			}
		}

		current->state.id = audio;
		current->lastUpdateWhen = 0;
		current->lastUpdateCorrectedMs = 0;
		if (videoData) {
			current->videoData = std::move(videoData);
		} else {
			current->file = audio.audio()->location(true);
			current->data = audio.audio()->data();
			notLoadedYet = (current->file.isEmpty() && current->data.isEmpty());
		}
		if (notLoadedYet) {
			auto newState = (type == AudioMsgId::Type::Song) ? State::Stopped : State::StoppedAtError;
			setStoppedState(current, newState);
		} else {
			current->state.position = position;
			current->state.state = current->videoData ? State::Paused : fadedStart ? State::Starting : State::Playing;
			current->loading = true;
			emit loaderOnStart(current->state.id, position);
			if (type == AudioMsgId::Type::Voice) {
				emit suppressSong();
			}
		}
	}
	if (notLoadedYet) {
		if (type == AudioMsgId::Type::Song || type == AudioMsgId::Type::Video) {
			DocumentOpenClickHandler::doOpen(audio.audio(), App::histItemById(audio.contextId()));
		} else {
			onError(audio);
		}
	}
	if (stopped) {
		emit updated(stopped);
	}
}

void Mixer::feedFromVideo(VideoSoundPart &&part) {
	_loader->feedFromVideo(std::move(part));
}

TimeMs Mixer::getVideoCorrectedTime(const AudioMsgId &audio, TimeMs frameMs, TimeMs systemMs) {
	auto result = frameMs;

	QMutexLocker lock(&AudioMutex);
	auto type = audio.type();
	auto track = trackForType(type);
	if (track && track->state.id == audio && track->lastUpdateWhen > 0) {
		result = static_cast<TimeMs>(track->lastUpdateCorrectedMs);
		if (systemMs > track->lastUpdateWhen) {
			result += (systemMs - track->lastUpdateWhen);
		}
	}

	return result;
}

void Mixer::videoSoundProgress(const AudioMsgId &audio) {
	auto type = audio.type();

	QMutexLocker lock(&AudioMutex);

	auto current = trackForType(type);
	if (current && current->state.length && current->state.frequency) {
		if (current->state.id == audio && current->state.state == State::Playing) {
			current->lastUpdateWhen = getms();
			current->lastUpdateCorrectedMs = (current->state.position * 1000ULL) / current->state.frequency;
		}
	}
}

bool Mixer::checkCurrentALError(AudioMsgId::Type type) {
	if (!Audio::PlaybackErrorHappened()) return true;

	auto data = trackForType(type);
	if (!data) {
		setStoppedState(data, State::StoppedAtError);
		onError(data->state.id);
	}
	return false;
}

void Mixer::pause(const AudioMsgId &audio, bool fast) {
	AudioMsgId current;
	{
		QMutexLocker lock(&AudioMutex);
		auto type = audio.type();
		auto track = trackForType(type);
		if (!track || track->state.id != audio) {
			return;
		}

		current = track->state.id;
		switch (track->state.state) {
		case State::Starting:
		case State::Resuming:
		case State::Playing: {
			track->state.state = fast ? State::Paused : State::Pausing;
			resetFadeStartPosition(type);
			if (type == AudioMsgId::Type::Voice) {
				emit unsuppressSong();
			}
		} break;

		case State::Pausing:
		case State::Stopping: {
			track->state.state = fast ? State::Paused : State::Pausing;
		} break;
		}

		if (fast && track->isStreamCreated()) {
			ALint state = AL_INITIAL;
			alGetSourcei(track->stream.source, AL_SOURCE_STATE, &state);
			if (!checkCurrentALError(type)) return;

			if (state == AL_PLAYING) {
				alSourcePause(track->stream.source);
				if (!checkCurrentALError(type)) return;
			}
		}

		emit faderOnTimer();

		track->lastUpdateWhen = 0;
		track->lastUpdateCorrectedMs = 0;
	}
	if (current) emit updated(current);
}

void Mixer::resume(const AudioMsgId &audio, bool fast) {
	AudioMsgId current;
	{
		QMutexLocker lock(&AudioMutex);
		auto type = audio.type();
		auto track = trackForType(type);
		if (!track || track->state.id != audio) {
			return;
		}

		current = track->state.id;
		switch (track->state.state) {
		case State::Pausing:
		case State::Paused:
		case State::PausedAtEnd: {
			if (track->state.state == State::Paused) {
				// This calls Audio::AttachToDevice().
				resetFadeStartPosition(type);
			} else {
				Audio::AttachToDevice();
				if (track->state.state == State::PausedAtEnd) {
					if (track->isStreamCreated()) {
						alSourcei(track->stream.source, AL_SAMPLE_OFFSET,
						          std::max(track->state.position - track->bufferedPosition, Q_INT64_C(0)));
						if (!checkCurrentALError(type)) return;
					}
				}
			}
			track->state.state = fast ? State::Playing : State::Resuming;

			if (track->isStreamCreated()) {
				// When starting the video audio is in paused state and
				// gets resumed before the stream is created with any data.
				ALint state = AL_INITIAL;
				alGetSourcei(track->stream.source, AL_SOURCE_STATE, &state);
				if (!checkCurrentALError(type)) return;

				if (state != AL_PLAYING) {
					if (state == AL_STOPPED && !internal::CheckAudioDeviceConnected()) {
						return;
					}

					alSourcef(track->stream.source, AL_GAIN, ComputeVolume(type));
					if (!checkCurrentALError(type)) return;

					alSourcePlay(track->stream.source);
					if (!checkCurrentALError(type)) return;
				}
				if (type == AudioMsgId::Type::Voice) {
					emit suppressSong();
				}
			}
		} break;
		}
		emit faderOnTimer();
	}
	if (current) emit updated(current);
}

void Mixer::seek(AudioMsgId::Type type, qint64 position) {
	QMutexLocker lock(&AudioMutex);

	auto current = trackForType(type);
	auto audio = current->state.id;

	Audio::AttachToDevice();
	auto streamCreated = current->isStreamCreated();
	auto fastSeek =
	    (position >= current->bufferedPosition &&
	     position < current->bufferedPosition + current->bufferedLength - (current->loaded ? 0 : kDefaultFrequency));
	if (!streamCreated) {
		fastSeek = false;
	} else if (IsStoppedOrStopping(current->state.state)) {
		fastSeek = false;
	}
	if (fastSeek) {
		alSourcei(current->stream.source, AL_SAMPLE_OFFSET, position - current->bufferedPosition);
		if (!checkCurrentALError(type)) return;

		alSourcef(current->stream.source, AL_GAIN, ComputeVolume(type));
		if (!checkCurrentALError(type)) return;

		resetFadeStartPosition(type, position - current->bufferedPosition);
	} else {
		setStoppedState(current);
	}
	switch (current->state.state) {
	case State::Pausing:
	case State::Paused:
	case State::PausedAtEnd: {
		if (current->state.state == State::PausedAtEnd) {
			current->state.state = State::Paused;
		}
		lock.unlock();
		return resume(audio, true);
	} break;
	case State::Starting:
	case State::Resuming:
	case State::Playing: {
		current->state.state = State::Pausing;
		resetFadeStartPosition(type);
		if (type == AudioMsgId::Type::Voice) {
			emit unsuppressSong();
		}
	} break;
	case State::Stopping:
	case State::Stopped:
	case State::StoppedAtEnd:
	case State::StoppedAtError:
	case State::StoppedAtStart: {
		lock.unlock();
	}
		return play(audio, position);
	}
	emit faderOnTimer();
}

void Mixer::stop(const AudioMsgId &audio) {
	AudioMsgId current;
	{
		QMutexLocker lock(&AudioMutex);
		auto type = audio.type();
		auto track = trackForType(type);
		if (!track || track->state.id != audio) {
			return;
		}

		current = track->state.id;
		fadedStop(type);
		if (type == AudioMsgId::Type::Voice) {
			emit unsuppressSong();
		} else if (type == AudioMsgId::Type::Video) {
			track->clear();
		}
	}
	if (current) emit updated(current);
}

void Mixer::stop(const AudioMsgId &audio, State state) {
	Expects(IsStopped(state));

	AudioMsgId current;
	{
		QMutexLocker lock(&AudioMutex);
		auto type = audio.type();
		auto track = trackForType(type);
		if (!track || track->state.id != audio || IsStopped(track->state.state)) {
			return;
		}

		current = track->state.id;
		setStoppedState(track, state);
		if (type == AudioMsgId::Type::Voice) {
			emit unsuppressSong();
		} else if (type == AudioMsgId::Type::Video) {
			track->clear();
		}
	}
	if (current) emit updated(current);
}

void Mixer::stopAndClear() {
	Track *current_audio = nullptr, *current_song = nullptr;
	{
		QMutexLocker lock(&AudioMutex);
		if ((current_audio = trackForType(AudioMsgId::Type::Voice))) {
			setStoppedState(current_audio);
		}
		if ((current_song = trackForType(AudioMsgId::Type::Song))) {
			setStoppedState(current_song);
		}
	}
	if (current_song) {
		emit updated(current_song->state.id);
	}
	if (current_audio) {
		emit updated(current_audio->state.id);
	}
	{
		QMutexLocker lock(&AudioMutex);
		auto clearAndCancel = [this](AudioMsgId::Type type, int index) {
			auto track = trackForType(type, index);
			if (track->state.id) {
				emit loaderOnCancel(track->state.id);
			}
			track->clear();
		};
		for (auto index = 0; index != kTogetherLimit; ++index) {
			clearAndCancel(AudioMsgId::Type::Voice, index);
			clearAndCancel(AudioMsgId::Type::Song, index);
		}
		_videoTrack.clear();
	}
}

TrackState Mixer::currentState(AudioMsgId::Type type) {
	QMutexLocker lock(&AudioMutex);
	auto current = trackForType(type);
	if (!current) {
		return TrackState();
	}
	return current->state;
}

void Mixer::setStoppedState(Track *current, State state) {
	current->state.state = state;
	current->state.position = 0;
	if (current->isStreamCreated()) {
		alSourceStop(current->stream.source);
		alSourcef(current->stream.source, AL_GAIN, 1);
	}
}

void Mixer::clearStoppedAtStart(const AudioMsgId &audio) {
	QMutexLocker lock(&AudioMutex);
	auto track = trackForType(audio.type());
	if (track && track->state.id == audio && track->state.state == State::StoppedAtStart) {
		setStoppedState(track);
	}
}

// Thread: Main. Must be locked: AudioMutex.
void Mixer::detachTracks() {
	for (auto i = 0; i != kTogetherLimit; ++i) {
		trackForType(AudioMsgId::Type::Voice, i)->detach();
		trackForType(AudioMsgId::Type::Song, i)->detach();
	}
	_videoTrack.detach();
}

// Thread: Main. Must be locked: AudioMutex.
void Mixer::reattachIfNeeded() {
	Audio::Current().stopDetachIfNotUsed();

	auto reattachNeeded = [this] {
		auto isPlayingState = [](const Track &track) {
			auto state = track.state.state;
			return (state == State::Playing) || IsFading(state);
		};
		for (auto i = 0; i != kTogetherLimit; ++i) {
			if (isPlayingState(*trackForType(AudioMsgId::Type::Voice, i)) ||
			    isPlayingState(*trackForType(AudioMsgId::Type::Song, i))) {
				return true;
			}
		}
		return isPlayingState(_videoTrack);
	};

	if (reattachNeeded() || Audio::Current().hasActiveTracks()) {
		Audio::AttachToDevice();
	}
}

// Thread: Any. Must be locked: AudioMutex.
void Mixer::reattachTracks() {
	for (auto i = 0; i != kTogetherLimit; ++i) {
		trackForType(AudioMsgId::Type::Voice, i)->reattach(AudioMsgId::Type::Voice);
		trackForType(AudioMsgId::Type::Song, i)->reattach(AudioMsgId::Type::Song);
	}
	_videoTrack.reattach(AudioMsgId::Type::Video);
}

void Mixer::setSongVolume(double volume) {
	_volumeSong.storeRelease(std::round(volume * kVolumeRound));
}

double Mixer::getSongVolume() const {
	return double(_volumeSong.loadAcquire()) / kVolumeRound;
}

void Mixer::setVideoVolume(double volume) {
	_volumeVideo.storeRelease(std::round(volume * kVolumeRound));
}

double Mixer::getVideoVolume() const {
	return double(_volumeVideo.loadAcquire()) / kVolumeRound;
}

Fader::Fader(QThread *thread)
    : QObject()
    , _timer(this)
    , _suppressVolumeAll(1., 1.)
    , _suppressVolumeSong(1., 1.) {
	moveToThread(thread);
	_timer.moveToThread(thread);
	connect(thread, SIGNAL(started()), this, SLOT(onInit()));
	connect(thread, SIGNAL(finished()), this, SLOT(deleteLater()));

	_timer.setSingleShot(true);
	connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
}

void Fader::onInit() {}

void Fader::onTimer() {
	QMutexLocker lock(&AudioMutex);
	if (!mixer()) return;

	auto volumeChangedAll = false;
	auto volumeChangedSong = false;
	if (_suppressAll || _suppressSongAnim) {
		auto ms = getms();
		if (_suppressAll) {
			if (ms >= _suppressAllEnd || ms < _suppressAllStart) {
				_suppressAll = _suppressAllAnim = false;
				_suppressVolumeAll = anim::value(1., 1.);
			} else if (ms > _suppressAllEnd - kFadeDuration) {
				if (_suppressVolumeAll.to() != 1.) _suppressVolumeAll.start(1.);
				_suppressVolumeAll.update(1. - ((_suppressAllEnd - ms) / double(kFadeDuration)), anim::linear);
			} else if (ms >= _suppressAllStart + st::mediaPlayerSuppressDuration) {
				if (_suppressAllAnim) {
					_suppressVolumeAll.finish();
					_suppressAllAnim = false;
				}
			} else if (ms > _suppressAllStart) {
				_suppressVolumeAll.update((ms - _suppressAllStart) / double(st::mediaPlayerSuppressDuration),
				                          anim::linear);
			}
			auto wasVolumeMultiplierAll = VolumeMultiplierAll;
			VolumeMultiplierAll = _suppressVolumeAll.current();
			volumeChangedAll = (VolumeMultiplierAll != wasVolumeMultiplierAll);
		}
		if (_suppressSongAnim) {
			if (ms >= _suppressSongStart + kFadeDuration) {
				_suppressVolumeSong.finish();
				_suppressSongAnim = false;
			} else {
				_suppressVolumeSong.update((ms - _suppressSongStart) / double(kFadeDuration), anim::linear);
			}
		}
		auto wasVolumeMultiplierSong = VolumeMultiplierSong;
		VolumeMultiplierSong = _suppressVolumeSong.current();
		accumulate_min(VolumeMultiplierSong, VolumeMultiplierAll);
		volumeChangedSong = (VolumeMultiplierSong != wasVolumeMultiplierSong);
	}
	auto hasFading = (_suppressAll || _suppressSongAnim);
	auto hasPlaying = false;

	auto updatePlayback = [this, &hasPlaying, &hasFading](AudioMsgId::Type type, int index, double volumeMultiplier,
	                                                      bool suppressGainChanged) {
		auto track = mixer()->trackForType(type, index);
		if (IsStopped(track->state.state) || track->state.state == State::Paused || !track->isStreamCreated()) return;

		auto emitSignals = updateOnePlayback(track, hasPlaying, hasFading, volumeMultiplier, suppressGainChanged);
		if (emitSignals & EmitError) emit error(track->state.id);
		if (emitSignals & EmitStopped) emit audioStopped(track->state.id);
		if (emitSignals & EmitPositionUpdated) emit playPositionUpdated(track->state.id);
		if (emitSignals & EmitNeedToPreload) emit needToPreload(track->state.id);
	};
	auto suppressGainForMusic = ComputeVolume(AudioMsgId::Type::Song);
	auto suppressGainForMusicChanged = volumeChangedSong || _volumeChangedSong;
	for (auto i = 0; i != kTogetherLimit; ++i) {
		updatePlayback(AudioMsgId::Type::Voice, i, VolumeMultiplierAll, volumeChangedAll);
		updatePlayback(AudioMsgId::Type::Song, i, suppressGainForMusic, suppressGainForMusicChanged);
	}
	auto suppressGainForVideo = ComputeVolume(AudioMsgId::Type::Video);
	auto suppressGainForVideoChanged = volumeChangedAll || _volumeChangedVideo;
	updatePlayback(AudioMsgId::Type::Video, 0, suppressGainForVideo, suppressGainForVideoChanged);

	_volumeChangedSong = _volumeChangedVideo = false;

	if (hasFading) {
		_timer.start(kCheckFadingTimeout);
		Audio::StopDetachIfNotUsedSafe();
	} else if (hasPlaying) {
		_timer.start(kCheckPlaybackPositionTimeout);
		Audio::StopDetachIfNotUsedSafe();
	} else {
		Audio::ScheduleDetachIfNotUsedSafe();
	}
}

qint32 Fader::updateOnePlayback(Mixer::Track *track, bool &hasPlaying, bool &hasFading, double volumeMultiplier,
                                bool volumeChanged) {
	auto playing = false;
	auto fading = false;

	auto errorHappened = [this, track] {
		if (Audio::PlaybackErrorHappened()) {
			setStoppedState(track, State::StoppedAtError);
			return true;
		}
		return false;
	};

	ALint positionInBuffered = 0;
	ALint state = AL_INITIAL;
	alGetSourcei(track->stream.source, AL_SAMPLE_OFFSET, &positionInBuffered);
	alGetSourcei(track->stream.source, AL_SOURCE_STATE, &state);
	if (errorHappened()) return EmitError;

	qint32 emitSignals = 0;

	if (state == AL_STOPPED && positionInBuffered == 0 && !internal::CheckAudioDeviceConnected()) {
		return emitSignals;
	}

	switch (track->state.state) {
	case State::Stopping:
	case State::Pausing:
	case State::Starting:
	case State::Resuming: {
		fading = true;
	} break;
	case State::Playing: {
		playing = true;
	} break;
	}

	auto fullPosition = track->bufferedPosition + positionInBuffered;
	if (state != AL_PLAYING && !track->loading) {
		if (fading || playing) {
			fading = false;
			playing = false;
			if (track->state.state == State::Pausing) {
				setStoppedState(track, State::PausedAtEnd);
			} else if (track->state.state == State::Stopping) {
				setStoppedState(track, State::Stopped);
			} else {
				setStoppedState(track, State::StoppedAtEnd);
			}
			if (errorHappened()) return EmitError;
			emitSignals |= EmitStopped;
		}
	} else if (fading && state == AL_PLAYING) {
		auto fadingForSamplesCount = (fullPosition - track->fadeStartPosition);
		if (TimeMs(1000) * fadingForSamplesCount >= kFadeDuration * track->state.frequency) {
			fading = false;
			alSourcef(track->stream.source, AL_GAIN, 1. * volumeMultiplier);
			if (errorHappened()) return EmitError;

			switch (track->state.state) {
			case State::Stopping: {
				setStoppedState(track);
				state = AL_STOPPED;
			} break;
			case State::Pausing: {
				alSourcePause(track->stream.source);
				if (errorHappened()) return EmitError;

				track->state.state = State::Paused;
			} break;
			case State::Starting:
			case State::Resuming: {
				track->state.state = State::Playing;
				playing = true;
			} break;
			}
		} else {
			auto newGain = TimeMs(1000) * fadingForSamplesCount / double(kFadeDuration * track->state.frequency);
			if (track->state.state == State::Pausing || track->state.state == State::Stopping) {
				newGain = 1. - newGain;
			}
			alSourcef(track->stream.source, AL_GAIN, newGain * volumeMultiplier);
			if (errorHappened()) return EmitError;
		}
	} else if (playing && state == AL_PLAYING) {
		if (volumeChanged) {
			alSourcef(track->stream.source, AL_GAIN, 1. * volumeMultiplier);
			if (errorHappened()) return EmitError;
		}
	}
	if (state == AL_PLAYING && fullPosition >= track->state.position + kCheckPlaybackPositionDelta) {
		track->state.position = fullPosition;
		emitSignals |= EmitPositionUpdated;
	}
	if (playing || track->state.state == State::Starting || track->state.state == State::Resuming) {
		if (!track->loaded && !track->loading) {
			auto needPreload =
			    (track->state.position + kPreloadSamples > track->bufferedPosition + track->bufferedLength);
			if (needPreload) {
				track->loading = true;
				emitSignals |= EmitNeedToPreload;
			}
		}
	}
	if (playing) hasPlaying = true;
	if (fading) hasFading = true;

	return emitSignals;
}

void Fader::setStoppedState(Mixer::Track *track, State state) {
	mixer()->setStoppedState(track, state);
}

void Fader::onSuppressSong() {
	if (!_suppressSong) {
		_suppressSong = true;
		_suppressSongAnim = true;
		_suppressSongStart = getms();
		_suppressVolumeSong.start(kSuppressRatioSong);
		onTimer();
	}
}

void Fader::onUnsuppressSong() {
	if (_suppressSong) {
		_suppressSong = false;
		_suppressSongAnim = true;
		_suppressSongStart = getms();
		_suppressVolumeSong.start(1.);
		onTimer();
	}
}

void Fader::onSuppressAll(qint64 duration) {
	_suppressAll = true;
	auto now = getms();
	if (_suppressAllEnd < now + kFadeDuration) {
		_suppressAllStart = now;
	}
	_suppressAllEnd = now + duration;
	_suppressVolumeAll.start(kSuppressRatioAll);
	onTimer();
}

void Fader::onSongVolumeChanged() {
	_volumeChangedSong = true;
	onTimer();
}

void Fader::onVideoVolumeChanged() {
	_volumeChangedVideo = true;
	onTimer();
}

namespace internal {

// Thread: Any.
QMutex *audioPlayerMutex() {
	return &AudioMutex;
}

// Thread: Any.
bool audioCheckError() {
	return !Audio::PlaybackErrorHappened();
}

// Thread: Any. Must be locked: AudioMutex.
bool audioDeviceIsConnected() {
	if (!AudioDevice) {
		return false;
	}
	auto isConnected = ALint(0);
	alcGetIntegerv(AudioDevice, ALC_CONNECTED, 1, &isConnected);
	if (Audio::ContextErrorHappened()) {
		return false;
	}
	return (isConnected != 0);
}

// Thread: Any. Must be locked: AudioMutex.
bool CheckAudioDeviceConnected() {
	if (audioDeviceIsConnected()) {
		return true;
	}
	Audio::ScheduleDetachFromDeviceSafe();
	return false;
}

// Thread: Main. Locks: AudioMutex.
void DetachFromDevice() {
	QMutexLocker lock(&AudioMutex);
	Audio::ClosePlaybackDevice();
	if (mixer()) {
		mixer()->reattachIfNeeded();
	}
}

} // namespace internal

} // namespace Player
} // namespace Media

class FFMpegAttributesReader : public AbstractFFMpegLoader {
public:
	FFMpegAttributesReader(const FileLocation &file, const QByteArray &data)
	    : AbstractFFMpegLoader(file, data, base::byte_vector()) {}

	bool open(qint64 &position) override {
		if (!AbstractFFMpegLoader::open(position)) {
			return false;
		}

		char err[AV_ERROR_MAX_STRING_SIZE] = {0};

		int videoStreamId = av_find_best_stream(fmtContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
		if (videoStreamId >= 0) {
			DEBUG_LOG(("Audio Read Error: Found video stream in file '%1', data size '%2', error %3, %4")
			              .arg(_file.name())
			              .arg(_data.size())
			              .arg(videoStreamId)
			              .arg(av_make_error_string(err, sizeof(err), streamId)));
			return false;
		}

		for (qint32 i = 0, l = fmtContext->nb_streams; i < l; ++i) {
			AVStream *stream = fmtContext->streams[i];
			if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
				const AVPacket &packet(stream->attached_pic);
				if (packet.size) {
					bool animated = false;
					QByteArray cover((const char *)packet.data, packet.size), format;
					_cover = App::readImage(cover, &format, true, &animated);
					if (!_cover.isNull()) {
						_coverBytes = cover;
						_coverFormat = format;
						break;
					}
				}
			}
		}

		extractMetaData(fmtContext->streams[streamId]->metadata);
		extractMetaData(fmtContext->metadata);

		return true;
	}

	void trySet(QString &to, AVDictionary *dict, const char *key) {
		if (!to.isEmpty()) return;
		if (AVDictionaryEntry *tag = av_dict_get(dict, key, 0, 0)) {
			to = QString::fromUtf8(tag->value);
		}
	}
	void extractMetaData(AVDictionary *dict) {
		trySet(_title, dict, "title");
		trySet(_performer, dict, "artist");
		trySet(_performer, dict, "performer");
		trySet(_performer, dict, "album_artist");
		// for (AVDictionaryEntry *tag = av_dict_get(dict, "", 0, AV_DICT_IGNORE_SUFFIX); tag;
		//      tag = av_dict_get(dict, "", tag, AV_DICT_IGNORE_SUFFIX)) {
		// 	const char *key = tag->key;
		// 	const char *value = tag->value;
		// 	QString tmp = QString::fromUtf8(value);
		// }
	}

	qint32 format() override {
		return 0;
	}

	QString title() {
		return _title;
	}

	QString performer() {
		return _performer;
	}

	QImage cover() {
		return _cover;
	}

	QByteArray coverBytes() {
		return _coverBytes;
	}

	QByteArray coverFormat() {
		return _coverFormat;
	}

	ReadResult readMore(QByteArray &result, qint64 &samplesAdded) override {
		DEBUG_LOG(("Audio Read Error: should not call this"));
		return ReadResult::Error;
	}

	~FFMpegAttributesReader() {}

private:
	QString _title, _performer;
	QImage _cover;
	QByteArray _coverBytes, _coverFormat;
};

namespace Media {
namespace Player {

FileLoadTask::Song PrepareForSending(const QString &fname, const QByteArray &data) {
	auto result = FileLoadTask::Song();
	FFMpegAttributesReader reader(FileLocation(fname), data);
	qint64 position = 0;
	if (reader.open(position) && reader.samplesCount() > 0) {
		result.duration = reader.samplesCount() / reader.samplesFrequency();
		result.title = reader.title();
		result.performer = reader.performer();
		result.cover = reader.cover();
	}
	return result;
}

} // namespace Player
} // namespace Media

class FFMpegWaveformCounter : public FFMpegLoader {
public:
	FFMpegWaveformCounter(const FileLocation &file, const QByteArray &data)
	    : FFMpegLoader(file, data, base::byte_vector()) {}

	bool open(qint64 &position) override {
		if (!FFMpegLoader::open(position)) {
			return false;
		}

		QByteArray buffer;
		buffer.reserve(AudioVoiceMsgBufferSize);
		qint64 countbytes = sampleSize * samplesCount(), processed = 0, sumbytes = 0;
		if (samplesCount() < Media::Player::kWaveformSamplesCount) {
			return false;
		}

		QVector<quint16> peaks;
		peaks.reserve(Media::Player::kWaveformSamplesCount);

		auto fmt = format();
		auto peak = quint16(0);
		auto callback = [&peak, &sumbytes, &peaks, countbytes](quint16 sample) {
			accumulate_max(peak, sample);
			sumbytes += Media::Player::kWaveformSamplesCount;
			if (sumbytes >= countbytes) {
				sumbytes -= countbytes;
				peaks.push_back(peak);
				peak = 0;
			}
		};
		while (processed < countbytes) {
			buffer.resize(0);

			qint64 samples = 0;
			auto res = readMore(buffer, samples);
			if (res == ReadResult::Error || res == ReadResult::EndOfFile) {
				break;
			}
			if (buffer.isEmpty()) {
				continue;
			}

			auto sampleBytes = gsl::as_bytes(gsl::make_span(buffer));
			if (fmt == AL_FORMAT_MONO8 || fmt == AL_FORMAT_STEREO8) {
				Media::Audio::IterateSamples<uchar>(sampleBytes, callback);
			} else if (fmt == AL_FORMAT_MONO16 || fmt == AL_FORMAT_STEREO16) {
				Media::Audio::IterateSamples<qint16>(sampleBytes, callback);
			}
			processed += sampleSize * samples;
		}
		if (sumbytes > 0 && peaks.size() < Media::Player::kWaveformSamplesCount) {
			peaks.push_back(peak);
		}

		if (peaks.isEmpty()) {
			return false;
		}

		auto sum = std::accumulate(peaks.cbegin(), peaks.cend(), 0LL);
		peak = std::max(qint32(sum * 1.8 / peaks.size()), 2500);

		result.resize(peaks.size());
		for (qint32 i = 0, l = peaks.size(); i != l; ++i) {
			result[i] = char(std::min(31U, quint32(std::min(peaks.at(i), peak)) * 31 / peak));
		}

		return true;
	}

	const VoiceWaveform &waveform() const {
		return result;
	}

	~FFMpegWaveformCounter() {}

private:
	VoiceWaveform result;
};

VoiceWaveform audioCountWaveform(const FileLocation &file, const QByteArray &data) {
	FFMpegWaveformCounter counter(file, data);
	qint64 position = 0;
	if (counter.open(position)) {
		return counter.waveform();
	}
	return VoiceWaveform();
}
