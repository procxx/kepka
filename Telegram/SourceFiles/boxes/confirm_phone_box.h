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
#include "ui/widgets/input_fields.h"

namespace Ui {
class InputField;
class FlatLabel;
} // namespace Ui

class SentCodeField : public Ui::InputField {
public:
	SentCodeField(QWidget *parent, const style::InputField &st, Fn<QString()> placeholderFactory = Fn<QString()>(),
	              const QString &val = QString())
	    : Ui::InputField(parent, st, std::move(placeholderFactory), val) {
		connect(this, &Ui::InputField::changed, [this] { fix(); });
	}

	void setAutoSubmit(int length, Fn<void()> submitCallback) {
		_autoSubmitLength = length;
		_submitCallback = std::move(submitCallback);
	}
	void setChangedCallback(Fn<void()> changedCallback) {
		_changedCallback = std::move(changedCallback);
	}

private:
	void fix();

	// Flag for not calling onTextChanged() recursively.
	bool _fixing = false;

	int _autoSubmitLength = 0;
	Fn<void()> _submitCallback;
	Fn<void()> _changedCallback;
};

class SentCodeCall {
public:
	SentCodeCall(QObject *parent, FnMut<void()> callCallback, Fn<void()> updateCallback);

	enum class State {
		Waiting,
		Calling,
		Called,
		Disabled,
	};
	struct Status {
		Status() {}
		Status(State state, int timeout)
		    : state(state)
		    , timeout(timeout) {}

		State state = State::Disabled;
		int timeout = 0;
	};
	void setStatus(const Status &status);

	void callDone() {
		if (_status.state == State::Calling) {
			_status.state = State::Called;
			if (_update) {
				_update();
			}
		}
	}

	QString getText() const;

private:
	Status _status;
	object_ptr<QTimer> _timer;
	FnMut<void()> _call;
	Fn<void()> _update;
};

class ConfirmPhoneBox : public BoxContent, public RPCSender {
	Q_OBJECT

public:
	static void start(const QString &phone, const QString &hash);

	~ConfirmPhoneBox();

private slots:
	void onSendCode();

protected:
	void prepare() override;
	void setInnerFocus() override;

	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	ConfirmPhoneBox(QWidget *, const QString &phone, const QString &hash);
	friend class object_ptr<ConfirmPhoneBox>;

	void sendCall();
	void checkPhoneAndHash();

	void sendCodeDone(const MTPauth_SentCode &result);
	bool sendCodeFail(const RPCError &error);

	void callDone(const MTPauth_SentCode &result);

	void confirmDone(const MTPBool &result);
	bool confirmFail(const RPCError &error);

	QString getPhone() const {
		return _phone;
	}
	void launch();

	void showError(const QString &error);

	mtpRequestId _sendCodeRequestId = 0;

	// _hash from the link for account.sendConfirmPhoneCode call.
	// _phoneHash from auth.sentCode for account.confirmPhone call.
	QString _phone, _hash;
	QString _phoneHash;

	// If we receive the code length, we autosubmit _code field when enough symbols is typed.
	int _sentCodeLength = 0;

	mtpRequestId _checkCodeRequestId = 0;

	object_ptr<Ui::FlatLabel> _about = {nullptr};
	object_ptr<SentCodeField> _code = {nullptr};

	QString _error;
	SentCodeCall _call;
};
