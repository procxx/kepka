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

#include "window/themes/window_theme_preview.h"
#include "window/window_title.h"

namespace Platform {

object_ptr<Window::TitleWidget> CreateTitleWidget(QWidget *parent);

int PreviewTitleHeight();
void PreviewWindowFramePaint(QImage &preview, const style::palette &palette, QRect body, int outerWidth);

} // namespace Platform

// Platform dependent implementations.

#ifdef Q_OS_MAC
#include "platform/mac/window_title_mac.h"
#elif defined Q_OS_WIN // Q_OS_MAC
#include "platform/win/window_title_win.h"
#elif defined Q_OS_WINRT || defined Q_OS_LINUX // Q_OS_MAC || Q_OS_WIN

namespace Platform {

inline object_ptr<Window::TitleWidget> CreateTitleWidget(QWidget *parent) {
	return {nullptr};
}

inline int PreviewTitleHeight() {
	return Window::Theme::DefaultPreviewTitleHeight();
}

inline void PreviewWindowFramePaint(QImage &preview, const style::palette &palette, QRect body, int outerWidth) {
	return Window::Theme::DefaultPreviewWindowFramePaint(preview, palette, body, outerWidth);
}

} // namespace Platform

#endif // Q_OS_MAC || Q_OS_WIN || Q_OS_WINRT || Q_OS_LINUX
