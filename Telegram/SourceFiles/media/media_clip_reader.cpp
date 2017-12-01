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
#include "media/media_clip_reader.h"

#include "storage/file_download.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include "media/media_clip_ffmpeg.h"
#include "media/media_clip_qtgif.h"
#include "mainwidget.h"
#include "mainwindow.h"

namespace Media {
namespace Clip {
namespace {

QVector<QThread*> threads;
QVector<Manager*> managers;

QImage PrepareFrameImage(const FrameRequest &request, const QImage &original, bool hasAlpha, QImage &cache) {
	auto needResize = (original.width() != request.framew) || (original.height() != request.frameh);
	auto needOuterFill = (request.outerw != request.framew) || (request.outerh != request.frameh);
	auto needRounding = (request.radius != ImageRoundRadius::None);
	if (!needResize && !needOuterFill && !hasAlpha && !needRounding) {
		return original;
	}

	auto factor = request.factor;
	auto needNewCache = (cache.width() != request.outerw || cache.height() != request.outerh);
	if (needNewCache) {
		cache = QImage(request.outerw, request.outerh, QImage::Format_ARGB32_Premultiplied);
		cache.setDevicePixelRatio(factor);
	}
	{
		Painter p(&cache);
		if (needNewCache) {
			if (request.framew < request.outerw) {
				p.fillRect(0, 0, (request.outerw - request.framew) / (2 * factor), cache.height() / factor, st::imageBg);
				p.fillRect((request.outerw - request.framew) / (2 * factor) + (request.framew / factor), 0, (cache.width() / factor) - ((request.outerw - request.framew) / (2 * factor) + (request.framew / factor)), cache.height() / factor, st::imageBg);
			}
			if (request.frameh < request.outerh) {
				p.fillRect(qMax(0, (request.outerw - request.framew) / (2 * factor)), 0, qMin(cache.width(), request.framew) / factor, (request.outerh - request.frameh) / (2 * factor), st::imageBg);
				p.fillRect(qMax(0, (request.outerw - request.framew) / (2 * factor)), (request.outerh - request.frameh) / (2 * factor) + (request.frameh / factor), qMin(cache.width(), request.framew) / factor, (cache.height() / factor) - ((request.outerh - request.frameh) / (2 * factor) + (request.frameh / factor)), st::imageBg);
			}
		}
		if (hasAlpha) {
			p.fillRect(qMax(0, (request.outerw - request.framew) / (2 * factor)), qMax(0, (request.outerh - request.frameh) / (2 * factor)), qMin(cache.width(), request.framew) / factor, qMin(cache.height(), request.frameh) / factor, st::imageBgTransparent);
		}
		auto position = QPoint((request.outerw - request.framew) / (2 * factor), (request.outerh - request.frameh) / (2 * factor));
		if (needResize) {
			PainterHighQualityEnabler hq(p);

			auto dst = QRect(position, QSize(request.framew / factor, request.frameh / factor));
			auto src = QRect(0, 0, original.width(), original.height());
			p.drawImage(dst, original, src, Qt::ColorOnly);
		} else {
			p.drawImage(position, original);
		}
	}
	if (needRounding) {
		Images::prepareRound(cache, request.radius, request.corners);
	}
	return cache;
}

QPixmap PrepareFrame(const FrameRequest &request, const QImage &original, bool hasAlpha, QImage &cache) {
	return QPixmap::fromImage(PrepareFrameImage(request, original, hasAlpha, cache), Qt::ColorOnly);
}

} // namespace

Reader::Reader(const QString &filepath, Callback &&callback, Mode mode, int64_t seekMs)
: _callback(std::move(callback))
, _mode(mode)
, _seekPositionMs(seekMs) {
	init(FileLocation(filepath), QByteArray());
}

Reader::Reader(not_null<DocumentData*> document, FullMsgId msgId, Callback &&callback, Mode mode, int64_t seekMs)
: _callback(std::move(callback))
, _mode(mode)
, _audioMsgId(document, msgId, (mode == Mode::Video) ? rand_value<uint32_t>() : 0)
, _seekPositionMs(seekMs) {
	init(document->location(), document->data());
}

void Reader::init(const FileLocation &location, const QByteArray &data) {
	if (threads.size() < ClipThreadsCount) {
		_threadIndex = threads.size();
		threads.push_back(new QThread());
		managers.push_back(new Manager(threads.back()));
		threads.back()->start();
	} else {
		_threadIndex = int32_t(rand_value<uint32_t>() % threads.size());
		int32_t loadLevel = 0x7FFFFFFF;
		for (int32_t i = 0, l = threads.size(); i < l; ++i) {
			int32_t level = managers.at(i)->loadLevel();
			if (level < loadLevel) {
				_threadIndex = i;
				loadLevel = level;
			}
		}
	}
	managers.at(_threadIndex)->append(this, location, data);
}

Reader::Frame *Reader::frameToShow(int32_t *index) const { // 0 means not ready
	int step = _step.loadAcquire(), i;
	if (step == WaitingForDimensionsStep) {
		if (index) *index = 0;
		return nullptr;
	} else if (step == WaitingForRequestStep) {
		i = 0;
	} else if (step == WaitingForFirstFrameStep) {
		i = 0;
	} else {
		i = (step / 2) % 3;
	}
	if (index) *index = i;
	return _frames + i;
}

Reader::Frame *Reader::frameToWrite(int32_t *index) const { // 0 means not ready
	int32_t step = _step.loadAcquire(), i;
	if (step == WaitingForDimensionsStep) {
		i = 0;
	} else if (step == WaitingForRequestStep) {
		if (index) *index = 0;
		return nullptr;
	} else if (step == WaitingForFirstFrameStep) {
		i = 0;
	} else {
		i = ((step + 2) / 2) % 3;
	}
	if (index) *index = i;
	return _frames + i;
}

Reader::Frame *Reader::frameToWriteNext(bool checkNotWriting, int32_t *index) const {
	int32_t step = _step.loadAcquire(), i;
	if (step == WaitingForDimensionsStep || step == WaitingForRequestStep || (checkNotWriting && (step % 2))) {
		if (index) *index = 0;
		return nullptr;
	}
	i = ((step + 4) / 2) % 3;
	if (index) *index = i;
	return _frames + i;
}

void Reader::moveToNextShow() const {
	int32_t step = _step.loadAcquire();
	if (step == WaitingForDimensionsStep) {
	} else if (step == WaitingForRequestStep) {
		_step.storeRelease(WaitingForFirstFrameStep);
	} else if (step == WaitingForFirstFrameStep) {
	} else if (!(step % 2)) {
		_step.storeRelease(step + 1);
	}
}

void Reader::moveToNextWrite() const {
	int32_t step = _step.loadAcquire();
	if (step == WaitingForDimensionsStep) {
		_step.storeRelease(WaitingForRequestStep);
	} else if (step == WaitingForRequestStep) {
	} else if (step == WaitingForFirstFrameStep) {
		_step.storeRelease(0);

		// Force paint the first frame so moveToNextShow() is called.
		_frames[0].displayed.storeRelease(0);
	} else if (step % 2) {
		_step.storeRelease((step + 1) % 6);
	}
}

void Reader::callback(Reader *reader, int32_t threadIndex, Notification notification) {
	// check if reader is not deleted already
	if (managers.size() > threadIndex && managers.at(threadIndex)->carries(reader) && reader->_callback) {
		reader->_callback(notification);
	}
}

void Reader::start(int32_t framew, int32_t frameh, int32_t outerw, int32_t outerh, ImageRoundRadius radius, ImageRoundCorners corners) {
	if (managers.size() <= _threadIndex) error();
	if (_state == State::Error) return;

	if (_step.loadAcquire() == WaitingForRequestStep) {
		int factor = cIntRetinaFactor();
		FrameRequest request;
		request.factor = factor;
		request.framew = framew * factor;
		request.frameh = frameh * factor;
		request.outerw = outerw * factor;
		request.outerh = outerh * factor;
		request.radius = radius;
		request.corners = corners;
		_frames[0].request = _frames[1].request = _frames[2].request = request;
		moveToNextShow();
		managers.at(_threadIndex)->start(this);
	}
}

QPixmap Reader::current(int32_t framew, int32_t frameh, int32_t outerw, int32_t outerh, ImageRoundRadius radius, ImageRoundCorners corners, TimeMs ms) {
	Expects(outerw > 0);
	Expects(outerh > 0);

	auto frame = frameToShow();
	Assert(frame != nullptr);

	auto shouldBePaused = !ms;
	if (!shouldBePaused) {
		frame->displayed.storeRelease(1);
		if (_autoPausedGif.loadAcquire()) {
			_autoPausedGif.storeRelease(0);
			if (managers.size() <= _threadIndex) error();
			if (_state != State::Error) {
				managers.at(_threadIndex)->update(this);
			}
		}
	} else {
		frame->displayed.storeRelease(-1);
	}

	auto factor = cIntRetinaFactor();
	if (frame->pix.width() == outerw * factor
		&& frame->pix.height() == outerh * factor
		&& frame->request.radius == radius
		&& frame->request.corners == corners) {
		moveToNextShow();
		return frame->pix;
	}

	frame->request.framew = framew * factor;
	frame->request.frameh = frameh * factor;
	frame->request.outerw = outerw * factor;
	frame->request.outerh = outerh * factor;

	QImage cacheForResize;
	frame->original.setDevicePixelRatio(factor);
	frame->pix = QPixmap();
	frame->pix = PrepareFrame(frame->request, frame->original, true, cacheForResize);

	auto other = frameToWriteNext(true);
	if (other) other->request = frame->request;

	moveToNextShow();

	if (managers.size() <= _threadIndex) error();
	if (_state != State::Error) {
		managers.at(_threadIndex)->update(this);
	}

	return frame->pix;
}

QPixmap Reader::current() {
	Expects(_mode == Mode::Video);

	auto frame = frameToShow();
	Assert(frame != nullptr);

	frame->displayed.storeRelease(1);
	moveToNextShow();
	return frame->pix;
}

bool Reader::ready() const {
	if (_width && _height) return true;

	auto frame = frameToShow();
	if (frame) {
		_width = frame->original.width();
		_height = frame->original.height();
		return true;
	}
	return false;
}

bool Reader::hasAudio() const {
	return ready() ? _hasAudio : false;
}

TimeMs Reader::getPositionMs() const {
	if (auto frame = frameToShow()) {
		return frame->positionMs;
	}
	return _seekPositionMs;
}

TimeMs Reader::getDurationMs() const {
	return ready() ? _durationMs : 0;
}

void Reader::pauseResumeVideo() {
	if (managers.size() <= _threadIndex) error();
	if (_state == State::Error) return;

	_videoPauseRequest.storeRelease(1 - _videoPauseRequest.loadAcquire());
	managers.at(_threadIndex)->start(this);
}

bool Reader::videoPaused() const {
	return _videoPauseRequest.loadAcquire() != 0;
}

int32_t Reader::width() const {
	return _width;
}

int32_t Reader::height() const {
	return _height;
}

State Reader::state() const {
	return _state;
}

void Reader::stop() {
	if (managers.size() <= _threadIndex) error();
	if (_state != State::Error) {
		managers.at(_threadIndex)->stop(this);
		_width = _height = 0;
	}
}

void Reader::error() {
	_state = State::Error;
	_private = nullptr;
}

void Reader::finished() {
	_state = State::Finished;
	_private = nullptr;
}

Reader::~Reader() {
	stop();
}

class ReaderPrivate {
public:
	ReaderPrivate(Reader *reader, const FileLocation &location, const QByteArray &data) : _interface(reader)
	, _mode(reader->mode())
	, _audioMsgId(reader->audioMsgId())
	, _seekPositionMs(reader->seekPositionMs())
	, _data(data) {
		if (_data.isEmpty()) {
			_location = std::make_unique<FileLocation>(location);
			if (!_location->accessEnable()) {
				error();
				return;
			}
		}
		_accessed = true;
	}

	ProcessResult start(TimeMs ms) {
		if (!_implementation && !init()) {
			return error();
		}
		if (frame() && frame()->original.isNull()) {
			auto readResult = _implementation->readFramesTill(-1, ms);
			if (readResult == internal::ReaderImplementation::ReadResult::EndOfFile && _seekPositionMs > 0) {
				// If seek was done to the end: try to read the first frame,
				// get the frame size and return a black frame with that size.

				auto firstFramePositionMs = TimeMs(0);
				auto reader = std::make_unique<internal::FFMpegReaderImplementation>(_location.get(), &_data, AudioMsgId());
				if (reader->start(internal::ReaderImplementation::Mode::Normal, firstFramePositionMs)) {
					auto firstFrameReadResult = reader->readFramesTill(-1, ms);
					if (firstFrameReadResult == internal::ReaderImplementation::ReadResult::Success) {
						if (reader->renderFrame(frame()->original, frame()->alpha, QSize())) {
							frame()->original.fill(QColor(0, 0, 0));

							frame()->positionMs = _seekPositionMs;

							_width = frame()->original.width();
							_height = frame()->original.height();
							_durationMs = _implementation->durationMs();
							_hasAudio = _implementation->hasAudio();
							return ProcessResult::Started;
						}
					}
				}

				return error();
			} else if (readResult != internal::ReaderImplementation::ReadResult::Success) { // Read the first frame.
				return error();
			}
			if (!_implementation->renderFrame(frame()->original, frame()->alpha, QSize())) {
				return error();
			}
			frame()->positionMs = _implementation->frameRealTime();

			_width = frame()->original.width();
			_height = frame()->original.height();
			_durationMs = _implementation->durationMs();
			_hasAudio = _implementation->hasAudio();
			return ProcessResult::Started;
		}
		return ProcessResult::Wait;
	}

	ProcessResult process(TimeMs ms) { // -1 - do nothing, 0 - update, 1 - reinit
		if (_state == State::Error) {
			return ProcessResult::Error;
		} else if (_state == State::Finished) {
			return ProcessResult::Finished;
		}

		if (!_request.valid()) {
			return start(ms);
		}
		if (!_started) {
			_started = true;
			if (!_videoPausedAtMs && _hasAudio) {
				Player::mixer()->resume(_audioMsgId, true);
			}
		}

		if (!_autoPausedGif && !_videoPausedAtMs && ms >= _nextFrameWhen) {
			return ProcessResult::Repaint;
		}
		return ProcessResult::Wait;
	}

	ProcessResult finishProcess(TimeMs ms) {
		auto frameMs = _seekPositionMs + ms - _animationStarted;
		auto readResult = _implementation->readFramesTill(frameMs, ms);
		if (readResult == internal::ReaderImplementation::ReadResult::EndOfFile) {
			stop(Player::State::StoppedAtEnd);
			_state = State::Finished;
			return ProcessResult::Finished;
		} else if (readResult == internal::ReaderImplementation::ReadResult::Error) {
			return error();
		}
		_nextFramePositionMs = _implementation->frameRealTime();
		_nextFrameWhen = _animationStarted + _implementation->framePresentationTime();
		if (_nextFrameWhen > _seekPositionMs) {
			_nextFrameWhen -= _seekPositionMs;
		} else {
			_nextFrameWhen = 1;
		}

		if (!renderFrame()) {
			return error();
		}
		return ProcessResult::CopyFrame;
	}

	bool renderFrame() {
		Assert(frame() != 0 && _request.valid());
		if (!_implementation->renderFrame(frame()->original, frame()->alpha, QSize(_request.framew, _request.frameh))) {
			return false;
		}
		frame()->original.setDevicePixelRatio(_request.factor);
		frame()->pix = QPixmap();
		frame()->pix = PrepareFrame(_request, frame()->original, frame()->alpha, frame()->cache);
		frame()->when = _nextFrameWhen;
		frame()->positionMs = _nextFramePositionMs;
		return true;
	}

	bool init() {
		if (_data.isEmpty() && QFileInfo(_location->name()).size() <= Storage::kMaxAnimationInMemory) {
			QFile f(_location->name());
			if (f.open(QIODevice::ReadOnly)) {
				_data = f.readAll();
				if (f.error() != QFile::NoError) {
					_data = QByteArray();
				}
			}
		}

		_implementation = std::make_unique<internal::FFMpegReaderImplementation>(_location.get(), &_data, _audioMsgId);
//		_implementation = new QtGifReaderImplementation(_location, &_data);

		auto implementationMode = [this]() {
			using ImplementationMode = internal::ReaderImplementation::Mode;
			if (_mode == Reader::Mode::Gif) {
				return ImplementationMode::Silent;
			}
			return ImplementationMode::Normal;
		};
		return _implementation->start(implementationMode(), _seekPositionMs);
	}

	void startedAt(TimeMs ms) {
		_animationStarted = _nextFrameWhen = ms;
	}

	void pauseVideo(TimeMs ms) {
		if (_videoPausedAtMs) return; // Paused already.

		_videoPausedAtMs = ms;
		if (_hasAudio) {
			Player::mixer()->pause(_audioMsgId, true);
		}
	}

	void resumeVideo(TimeMs ms) {
		if (!_videoPausedAtMs) return; // Not paused.

		auto delta = ms - _videoPausedAtMs;
		_animationStarted += delta;
		_nextFrameWhen += delta;

		_videoPausedAtMs = 0;
		if (_hasAudio) {
			Player::mixer()->resume(_audioMsgId, true);
		}
	}

	ProcessResult error() {
		stop(Player::State::StoppedAtError);
		_state = State::Error;
		return ProcessResult::Error;
	}

	void stop(Player::State audioState) {
		_implementation = nullptr;
		if (_hasAudio) {
			Player::mixer()->stop(_audioMsgId, audioState);
		}

		if (_location) {
			if (_accessed) {
				_location->accessDisable();
			}
			_location = nullptr;
		}
		_accessed = false;
	}

	~ReaderPrivate() {
		stop(Player::State::Stopped);
		_data.clear();
	}

private:
	Reader *_interface;
	State _state = State::Reading;
	Reader::Mode _mode;
	AudioMsgId _audioMsgId;
	TimeMs _seekPositionMs = 0;

	QByteArray _data;
	std::unique_ptr<FileLocation> _location;
	bool _accessed = false;

	QBuffer _buffer;
	std::unique_ptr<internal::ReaderImplementation> _implementation;

	FrameRequest _request;
	struct Frame {
		QPixmap pix;
		QImage original, cache;
		bool alpha = true;
		TimeMs when = 0;

		// Counted from the end, so that positionMs <= durationMs despite keep up delays.
		TimeMs positionMs = 0;
	};
	Frame _frames[3];
	int _frame = 0;
	Frame *frame() {
		return _frames + _frame;
	}

	int _width = 0;
	int _height = 0;

	bool _hasAudio = false;
	TimeMs _durationMs = 0;
	TimeMs _animationStarted = 0;
	TimeMs _nextFrameWhen = 0;
	TimeMs _nextFramePositionMs = 0;

	bool _autoPausedGif = false;
	bool _started = false;
	TimeMs _videoPausedAtMs = 0;

	friend class Manager;

};

Manager::Manager(QThread *thread) : _processingInThread(0), _needReProcess(false) {
	moveToThread(thread);
	connect(thread, SIGNAL(started()), this, SLOT(process()));
	connect(thread, SIGNAL(finished()), this, SLOT(finish()));
	connect(this, SIGNAL(processDelayed()), this, SLOT(process()), Qt::QueuedConnection);

	_timer.setSingleShot(true);
	_timer.moveToThread(thread);
	connect(&_timer, SIGNAL(timeout()), this, SLOT(process()));

	anim::registerClipManager(this);
}

void Manager::append(Reader *reader, const FileLocation &location, const QByteArray &data) {
	reader->_private = new ReaderPrivate(reader, location, data);
	_loadLevel.fetchAndAddRelaxed(AverageGifSize);
	update(reader);
}

void Manager::start(Reader *reader) {
	update(reader);
}

void Manager::update(Reader *reader) {
	QMutexLocker lock(&_readerPointersMutex);
	auto i = _readerPointers.find(reader);
	if (i == _readerPointers.cend()) {
		_readerPointers.insert(reader, QAtomicInt(1));
	} else {
		i->storeRelease(1);
	}
	emit processDelayed();
}

void Manager::stop(Reader *reader) {
	if (!carries(reader)) return;

	QMutexLocker lock(&_readerPointersMutex);
	_readerPointers.remove(reader);
	emit processDelayed();
}

bool Manager::carries(Reader *reader) const {
	QMutexLocker lock(&_readerPointersMutex);
	return _readerPointers.contains(reader);
}

Manager::ReaderPointers::iterator Manager::unsafeFindReaderPointer(ReaderPrivate *reader) {
	ReaderPointers::iterator it = _readerPointers.find(reader->_interface);

	// could be a new reader which was realloced in the same address
	return (it == _readerPointers.cend() || it.key()->_private == reader) ? it : _readerPointers.end();
}

Manager::ReaderPointers::const_iterator Manager::constUnsafeFindReaderPointer(ReaderPrivate *reader) const {
	ReaderPointers::const_iterator it = _readerPointers.constFind(reader->_interface);

	// could be a new reader which was realloced in the same address
	return (it == _readerPointers.cend() || it.key()->_private == reader) ? it : _readerPointers.cend();
}

bool Manager::handleProcessResult(ReaderPrivate *reader, ProcessResult result, TimeMs ms) {
	QMutexLocker lock(&_readerPointersMutex);
	auto it = unsafeFindReaderPointer(reader);
	if (result == ProcessResult::Error) {
		if (it != _readerPointers.cend()) {
			it.key()->error();
			emit callback(it.key(), it.key()->threadIndex(), NotificationReinit);
			_readerPointers.erase(it);
		}
		return false;
	} else if (result == ProcessResult::Finished) {
		if (it != _readerPointers.cend()) {
			it.key()->finished();
			emit callback(it.key(), it.key()->threadIndex(), NotificationReinit);
		}
		return false;
	}
	if (it == _readerPointers.cend()) {
		return false;
	}

	if (result == ProcessResult::Started) {
		_loadLevel.fetchAndAddRelaxed(reader->_width * reader->_height - AverageGifSize);
		it.key()->_durationMs = reader->_durationMs;
		it.key()->_hasAudio = reader->_hasAudio;
	}
	// See if we need to pause GIF because it is not displayed right now.
	if (!reader->_autoPausedGif && reader->_mode == Reader::Mode::Gif && result == ProcessResult::Repaint) {
		int32_t ishowing, iprevious;
		auto showing = it.key()->frameToShow(&ishowing), previous = it.key()->frameToWriteNext(false, &iprevious);
		Assert(previous != nullptr && showing != nullptr && ishowing >= 0 && iprevious >= 0);
		if (reader->_frames[ishowing].when > 0 && showing->displayed.loadAcquire() <= 0) { // current frame was not shown
			if (reader->_frames[ishowing].when + WaitBeforeGifPause < ms || (reader->_frames[iprevious].when && previous->displayed.loadAcquire() <= 0)) {
				reader->_autoPausedGif = true;
				it.key()->_autoPausedGif.storeRelease(1);
				result = ProcessResult::Paused;
			}
		}
	}
	if (result == ProcessResult::Started || result == ProcessResult::CopyFrame) {
		Assert(reader->_frame >= 0);
		auto frame = it.key()->_frames + reader->_frame;
		frame->clear();
		frame->pix = reader->frame()->pix;
		frame->original = reader->frame()->original;
		frame->displayed.storeRelease(0);
		frame->positionMs = reader->frame()->positionMs;
		if (result == ProcessResult::Started) {
			reader->startedAt(ms);
			it.key()->moveToNextWrite();
			emit callback(it.key(), it.key()->threadIndex(), NotificationReinit);
		}
	} else if (result == ProcessResult::Paused) {
		it.key()->moveToNextWrite();
		emit callback(it.key(), it.key()->threadIndex(), NotificationReinit);
	} else if (result == ProcessResult::Repaint) {
		it.key()->moveToNextWrite();
		emit callback(it.key(), it.key()->threadIndex(), NotificationRepaint);
	}
	return true;
}

Manager::ResultHandleState Manager::handleResult(ReaderPrivate *reader, ProcessResult result, TimeMs ms) {
	if (!handleProcessResult(reader, result, ms)) {
		_loadLevel.fetchAndAddRelaxed(-1 * (reader->_width > 0 ? reader->_width * reader->_height : AverageGifSize));
		delete reader;
		return ResultHandleRemove;
	}

	_processingInThread->eventDispatcher()->processEvents(QEventLoop::AllEvents);
	if (_processingInThread->isInterruptionRequested()) {
		return ResultHandleStop;
	}

	if (result == ProcessResult::Repaint) {
		{
			QMutexLocker lock(&_readerPointersMutex);
			auto it = constUnsafeFindReaderPointer(reader);
			if (it != _readerPointers.cend()) {
				int32_t index = 0;
				Reader *r = it.key();
				Reader::Frame *frame = it.key()->frameToWrite(&index);
				if (frame) {
					frame->clear();
				} else {
					Assert(!reader->_request.valid());
				}
				reader->_frame = index;
			}
		}
		return handleResult(reader, reader->finishProcess(ms), ms);
	}

	return ResultHandleContinue;
}

void Manager::process() {
	if (_processingInThread) {
		_needReProcess = true;
		return;
	}

	_timer.stop();
	_processingInThread = thread();

	bool checkAllReaders = false;
	auto ms = getms(), minms = ms + 86400 * 1000LL;
	{
		QMutexLocker lock(&_readerPointersMutex);
		for (auto it = _readerPointers.begin(), e = _readerPointers.end(); it != e; ++it) {
			if (it->loadAcquire() && it.key()->_private != nullptr) {
				auto i = _readers.find(it.key()->_private);
				if (i == _readers.cend()) {
					_readers.insert(it.key()->_private, 0);
				} else {
					i.value() = ms;
					if (i.key()->_autoPausedGif && !it.key()->_autoPausedGif.loadAcquire()) {
						i.key()->_autoPausedGif = false;
					}
					if (it.key()->_videoPauseRequest.loadAcquire()) {
						i.key()->pauseVideo(ms);
					} else {
						i.key()->resumeVideo(ms);
					}
				}
				auto frame = it.key()->frameToWrite();
				if (frame) it.key()->_private->_request = frame->request;
				it->storeRelease(0);
			}
		}
		checkAllReaders = (_readers.size() > _readerPointers.size());
	}

	for (auto i = _readers.begin(), e = _readers.end(); i != e;) {
		ReaderPrivate *reader = i.key();
		if (i.value() <= ms) {
			ResultHandleState state = handleResult(reader, reader->process(ms), ms);
			if (state == ResultHandleRemove) {
				i = _readers.erase(i);
				continue;
			} else if (state == ResultHandleStop) {
				_processingInThread = 0;
				return;
			}
			ms = getms();
			if (reader->_videoPausedAtMs) {
				i.value() = ms + 86400 * 1000ULL;
			} else if (reader->_nextFrameWhen && reader->_started) {
				i.value() = reader->_nextFrameWhen;
			} else {
				i.value() = (ms + 86400 * 1000ULL);
			}
		} else if (checkAllReaders) {
			QMutexLocker lock(&_readerPointersMutex);
			auto it = constUnsafeFindReaderPointer(reader);
			if (it == _readerPointers.cend()) {
				_loadLevel.fetchAndAddRelaxed(-1 * (reader->_width > 0 ? reader->_width * reader->_height : AverageGifSize));
				delete reader;
				i = _readers.erase(i);
				continue;
			}
		}
		if (!reader->_autoPausedGif && i.value() < minms) {
			minms = i.value();
		}
		++i;
	}

	ms = getms();
	if (_needReProcess || minms <= ms) {
		_needReProcess = false;
		_timer.start(1);
	} else {
		_timer.start(minms - ms);
	}

	_processingInThread = 0;
}

void Manager::finish() {
	_timer.stop();
	clear();
}

void Manager::clear() {
	{
		QMutexLocker lock(&_readerPointersMutex);
		for (auto it = _readerPointers.begin(), e = _readerPointers.end(); it != e; ++it) {
			it.key()->_private = nullptr;
		}
		_readerPointers.clear();
	}

	for (Readers::iterator i = _readers.begin(), e = _readers.end(); i != e; ++i) {
		delete i.key();
	}
	_readers.clear();
}

Manager::~Manager() {
	clear();
}

FileLoadTask::Video PrepareForSending(const QString &fname, const QByteArray &data) {
	auto result = FileLoadTask::Video();
	auto localLocation = FileLocation(fname);
	auto localData = QByteArray(data);

	auto seekPositionMs = 0LL;
	auto reader = std::make_unique<internal::FFMpegReaderImplementation>(&localLocation, &localData, AudioMsgId());
	if (reader->start(internal::ReaderImplementation::Mode::Inspecting, seekPositionMs)) {
		auto durationMs = reader->durationMs();
		if (durationMs > 0) {
			result.isGifv = reader->isGifv();
			if (!result.isGifv) {
				auto middleMs = durationMs / 2;
				if (!reader->inspectAt(middleMs)) {
					return result;
				}
			}
			auto hasAlpha = false;
			auto readResult = reader->readFramesTill(-1, getms());
			auto readFrame = (readResult == internal::ReaderImplementation::ReadResult::Success);
			if (readFrame && reader->renderFrame(result.thumbnail, hasAlpha, QSize())) {
				if (hasAlpha) {
					auto cacheForResize = QImage();
					auto request = FrameRequest();
					request.framew = request.outerw = result.thumbnail.width();
					request.frameh = request.outerh = result.thumbnail.height();
					request.factor = 1;
					result.thumbnail = PrepareFrameImage(request, result.thumbnail, hasAlpha, cacheForResize);
				}
				result.duration = static_cast<int>(durationMs / 1000);
			}
		}
	}
	return result;
}

void Finish() {
	if (!threads.isEmpty()) {
		for (int32_t i = 0, l = threads.size(); i < l; ++i) {
			threads.at(i)->quit();
			DEBUG_LOG(("Waiting for clipThread to finish: %1").arg(i));
			threads.at(i)->wait();
			delete managers.at(i);
			delete threads.at(i);
		}
		threads.clear();
		managers.clear();
	}
}

} // namespace Clip
} // namespace Media
