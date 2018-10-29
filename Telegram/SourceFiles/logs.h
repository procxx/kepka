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

#include <QString>
#include <QVector>

extern inline bool cDebug();

class MTPlong;
namespace Logs {

void start();
bool started();
void finish();

bool instanceChecked();
void multipleInstances();

void closeMain();

void writeMain(const QString &v);

void writeDebug(const char *file, qint32 line, const QString &v);
void writeTcp(const QString &v);
void writeMtp(qint32 dc, const QString &v);

QString full();

inline const char *b(bool v) {
	return v ? "[TRUE]" : "[FALSE]";
}

struct MemoryBuffer {
	MemoryBuffer(const void *ptr, quint32 size)
	    : p(ptr)
	    , s(size) {}
	QString str() const {
		QString result;
		const uchar *buf((const uchar *)p);
		const char *hex = "0123456789ABCDEF";
		result.reserve(s * 3);
		for (quint32 i = 0; i < s; ++i) {
			result += hex[(buf[i] >> 4)];
			result += hex[buf[i] & 0x0F];
			result += ' ';
		}
		result.chop(1);
		return result;
	}

	const void *p;
	quint32 s;
};

inline MemoryBuffer mb(const void *ptr, quint32 size) {
	return MemoryBuffer(ptr, size);
}

QString vector(const QVector<MTPlong> &ids);
QString vector(const QVector<quint64> &ids);

} // namespace Logs

#define LOG(msg) (Logs::writeMain(QString msg))
// usage LOG(("log: %1 %2").arg(1).arg(2))

#define DEBUG_LOG(msg)                                                                                                 \
	{                                                                                                                  \
		if (cDebug() || !Logs::started()) Logs::writeDebug(__FILE__, __LINE__, QString msg);                           \
	}
// usage DEBUG_LOG(("log: %1 %2").arg(1).arg(2))

#define TCP_LOG(msg)                                                                                                   \
	{                                                                                                                  \
		if (cDebug() || !Logs::started()) Logs::writeTcp(QString msg);                                                 \
	}
// usage TCP_LOG(("log: %1 %2").arg(1).arg(2))

#define MTP_LOG(dc, msg)                                                                                               \
	{                                                                                                                  \
		if (cDebug() || !Logs::started()) Logs::writeMtp(dc, QString msg);                                             \
	}
// usage MTP_LOG(dc, ("log: %1 %2").arg(1).arg(2))

namespace SignalHandlers {

struct dump {
	~dump();
};
const dump &operator<<(const dump &stream, const char *str);
const dump &operator<<(const dump &stream, const wchar_t *str);
const dump &operator<<(const dump &stream, int num);
const dump &operator<<(const dump &stream, unsigned int num);
const dump &operator<<(const dump &stream, unsigned long num);
const dump &operator<<(const dump &stream, unsigned long long num);
const dump &operator<<(const dump &stream, double num);
enum Status { CantOpen, LastCrashed, Started };
Status start();
Status restart(); // can be only CantOpen or Started
void finish();

void setCrashAnnotation(const std::string &key, const QString &value);

// Remembers value pointer and tries to add the value to the crash report.
// Attention! You should call clearCrashAnnotationRef(key) before destroying value.
void setCrashAnnotationRef(const std::string &key, const QString *valuePtr);
inline void clearCrashAnnotationRef(const std::string &key) {
	setCrashAnnotationRef(key, nullptr);
}

} // namespace SignalHandlers

namespace base {
namespace assertion {

inline void log(const char *message, const char *file, int line) {
	auto info = QStringLiteral("%1 %2:%3").arg(message).arg(file).arg(line);
	LOG(("Assertion Failed! ") + info);
	SignalHandlers::setCrashAnnotation("Assertion", info);
}

} // namespace assertion
} // namespace base
