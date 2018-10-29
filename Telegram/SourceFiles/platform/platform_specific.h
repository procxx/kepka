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

#include <QPainter>
#include <QPoint>
#include <QString>

class QPaintEvent;

namespace Platform {

void start();
void finish();

void SetWatchingMediaKeys(bool watching);
bool TranslucentWindowsSupported(QPoint globalPosition);
void StartTranslucentPaint(QPainter &p, QPaintEvent *e);
void InitOnTopPanel(QWidget *panel);
void DeInitOnTopPanel(QWidget *panel);
void ReInitOnTopPanel(QWidget *panel);

QString SystemLanguage();
QString SystemCountry();

namespace ThirdParty {

void start();
void finish();

} // namespace ThirdParty
} // namespace Platform

#ifdef Q_OS_MAC
#include "platform/mac/specific_mac.h"
#elif defined Q_OS_LINUX // Q_OS_MAC
#include "platform/linux/specific_linux.h"
#elif defined Q_OS_WIN // Q_OS_MAC || Q_OS_LINUX
#include "platform/win/specific_win.h"
#endif // Q_OS_MAC || Q_OS_LINUX || Q_OS_WIN
