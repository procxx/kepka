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

#include <QAbstractNativeEventFilter>
#include <windows.h>

namespace Platform {

class EventFilter : public QAbstractNativeEventFilter {
public:
	bool nativeEventFilter(const QByteArray &eventType, void *message, long *result);
	bool mainWindowEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *result);

	bool sessionLoggedOff() const {
		return _sessionLoggedOff;
	}
	void setSessionLoggedOff(bool loggedOff) {
		_sessionLoggedOff = loggedOff;
	}

	static EventFilter *createInstance();
	static EventFilter *getInstance();
	static void destroy();

private:
	EventFilter() {}

	bool _sessionLoggedOff = false;
};

} // namespace Platform
