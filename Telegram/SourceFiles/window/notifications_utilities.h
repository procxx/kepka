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

#include "core/single_timer.h"
#include "window/notifications_manager.h"

namespace Window {
namespace Notifications {

class CachedUserpics : public QObject {
	Q_OBJECT

public:
	enum class Type {
		Rounded,
		Circled,
	};
	CachedUserpics(Type type);

	QString get(const StorageKey &key, PeerData *peer);

	~CachedUserpics();

private slots:
	void onClear();

private:
	void clearInMs(int ms);
	TimeMs clear(TimeMs ms);

	Type _type = Type::Rounded;
	struct Image {
		TimeMs until;
		QString path;
	};
	using Images = QMap<StorageKey, Image>;
	Images _images;
	bool _someSavedFlag = false;
	SingleTimer _clearTimer;
};

} // namespace Notifications
} // namespace Window
