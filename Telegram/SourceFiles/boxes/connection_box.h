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

#include "boxes/abstract_box.h"

namespace Ui {
class InputField;
class PortInput;
class PasswordInput;
class Checkbox;
template <typename Enum> class RadioenumGroup;
template <typename Enum> class Radioenum;
} // namespace Ui

class ConnectionBox : public BoxContent {
	Q_OBJECT

public:
	ConnectionBox(QWidget *parent);

	static void ShowApplyProxyConfirmation(const QMap<QString, QString> &fields);

protected:
	void prepare() override;
	void setInnerFocus() override;

	void resizeEvent(QResizeEvent *e) override;

private slots:
	void onSubmit();
	void onFieldFocus();
	void onSave();

private:
	void typeChanged(DBIConnectionType type);
	void updateControlsVisibility();
	void updateControlsPosition();
	bool badProxyValue() const;

	object_ptr<Ui::InputField> _hostInput;
	object_ptr<Ui::PortInput> _portInput;
	object_ptr<Ui::InputField> _userInput;
	object_ptr<Ui::PasswordInput> _passwordInput;
	std::shared_ptr<Ui::RadioenumGroup<DBIConnectionType>> _typeGroup;
	object_ptr<Ui::Radioenum<DBIConnectionType>> _autoRadio;
	object_ptr<Ui::Radioenum<DBIConnectionType>> _httpProxyRadio;
	object_ptr<Ui::Radioenum<DBIConnectionType>> _tcpProxyRadio;
	object_ptr<Ui::Checkbox> _tryIPv6;
};

class AutoDownloadBox : public BoxContent {
	Q_OBJECT

public:
	AutoDownloadBox(QWidget *parent);

protected:
	void prepare() override;

	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private slots:
	void onSave();

private:
	object_ptr<Ui::Checkbox> _photoPrivate;
	object_ptr<Ui::Checkbox> _photoGroups;
	object_ptr<Ui::Checkbox> _audioPrivate;
	object_ptr<Ui::Checkbox> _audioGroups;
	object_ptr<Ui::Checkbox> _gifPrivate;
	object_ptr<Ui::Checkbox> _gifGroups;
	object_ptr<Ui::Checkbox> _gifPlay;

	int _sectionHeight = 0;
};
