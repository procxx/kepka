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
#include "boxes/change_phone_box.h"
#include "app.h" // For formatPhone
#include "base/lambda_guard.h"
#include "boxes/confirm_box.h"
#include "boxes/confirm_phone_box.h"
#include "facades.h"
#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "ui/effects/widget_fade_wrap.h"
#include "ui/toast/toast.h"
#include "ui/widgets/input_fields.h"
#include "ui/widgets/labels.h"

namespace {

void createErrorLabel(QWidget *parent, object_ptr<Ui::WidgetFadeWrap<Ui::FlatLabel>> &label, const QString &text, int x,
                      int y) {
	if (label) {
		auto errorFadeOut = std::move(label);
		errorFadeOut->setUpdateCallback([label = errorFadeOut.data()] {
			if (label->isHidden() || !label->animating()) {
				label->deleteLater();
			}
		});
		errorFadeOut->hideAnimated();
	}
	if (!text.isEmpty()) {
		label.create(parent,
		             object_ptr<Ui::FlatLabel>(parent, text, Ui::FlatLabel::InitType::Simple, st::changePhoneError));
		label->hideFast();
		label->moveToLeft(x, y);
		label->showAnimated();
	}
}

} // namespace

class ChangePhoneBox::EnterPhone : public BoxContent {
public:
	EnterPhone(QWidget *) {}

	void setInnerFocus() override {
		_phone->setFocusFast();
	}

protected:
	void prepare() override;

private:
	void submit();
	void sendPhoneDone(const QString &phoneNumber, const MTPauth_SentCode &result);
	bool sendPhoneFail(const QString &phoneNumber, const RPCError &error);
	void showError(const QString &text);
	void hideError() {
		showError(QString());
	}

	object_ptr<Ui::PhoneInput> _phone = {nullptr};
	object_ptr<Ui::WidgetFadeWrap<Ui::FlatLabel>> _error = {nullptr};
	mtpRequestId _requestId = 0;
};

class ChangePhoneBox::EnterCode : public BoxContent {
public:
	EnterCode(QWidget *, const QString &phone, const QString &hash, int codeLength, int callTimeout);

	void setInnerFocus() override {
		_code->setFocusFast();
	}

protected:
	void prepare() override;

private:
	void submit();
	void sendCall();
	void updateCall();
	bool sendCodeFail(const RPCError &error);
	void showError(const QString &text);
	void hideError() {
		showError(QString());
	}
	int countHeight();

	QString _phone;
	QString _hash;
	int _codeLength = 0;
	int _callTimeout = 0;
	object_ptr<SentCodeField> _code = {nullptr};
	object_ptr<Ui::WidgetFadeWrap<Ui::FlatLabel>> _error = {nullptr};
	object_ptr<Ui::FlatLabel> _callLabel = {nullptr};
	mtpRequestId _requestId = 0;
	SentCodeCall _call;
};

void ChangePhoneBox::EnterPhone::prepare() {
	setTitle(langFactory(lng_change_phone_title));

	auto phoneValue = QString();
	_phone.create(this, st::defaultInputField, langFactory(lng_change_phone_new_title), phoneValue);

	_phone->resize(st::boxWidth - 2 * st::boxPadding.left(), _phone->height());
	_phone->moveToLeft(st::boxPadding.left(), st::boxLittleSkip);
	connect(_phone, &Ui::PhoneInput::submitted, this, [this] { submit(); });

	auto description = object_ptr<Ui::FlatLabel>(this, lang(lng_change_phone_new_description),
	                                             Ui::FlatLabel::InitType::Simple, st::changePhoneLabel);
	auto errorSkip = st::boxLittleSkip + st::changePhoneError.style.font->height;
	description->moveToLeft(st::boxPadding.left(), _phone->y() + _phone->height() + errorSkip + st::boxLittleSkip);

	setDimensions(st::boxWidth, description->bottomNoMargins() + st::boxLittleSkip);

	addButton(langFactory(lng_change_phone_new_submit), [this] { submit(); });
	addButton(langFactory(lng_cancel), [this] { closeBox(); });
}

void ChangePhoneBox::EnterPhone::submit() {
	if (_requestId) {
		return;
	}
	hideError();

	auto phoneNumber = _phone->getLastText().trimmed();
	_requestId = MTP::send(
	    MTPaccount_SendChangePhoneCode(MTP_flags(0), MTP_string(phoneNumber), MTP_bool(false)),
	    rpcDone(base::lambda_guarded(
	        this, [this, phoneNumber](const MTPauth_SentCode &result) { return sendPhoneDone(phoneNumber, result); })),
	    rpcFail(base::lambda_guarded(
	        this, [this, phoneNumber](const RPCError &error) { return sendPhoneFail(phoneNumber, error); })));
}

void ChangePhoneBox::EnterPhone::sendPhoneDone(const QString &phoneNumber, const MTPauth_SentCode &result) {
	Expects(result.type() == mtpc_auth_sentCode);
	_requestId = 0;

	auto codeLength = 0;
	auto &data = result.c_auth_sentCode();
	switch (data.vtype.type()) {
	case mtpc_auth_sentCodeTypeApp:
		LOG(("Error: should not be in-app code!"));
		showError(lang(lng_server_error));
		return;
	case mtpc_auth_sentCodeTypeSms: codeLength = data.vtype.c_auth_sentCodeTypeSms().vlength.v; break;
	case mtpc_auth_sentCodeTypeCall: codeLength = data.vtype.c_auth_sentCodeTypeCall().vlength.v; break;
	case mtpc_auth_sentCodeTypeFlashCall:
		LOG(("Error: should not be flashcall!"));
		showError(lang(lng_server_error));
		return;
	}
	auto phoneCodeHash = qs(data.vphone_code_hash);
	auto callTimeout = 0;
	if (data.has_next_type() && data.vnext_type.type() == mtpc_auth_codeTypeCall) {
		callTimeout = data.has_timeout() ? data.vtimeout.v : 60;
	}
	Ui::show(Box<EnterCode>(phoneNumber, phoneCodeHash, codeLength, callTimeout), KeepOtherLayers);
}

bool ChangePhoneBox::EnterPhone::sendPhoneFail(const QString &phoneNumber, const RPCError &error) {
	auto errorText = lang(lng_server_error);
	if (MTP::isFloodError(error)) {
		errorText = lang(lng_flood_error);
	} else if (MTP::isDefaultHandledError(error)) {
		return false;
	} else if (error.type() == qstr("PHONE_NUMBER_INVALID")) {
		errorText = lang(lng_bad_phone);
	} else if (error.type() == qstr("PHONE_NUMBER_OCCUPIED")) {
		Ui::show(Box<InformBox>(lng_change_phone_occupied(lt_phone, App::formatPhone(phoneNumber)), lang(lng_box_ok)));
		_requestId = 0;
		return true;
	}
	showError(errorText);
	_requestId = 0;
	return true;
}

void ChangePhoneBox::EnterPhone::showError(const QString &text) {
	createErrorLabel(this, _error, text, st::boxPadding.left(), _phone->y() + _phone->height() + st::boxLittleSkip);
	if (!text.isEmpty()) {
		_phone->showError();
	}
}

ChangePhoneBox::EnterCode::EnterCode(QWidget *, const QString &phone, const QString &hash, int codeLength,
                                     int callTimeout)
    : _phone(phone)
    , _hash(hash)
    , _codeLength(codeLength)
    , _callTimeout(callTimeout)
    , _call(this, [this] { sendCall(); }, [this] { updateCall(); }) {}

void ChangePhoneBox::EnterCode::prepare() {
	setTitle(langFactory(lng_change_phone_title));

	auto descriptionText = lng_change_phone_code_description(
	    lt_phone, textcmdStartSemibold() + App::formatPhone(_phone) + textcmdStopSemibold());
	auto description =
	    object_ptr<Ui::FlatLabel>(this, descriptionText, Ui::FlatLabel::InitType::Rich, st::changePhoneLabel);
	description->moveToLeft(st::boxPadding.left(), 0);

	auto phoneValue = QString();
	_code.create(this, st::defaultInputField, langFactory(lng_change_phone_code_title), phoneValue);
	_code->setAutoSubmit(_codeLength, [this] { submit(); });
	_code->setChangedCallback([this] { hideError(); });

	_code->resize(st::boxWidth - 2 * st::boxPadding.left(), _code->height());
	_code->moveToLeft(st::boxPadding.left(), description->bottomNoMargins());
	connect(_code, &Ui::InputField::submitted, this, [this] { submit(); });

	setDimensions(st::boxWidth, countHeight());

	if (_callTimeout > 0) {
		_call.setStatus({SentCodeCall::State::Waiting, _callTimeout});
		updateCall();
	}

	addButton(langFactory(lng_change_phone_new_submit), [this] { submit(); });
	addButton(langFactory(lng_cancel), [this] { closeBox(); });
}

int ChangePhoneBox::EnterCode::countHeight() {
	auto errorSkip = st::boxLittleSkip + st::changePhoneError.style.font->height;
	return _code->bottomNoMargins() + errorSkip + 3 * st::boxLittleSkip;
}

void ChangePhoneBox::EnterCode::submit() {
	if (_requestId) {
		return;
	}
	hideError();

	auto code = _code->getLastText().trimmed();
	_requestId =
	    MTP::send(MTPaccount_ChangePhone(MTP_string(_phone), MTP_string(_hash), MTP_string(code)),
	              rpcDone([weak = weak(this)](const MTPUser &result) {
		              App::feedUser(result);
		              if (weak) {
			              Ui::hideLayer();
		              }
		              Ui::Toast::Show(lang(lng_change_phone_success));
	              }),
	              rpcFail(base::lambda_guarded(this, [this](const RPCError &error) { return sendCodeFail(error); })));
}

void ChangePhoneBox::EnterCode::sendCall() {
	MTP::send(MTPauth_ResendCode(MTP_string(_phone), MTP_string(_hash)),
	          rpcDone(base::lambda_guarded(this, [this] { _call.callDone(); })));
}

void ChangePhoneBox::EnterCode::updateCall() {
	auto text = _call.getText();
	if (text.isEmpty()) {
		_callLabel.destroy();
	} else if (!_callLabel) {
		_callLabel.create(this, text, Ui::FlatLabel::InitType::Simple, st::changePhoneLabel);
		_callLabel->moveToLeft(st::boxPadding.left(), countHeight() - _callLabel->height());
		_callLabel->show();
	} else {
		_callLabel->setText(text);
	}
}

void ChangePhoneBox::EnterCode::showError(const QString &text) {
	createErrorLabel(this, _error, text, st::boxPadding.left(), _code->y() + _code->height() + st::boxLittleSkip);
	if (!text.isEmpty()) {
		_code->showError();
	}
}

bool ChangePhoneBox::EnterCode::sendCodeFail(const RPCError &error) {
	auto errorText = lang(lng_server_error);
	if (MTP::isFloodError(error)) {
		errorText = lang(lng_flood_error);
	} else if (MTP::isDefaultHandledError(error)) {
		return false;
	} else if (error.type() == qstr("PHONE_CODE_EMPTY") || error.type() == qstr("PHONE_CODE_INVALID")) {
		errorText = lang(lng_bad_code);
	} else if (error.type() == qstr("PHONE_CODE_EXPIRED")) {
		closeBox(); // Go back to phone input.
		_requestId = 0;
		return true;
	} else if (error.type() == qstr("PHONE_NUMBER_INVALID")) {
		errorText = lang(lng_bad_phone);
	}
	_requestId = 0;
	showError(errorText);
	return true;
}

void ChangePhoneBox::prepare() {
	setTitle(langFactory(lng_change_phone_title));
	addButton(langFactory(lng_change_phone_button),
	          [] { Ui::show(Box<ConfirmBox>(lang(lng_change_phone_warning), [] { Ui::show(Box<EnterPhone>()); })); });
	addButton(langFactory(lng_cancel), [this] { closeBox(); });

	auto label = object_ptr<Ui::FlatLabel>(this, lang(lng_change_phone_description), Ui::FlatLabel::InitType::Rich,
	                                       st::changePhoneDescription);
	label->moveToLeft((st::boxWideWidth - label->width()) / 2, st::changePhoneDescriptionTop);

	setDimensions(st::boxWideWidth, label->bottomNoMargins() + st::boxLittleSkip);
}

void ChangePhoneBox::paintEvent(QPaintEvent *e) {
	BoxContent::paintEvent(e);

	Painter p(this);
	st::changePhoneIcon.paint(p, (width() - st::changePhoneIcon.width()) / 2, st::changePhoneIconTop, width());
}
