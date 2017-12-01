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
#include "twidget.h"

#include "application.h"
#include "mainwindow.h"

namespace Fonts {
namespace {

bool ValidateFont(const QString &familyName, int flags = 0) {
	QFont checkFont(familyName);
	checkFont.setPixelSize(13);
	checkFont.setBold(flags & style::internal::FontBold);
	checkFont.setItalic(flags & style::internal::FontItalic);
	checkFont.setUnderline(flags & style::internal::FontUnderline);
	checkFont.setStyleStrategy(QFont::PreferQuality);
	auto realFamily = QFontInfo(checkFont).family();
	if (realFamily.trimmed().compare(familyName, Qt::CaseInsensitive)) {
		LOG(("Font Error: could not resolve '%1' font, got '%2' after feeding '%3'.").arg(familyName).arg(realFamily));
		return false;
	}

	auto metrics = QFontMetrics(checkFont);
	if (!metrics.height()) {
		LOG(("Font Error: got a zero height in '%1'.").arg(familyName));
		return false;
	}

	return true;
}

bool LoadCustomFont(const QString &filePath, const QString &familyName, int flags = 0) {
	auto regularId = QFontDatabase::addApplicationFont(filePath);
	if (regularId < 0) {
		LOG(("Font Error: could not add '%1'.").arg(filePath));
		return false;
	}

	auto found = [&familyName, regularId] {
		for (auto &family : QFontDatabase::applicationFontFamilies(regularId)) {
			if (!family.trimmed().compare(familyName, Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	};
	if (!found()) {
		LOG(("Font Error: could not locate '%1' font in '%2'.").arg(familyName).arg(filePath));
		return false;
	}

	return ValidateFont(familyName, flags);
}

bool Started = false;
QString OpenSansOverride;
QString OpenSansSemiboldOverride;

} // namespace

void Start() {
	if (Started) {
		return;
	}
	Started = true;

	auto regular = LoadCustomFont(qsl(":/gui/fonts/OpenSans-Regular.ttf"), qsl("Open Sans"));
	auto bold = LoadCustomFont(qsl(":/gui/fonts/OpenSans-Bold.ttf"), qsl("Open Sans"), style::internal::FontBold);
	auto semibold = LoadCustomFont(qsl(":/gui/fonts/OpenSans-Semibold.ttf"), qsl("Open Sans Semibold"));

#ifdef Q_OS_WIN
	// Attempt to workaround a strange font bug with Open Sans Semibold not loading.
	// See https://github.com/telegramdesktop/tdesktop/issues/3276 for details.
	// Crash happens on "options.maxh / _t->_st->font->height" with "division by zero".
	// In that place "_t->_st->font" is "semiboldFont" is "font(13 "Open Sans Semibold").
	if (!regular || !bold) {
		if (ValidateFont(qsl("Segoe UI")) && ValidateFont(qsl("Segoe UI"), style::internal::FontBold)) {
			OpenSansOverride = qsl("Segoe UI");
			LOG(("Fonts Info: Using Segoe UI instead of Open Sans."));
		}
	}
	if (!semibold) {
		if (ValidateFont(qsl("Segoe UI Semibold"))) {
			OpenSansSemiboldOverride = qsl("Segoe UI Semibold");
			LOG(("Fonts Info: Using Segoe UI Semibold instead of Open Sans Semibold."));
		}
	}
#endif // Q_OS_WIN
}

QString GetOverride(const QString &familyName) {
	if (familyName == qstr("Open Sans")) {
		return OpenSansOverride.isEmpty() ? familyName : OpenSansOverride;
	} else if (familyName == qstr("Open Sans Semibold")) {
		return OpenSansSemiboldOverride.isEmpty() ? familyName : OpenSansSemiboldOverride;
	}
	return familyName;
}

} // Fonts

namespace {

void _sendResizeEvents(QWidget *target) {
	QResizeEvent e(target->size(), QSize());
	QApplication::sendEvent(target, &e);

	const QObjectList children = target->children();
	for (int i = 0; i < children.size(); ++i) {
		QWidget *child = static_cast<QWidget*>(children.at(i));
		if (child->isWidgetType() && !child->isWindow() && child->testAttribute(Qt::WA_PendingResizeEvent)) {
			_sendResizeEvents(child);
		}
	}
}

} // namespace

bool TWidget::inFocusChain() const {
	return !isHidden() && App::wnd() && (App::wnd()->focusWidget() == this || isAncestorOf(App::wnd()->focusWidget()));
}

void myEnsureResized(QWidget *target) {
	if (target && (target->testAttribute(Qt::WA_PendingResizeEvent) || !target->testAttribute(Qt::WA_WState_Created))) {
		_sendResizeEvents(target);
	}
}

QPixmap myGrab(TWidget *target, QRect rect, QColor bg) {
	myEnsureResized(target);
	if (rect.isNull()) rect = target->rect();

    auto result = QPixmap(rect.size() * cIntRetinaFactor());
    result.setDevicePixelRatio(cRetinaFactor());
	if (!target->testAttribute(Qt::WA_OpaquePaintEvent)) {
		result.fill(bg);
	}

	App::wnd()->widgetGrabbed().notify(true);

	target->grabStart();
	target->render(&result, QPoint(0, 0), rect, QWidget::DrawChildren | QWidget::IgnoreMask);
	target->grabFinish();

	return result;
}

QImage myGrabImage(TWidget *target, QRect rect, QColor bg) {
	myEnsureResized(target);
	if (rect.isNull()) rect = target->rect();

	auto result = QImage(rect.size() * cIntRetinaFactor(), QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(cRetinaFactor());
	if (!target->testAttribute(Qt::WA_OpaquePaintEvent)) {
		result.fill(bg);
	}

	target->grabStart();
	target->render(&result, QPoint(0, 0), rect, QWidget::DrawChildren | QWidget::IgnoreMask);
	target->grabFinish();

	return result;
}

void sendSynteticMouseEvent(QWidget *widget, QEvent::Type type, Qt::MouseButton button, const QPoint &globalPoint) {
	if (auto windowHandle = widget->window()->windowHandle()) {
		auto localPoint = windowHandle->mapFromGlobal(globalPoint);
		QMouseEvent ev(type
			, localPoint
			, localPoint
			, globalPoint
			, button
			, QGuiApplication::mouseButtons() | button
			, QGuiApplication::keyboardModifiers()
#if !defined(OS_MAC_OLD) && QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
			, Qt::MouseEventSynthesizedByApplication
#endif
		);
		ev.setTimestamp(getms());
		QGuiApplication::sendEvent(windowHandle, &ev);
	}
}
