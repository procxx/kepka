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

#include "base/observer.h"
#include "settings/settings_block_widget.h"
#include "ui/twidget.h"

class QPaintEvent;

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace Settings {

class LabeledLink : public TWidget {
public:
	enum class Type {
		Primary,
		Secondary,
	};
	LabeledLink(QWidget *parent, const QString &label, const QString &text, Type type, const char *slot);

	Ui::LinkButton *link() const;

	int naturalWidth() const override;

protected:
	int resizeGetHeight(int newWidth) override;

private:
	object_ptr<Ui::FlatLabel> _label;
	object_ptr<Ui::LinkButton> _link;
};

#ifndef OS_WIN_STORE
class DownloadPathState : public TWidget, private base::Subscriber {
	Q_OBJECT

public:
	DownloadPathState(QWidget *parent);

protected:
	int resizeGetHeight(int newWidth) override;

	void paintEvent(QPaintEvent *e) override;

private slots:
	void onDownloadPath();
	void onClear();
	void onTempDirCleared(int task);
	void onTempDirClearFailed(int task);

private:
	QString downloadPathText() const;
	void updateControls();

	enum class State {
		Empty,
		Exists,
		Clearing,
		Cleared,
		ClearFailed,
	};
	State _state = State::Empty;

	object_ptr<LabeledLink> _path;
	object_ptr<Ui::LinkButton> _clear;
};
#endif // OS_WIN_STORE

class ChatSettingsWidget : public BlockWidget {
	Q_OBJECT

public:
	ChatSettingsWidget(QWidget *parent, UserData *self);

private slots:
	void onReplaceEmoji();
	void onDontAskDownloadPath();
	void onAutomaticMediaDownloadSettings();
	void onManageStickerSets();

private:
	enum class SendByType {
		Enter,
		CtrlEnter,
	};
	void sendByChanged(SendByType value);
	void createControls();

	object_ptr<Ui::Checkbox> _replaceEmoji = {nullptr};
	object_ptr<Ui::Checkbox> _messageFormat = {nullptr};
	object_ptr<Ui::Checkbox> _dontAskDownloadPath = {nullptr};
	void toggleMessageFormat();

#ifndef OS_WIN_STORE
	object_ptr<Ui::WidgetSlideWrap<DownloadPathState>> _downloadPath = {nullptr};
#endif // OS_WIN_STORE

	object_ptr<Ui::Radioenum<SendByType>> _sendByEnter = {nullptr};
	object_ptr<Ui::Radioenum<SendByType>> _sendByCtrlEnter = {nullptr};
	object_ptr<Ui::LinkButton> _automaticMediaDownloadSettings = {nullptr};
	object_ptr<Ui::LinkButton> _manageStickerSets = {nullptr};
};

} // namespace Settings
