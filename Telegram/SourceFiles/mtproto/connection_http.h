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

#include "mtproto/connection_abstract.h"

#include <QNetworkAccessManager>
#include <QUrl>

class QNetworkReply;

namespace MTP {
namespace internal {

class HTTPConnection : public AbstractConnection {
	Q_OBJECT

public:
	HTTPConnection(QThread *thread);

	void sendData(mtpBuffer &buffer) override;
	void disconnectFromServer() override;
	void connectTcp(const DcOptions::Endpoint &endpoint) override { // not supported
	}
	void connectHttp(const DcOptions::Endpoint &endpoint) override;
	bool isConnected() const override;
	bool usingHttpWait() override;
	bool needHttpWait() override;

	qint32 debugState() const override;

	QString transport() const override;

	static mtpBuffer handleResponse(QNetworkReply *reply);
	static qint32 handleError(QNetworkReply *reply); // returnes error code

public slots:
	void requestFinished(QNetworkReply *reply);

private:
	enum Status { WaitingHttp = 0, UsingHttp, FinishedWork };
	Status status;
	MTPint128 httpNonce;
	MTPDdcOption::Flags _flags;

	QNetworkAccessManager manager;
	QUrl address;

	typedef QSet<QNetworkReply *> Requests;
	Requests requests;
};

} // namespace internal
} // namespace MTP
