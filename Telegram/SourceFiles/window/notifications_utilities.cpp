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
#include "window/notifications_utilities.h"

#include "messenger.h"
#include "data/data_peer.h"
#include "platform/platform_specific.h"
#include "styles/style_window.h"

namespace Window {
namespace Notifications {
namespace {

// Delete notify photo file after 1 minute of not using.
constexpr int kNotifyDeletePhotoAfterMs = 60000;

} // namespace

CachedUserpics::CachedUserpics(Type type)
    : _type(type) {
	connect(&_clearTimer, SIGNAL(timeout()), this, SLOT(onClear()));
	QDir().mkpath(cWorkingDir() + qsl("tdata/temp"));
}

QString CachedUserpics::get(const StorageKey &key, PeerData *peer) {
	auto ms = getms(true);
	auto i = _images.find(key);
	if (i != _images.cend()) {
		if (i->until) {
			i->until = ms + kNotifyDeletePhotoAfterMs;
			clearInMs(-kNotifyDeletePhotoAfterMs);
		}
	} else {
		Image v;
		if (key.first) {
			v.until = ms + kNotifyDeletePhotoAfterMs;
			clearInMs(-kNotifyDeletePhotoAfterMs);
		} else {
			v.until = 0;
		}
		v.path = cWorkingDir() + qsl("tdata/temp/") + QString::number(rand_value<quint64>(), 16) + qsl(".png");
		if (key.first || key.second) {
			if (_type == Type::Rounded) {
				peer->saveUserpicRounded(v.path, st::notifyMacPhotoSize);
			} else {
				peer->saveUserpic(v.path, st::notifyMacPhotoSize);
			}
		} else {
			Messenger::Instance().logoNoMargin().save(v.path, "PNG");
		}
		i = _images.insert(key, v);
		_someSavedFlag = true;
	}
	return i->path;
}

TimeMs CachedUserpics::clear(TimeMs ms) {
	TimeMs result = 0;
	for (auto i = _images.begin(); i != _images.end();) {
		if (!i->until) {
			++i;
			continue;
		}
		if (i->until <= ms) {
			QFile(i->path).remove();
			i = _images.erase(i);
		} else {
			if (!result) {
				result = i->until;
			} else {
				accumulate_min(result, i->until);
			}
			++i;
		}
	}
	return result;
}

void CachedUserpics::clearInMs(int ms) {
	if (ms < 0) {
		ms = -ms;
		if (_clearTimer.isActive() && _clearTimer.remainingTime() <= ms) {
			return;
		}
	}
	_clearTimer.start(ms);
}

void CachedUserpics::onClear() {
	auto ms = getms(true);
	auto minuntil = clear(ms);
	if (minuntil) {
		clearInMs(int(minuntil - ms));
	}
}

CachedUserpics::~CachedUserpics() {
	if (_someSavedFlag) {
		for_const (auto &item, _images) { QFile(item.path).remove(); }

		// This works about 1200ms on Windows for a folder with one image O_o
		// psDeleteDir(cWorkingDir() + qsl("tdata/temp"));
	}
}

} // namespace Notifications
} // namespace Window
