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


#include "mtproto/mtp_instance.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace MTP {

class SpecialConfigRequest : public QObject {
public:
	SpecialConfigRequest(Fn<void(DcId dcId, const std::string &ip, int port)> callback);

	~SpecialConfigRequest();

private:
	void performAppRequest();
	void performDnsRequest();
	void appFinished();
	void dnsFinished();
	void handleResponse(const QByteArray &bytes);
	bool decryptSimpleConfig(const QByteArray &bytes);

	Fn<void(DcId dcId, const std::string &ip, int port)> _callback;
	MTPhelp_ConfigSimple _simpleConfig;

	QNetworkAccessManager _manager;
	std::unique_ptr<QNetworkReply> _appReply;
	std::unique_ptr<QNetworkReply> _dnsReply;

	std::unique_ptr<DcOptions> _localOptions;
	std::unique_ptr<Instance> _localInstance;
};

} // namespace MTP
