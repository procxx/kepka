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
#include "settings/settings_general_widget.h"

#include "styles/style_settings.h"
#include "lang/lang_keys.h"
#include "ui/effects/widget_slide_wrap.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "storage/localstorage.h"
#include "platform/platform_specific.h"
#include "mainwindow.h"
#include "application.h"
#include "boxes/language_box.h"
#include "boxes/confirm_box.h"
#include "boxes/about_box.h"
#include "core/file_utilities.h"
#include "lang/lang_file_parser.h"
#include "lang/lang_cloud_manager.h"
#include "messenger.h"

namespace Settings {

GeneralWidget::GeneralWidget(QWidget *parent, UserData *self) : BlockWidget(parent, self, lang(lng_settings_section_general))
, _changeLanguage(this, lang(lng_settings_change_lang), st::boxLinkButton) {
	connect(_changeLanguage, SIGNAL(clicked()), this, SLOT(onChangeLanguage()));
	refreshControls();
}

int GeneralWidget::resizeGetHeight(int newWidth) {
	_changeLanguage->moveToRight(contentLeft(), st::settingsBlockMarginTop + st::settingsBlockTitleTop + st::settingsBlockTitleFont->ascent - st::defaultLinkButton.font->ascent, newWidth);
	return BlockWidget::resizeGetHeight(newWidth);
}

void GeneralWidget::refreshControls() {
	style::margins marginSub(0, 0, 0, st::settingsSubSkip);
	style::margins marginLarge(0, 0, 0, st::settingsLargeSkip);
	style::margins marginSmall(0, 0, 0, st::settingsSmallSkip);
	style::margins slidedPadding(0, marginSmall.bottom() / 2, 0, marginSmall.bottom() - (marginSmall.bottom() / 2));

	if (cPlatform() == dbipWindows || cSupportTray()) {
		auto workMode = Global::WorkMode().value();
		addChildRow(_enableTrayIcon, marginSmall, lang(lng_settings_workmode_tray), [this](bool) { onEnableTrayIcon(); }, (workMode == dbiwmTrayOnly || workMode == dbiwmWindowAndTray));
		if (cPlatform() == dbipWindows) {
			addChildRow(_enableTaskbarIcon, marginLarge, lang(lng_settings_workmode_window), [this](bool) { onEnableTaskbarIcon(); }, (workMode == dbiwmWindowOnly || workMode == dbiwmWindowAndTray));

#ifndef OS_WIN_STORE
			addChildRow(_autoStart, marginSmall, lang(lng_settings_auto_start), [this](bool) { onAutoStart(); }, cAutoStart());
			addChildRow(_startMinimized, marginLarge, slidedPadding, lang(lng_settings_start_min), [this](bool) { onStartMinimized(); }, (cStartMinimized() && !Global::LocalPasscode()));
			subscribe(Global::RefLocalPasscodeChanged(), [this] {
				_startMinimized->entity()->setChecked(cStartMinimized() && !Global::LocalPasscode());
			});
			if (!cAutoStart()) {
				_startMinimized->hideFast();
			}
			addChildRow(_addInSendTo, marginSmall, lang(lng_settings_add_sendto), [this](bool) { onAddInSendTo(); }, cSendToMenu());
#endif // OS_WIN_STORE
		}
	}
}

void GeneralWidget::onChangeLanguage() {
	if ((_changeLanguage->clickModifiers() & Qt::ShiftModifier) && (_changeLanguage->clickModifiers() & Qt::AltModifier)) {
		Lang::CurrentCloudManager().switchToLanguage(qsl("custom"));
		return;
	}
	auto manager = Messenger::Instance().langCloudManager();
	if (manager->languageList().isEmpty()) {
		_languagesLoadedSubscription = subscribe(manager->languageListChanged(), [this] {
			unsubscribe(base::take(_languagesLoadedSubscription));
			Ui::show(Box<LanguageBox>());
		});
	} else {
		unsubscribe(base::take(_languagesLoadedSubscription));
		Ui::show(Box<LanguageBox>());
	}
	manager->requestLanguageList();
}

void GeneralWidget::onRestart() {
	App::restart();
}

void GeneralWidget::onEnableTrayIcon() {
	if ((!_enableTrayIcon->checked() || cPlatform() != dbipWindows) && _enableTaskbarIcon && !_enableTaskbarIcon->checked()) {
		_enableTaskbarIcon->setChecked(true);
	} else {
		updateWorkmode();
	}
}

void GeneralWidget::onEnableTaskbarIcon() {
	if (!_enableTrayIcon->checked() && !_enableTaskbarIcon->checked()) {
		_enableTrayIcon->setChecked(true);
	} else {
		updateWorkmode();
	}
}

void GeneralWidget::updateWorkmode() {
	auto newMode = (_enableTrayIcon->checked() && (!_enableTaskbarIcon || _enableTaskbarIcon->checked())) ? dbiwmWindowAndTray : (_enableTrayIcon->checked() ? dbiwmTrayOnly : dbiwmWindowOnly);
	if (Global::WorkMode().value() != newMode && (newMode == dbiwmWindowAndTray || newMode == dbiwmTrayOnly)) {
		cSetSeenTrayTooltip(false);
	}
	Global::RefWorkMode().set(newMode);
	Local::writeSettings();
}

#if !defined OS_WIN_STORE
void GeneralWidget::onAutoStart() {
	cSetAutoStart(_autoStart->checked());
	if (cAutoStart()) {
		psAutoStart(true);
		Local::writeSettings();
	} else {
		psAutoStart(false);
		if (_startMinimized->entity()->checked()) {
			_startMinimized->entity()->setChecked(false);
		} else {
			Local::writeSettings();
		}
	}
	_startMinimized->toggleAnimated(cAutoStart());
}

void GeneralWidget::onStartMinimized() {
	auto checked = _startMinimized->entity()->checked();
	if (Global::LocalPasscode()) {
		if (checked) {
			_startMinimized->entity()->setChecked(false);
			Ui::show(Box<InformBox>(lang(lng_error_start_minimized_passcoded)));
		}
		return;
	}
	if (cStartMinimized() != checked) {
		cSetStartMinimized(checked);
		Local::writeSettings();
	}
}

void GeneralWidget::onAddInSendTo() {
	cSetSendToMenu(_addInSendTo->checked());
	psSendToMenu(_addInSendTo->checked());
	Local::writeSettings();
}
#endif // !OS_WIN_STORE

} // namespace Settings
