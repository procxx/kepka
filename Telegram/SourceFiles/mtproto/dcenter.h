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

#include "mtproto/core_types.h"

#include <gsl/gsl>

#include <QMutex>
#include <QMutexLocker>
#include <QReadWriteLock>

namespace MTP {

class Instance;
class AuthKey;
using AuthKeyPtr = std::shared_ptr<AuthKey>;

namespace internal {

class Dcenter : public QObject {
	Q_OBJECT

public:
	Dcenter(gsl::not_null<Instance *> instance, DcId dcId, AuthKeyPtr &&key);

	QReadWriteLock *keyMutex() const;
	const AuthKeyPtr &getKey() const;
	void setKey(AuthKeyPtr &&key);
	void destroyKey();

	bool connectionInited() const {
		QMutexLocker lock(&initLock);
		bool res = _connectionInited;
		return res;
	}
	void setConnectionInited(bool connectionInited = true) {
		QMutexLocker lock(&initLock);
		_connectionInited = connectionInited;
	}

signals:
	void authKeyCreated();
	void layerWasInited(bool was);

private slots:
	void authKeyWrite();

private:
	mutable QReadWriteLock keyLock;
	mutable QMutex initLock;
	gsl::not_null<Instance *> _instance;
	DcId _id = 0;
	AuthKeyPtr _key;
	bool _connectionInited = false;
};

} // namespace internal
} // namespace MTP
