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

#include "stdafx.h"
#include <QApplication>

class Application : public QApplication {
	Q_OBJECT

public:
	Application(int &argc, char **argv);

	bool event(QEvent *e) override;

	void createMessenger();

	~Application();

signals:
	void adjustSingleTimers();

// Single instance application
public slots:
	void socketConnected();
	void socketError(QLocalSocket::LocalSocketError e);
	void socketDisconnected();
	void socketWritten(int64_t bytes);
	void socketReading();
	void newInstanceConnected();

	void readClients();
	void removeClients();

	void startApplication(); // will be done in exec()
	void closeApplication(); // will be done in aboutToQuit()

private:
	typedef QPair<QLocalSocket*, QByteArray> LocalClient;
	typedef QList<LocalClient> LocalClients;

	std::unique_ptr<Messenger> _messengerInstance;

	QString _localServerName, _localSocketReadData;
	QLocalServer _localServer;
	QLocalSocket _localSocket;
	LocalClients _localClients;
	bool _secondInstance = false;

	void singleInstanceChecked();
};

namespace Sandbox {

QRect availableGeometry();
QRect screenGeometry(const QPoint &p);
void setActiveWindow(QWidget *window);
bool isSavingSession();

void execExternal(const QString &cmd);

void adjustSingleTimers();

void connect(const char *signal, QObject *object, const char *method);

void launch();

} // namespace Sandbox
