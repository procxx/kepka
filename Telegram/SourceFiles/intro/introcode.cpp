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
#include "intro/introcode.h"

#include "app.h" // for formatPhone
#include "application.h"
#include "intro/intropwdcheck.h"
#include "intro/introsignup.h"
#include "lang/lang_keys.h"
#include "styles/style_intro.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"

namespace Intro {

CodeInput::CodeInput(QWidget *parent, const style::InputField &st, Fn<QString()> placeholderFactory)
    : Ui::MaskedInputField(parent, st, std::move(placeholderFactory)) {}

void CodeInput::setDigitsCountMax(int digitsCount) {
	_digitsCountMax = digitsCount;
}

void CodeInput::correctValue(const QString &was, int wasCursor, QString &now, int &nowCursor) {
	QString newText;
	int oldPos(nowCursor), newPos(-1), oldLen(now.length()), digitCount = 0;
	for (int i = 0; i < oldLen; ++i) {
		if (now[i].isDigit()) {
			++digitCount;
		}
	}
	accumulate_min(digitCount, _digitsCountMax);
	auto strict = (digitCount == _digitsCountMax);

	newText.reserve(oldLen);
	for (int i = 0; i < oldLen; ++i) {
		QChar ch(now[i]);
		if (ch.isDigit()) {
			if (!digitCount--) {
				break;
			}
			newText += ch;
			if (strict && !digitCount) {
				break;
			}
		}
		if (i == oldPos) {
			newPos = newText.length();
		}
	}
	if (newPos < 0 || newPos > newText.size()) {
		newPos = newText.size();
	}
	if (newText != now) {
		now = newText;
		setText(now);
		startPlaceholderAnimation();
	}
	if (newPos != nowCursor) {
		nowCursor = newPos;
		setCursorPosition(nowCursor);
	}

	if (strict) emit codeEntered();
}

CodeWidget::CodeWidget(QWidget *parent, Widget::Data *data)
    : Step(parent, data)
    , _noTelegramCode(this, lang(lng_code_no_telegram), st::introLink)
    , _code(this, st::introCode, langFactory(lng_code_ph))
    , _callTimer(this)
    , _callStatus(getData()->callStatus)
    , _callTimeout(getData()->callTimeout)
    , _callLabel(this, st::introDescription)
    , _checkRequest(this) {
	subscribe(Lang::Current().updated(), [this] { refreshLang(); });

	connect(_code, SIGNAL(changed()), this, SLOT(onInputChange()));
	connect(_callTimer, SIGNAL(timeout()), this, SLOT(onSendCall()));
	connect(_checkRequest, SIGNAL(timeout()), this, SLOT(onCheckRequest()));
	connect(_noTelegramCode, SIGNAL(clicked()), this, SLOT(onNoTelegramCode()));

	_code->setDigitsCountMax(getData()->codeLength);
	setErrorBelowLink(true);

	setTitleText([text = App::formatPhone(getData()->phone)] { return text; });
	updateDescText();
}

void CodeWidget::refreshLang() {
	if (_noTelegramCode) _noTelegramCode->setText(lang(lng_code_no_telegram));
	updateDescText();
	updateControlsGeometry();
}

void CodeWidget::updateDescText() {
	setDescriptionText(langFactory(getData()->codeByTelegram ? lng_code_telegram : lng_code_desc));
	if (getData()->codeByTelegram) {
		_noTelegramCode->show();
		_callTimer->stop();
	} else {
		_noTelegramCode->hide();
		_callStatus = getData()->callStatus;
		_callTimeout = getData()->callTimeout;
		if (_callStatus == Widget::Data::CallStatus::Waiting && !_callTimer->isActive()) {
			_callTimer->start(1000);
		}
	}
	updateCallText();
}

void CodeWidget::updateCallText() {
	auto text = ([this]() -> QString {
		if (getData()->codeByTelegram) {
			return QString();
		}
		switch (_callStatus) {
		case Widget::Data::CallStatus::Waiting: {
			if (_callTimeout >= 3600) {
				return lng_code_call(
				    lt_minutes, qsl("%1:%2").arg(_callTimeout / 3600).arg((_callTimeout / 60) % 60, 2, 10, QChar('0')),
				    lt_seconds, qsl("%1").arg(_callTimeout % 60, 2, 10, QChar('0')));
			} else {
				return lng_code_call(lt_minutes, QString::number(_callTimeout / 60), lt_seconds,
				                     qsl("%1").arg(_callTimeout % 60, 2, 10, QChar('0')));
			}
		} break;
		case Widget::Data::CallStatus::Calling: return lang(lng_code_calling);
		case Widget::Data::CallStatus::Called: return lang(lng_code_called);
		}
		return QString();
	})();
	_callLabel->setText(text);
	_callLabel->setVisible(!text.isEmpty() && !animating());
}

void CodeWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateControlsGeometry();
}

void CodeWidget::updateControlsGeometry() {
	_code->moveToLeft(contentLeft(), contentTop() + st::introStepFieldTop);
	auto linkTop = _code->y() + _code->height() + st::introLinkTop;
	_noTelegramCode->moveToLeft(contentLeft() + st::buttonRadius, linkTop);
	_callLabel->moveToLeft(contentLeft() + st::buttonRadius, linkTop);
}

void CodeWidget::showCodeError(Fn<QString()> textFactory) {
	if (textFactory) _code->showError();
	showError(std::move(textFactory));
}

void CodeWidget::setInnerFocus() {
	_code->setFocusFast();
}

void CodeWidget::activate() {
	Step::activate();
	_code->show();
	if (getData()->codeByTelegram) {
		_noTelegramCode->show();
	} else {
		_callLabel->show();
	}
	setInnerFocus();
}

void CodeWidget::finished() {
	Step::finished();
	_checkRequest->stop();
	_callTimer->stop();
	rpcInvalidate();

	cancelled();
	_sentCode.clear();
	_code->setText(QString());
}

void CodeWidget::cancelled() {
	MTP::cancel(base::take(_sentRequest));
	MTP::cancel(base::take(_callRequestId));
	MTP::send(MTPauth_CancelCode(MTP_string(getData()->phone), MTP_bytes(getData()->phoneHash)));
}

void CodeWidget::stopCheck() {
	_checkRequest->stop();
}

void CodeWidget::onCheckRequest() {
	auto status = MTP::state(_sentRequest);
	if (status < 0) {
		auto leftms = -status;
		if (leftms >= 1000) {
			if (_sentRequest) {
				MTP::cancel(base::take(_sentRequest));
				_sentCode.clear();
			}
		}
	}
	if (!_sentRequest && status == MTP::RequestSent) {
		stopCheck();
	}
}

void CodeWidget::codeSubmitDone(const MTPauth_Authorization &result) {
	stopCheck();
	_sentRequest = 0;
	auto &d = result.c_auth_authorization();
	if (d.vuser.type() != mtpc_user || !d.vuser.c_user().is_self()) { // wtf?
		showCodeError(langFactory(lng_server_error));
		return;
	}
	cSetLoggedPhoneNumber(getData()->phone);
	finish(d.vuser);
}

bool CodeWidget::codeSubmitFail(const RPCError &error) {
	if (MTP::isFloodError(error)) {
		stopCheck();
		_sentRequest = 0;
		showCodeError(langFactory(lng_flood_error));
		return true;
	}
	if (MTP::isDefaultHandledError(error)) return false;

	stopCheck();
	_sentRequest = 0;
	auto &err = error.type();
	if (err == qstr("PHONE_NUMBER_INVALID") || err == qstr("PHONE_CODE_EXPIRED")) { // show error
		goBack();
		return true;
	} else if (err == qstr("PHONE_CODE_EMPTY") || err == qstr("PHONE_CODE_INVALID")) {
		showCodeError(langFactory(lng_bad_code));
		return true;
	} else if (err == qstr("PHONE_NUMBER_UNOCCUPIED")) { // success, need to signUp
		getData()->code = _sentCode;
		goReplace(new Intro::SignupWidget(parentWidget(), getData()));
		return true;
	} else if (err == qstr("SESSION_PASSWORD_NEEDED")) {
		getData()->code = _sentCode;
		_checkRequest->start(1000);
		_sentRequest = MTP::send(MTPaccount_GetPassword(), rpcDone(&CodeWidget::gotPassword),
		                         rpcFail(&CodeWidget::codeSubmitFail));
		return true;
	}
	if (cDebug()) { // internal server error
		auto text = err + ": " + error.description();
		showCodeError([text] { return text; });
	} else {
		showCodeError(langFactory(lng_server_error));
	}
	return false;
}

void CodeWidget::onInputChange() {
	hideError();
	if (_code->getLastText().length() == getData()->codeLength) {
		submit();
	}
}

void CodeWidget::onSendCall() {
	if (_callStatus == Widget::Data::CallStatus::Waiting) {
		if (--_callTimeout <= 0) {
			_callStatus = Widget::Data::CallStatus::Calling;
			_callTimer->stop();
			_callRequestId =
			    MTP::send(MTPauth_ResendCode(MTP_string(getData()->phone), MTP_bytes(getData()->phoneHash)),
			              rpcDone(&CodeWidget::callDone));
		} else {
			getData()->callStatus = _callStatus;
			getData()->callTimeout = _callTimeout;
		}
		updateCallText();
	}
}

void CodeWidget::callDone(const MTPauth_SentCode &v) {
	if (v.type() == mtpc_auth_sentCode) {
		fillSentCodeData(v.c_auth_sentCode().vtype);
		_code->setDigitsCountMax(getData()->codeLength);
	}
	if (_callStatus == Widget::Data::CallStatus::Calling) {
		_callStatus = Widget::Data::CallStatus::Called;
		getData()->callStatus = _callStatus;
		getData()->callTimeout = _callTimeout;
		updateCallText();
	}
}

void CodeWidget::gotPassword(const MTPaccount_Password &result) {
	stopCheck();
	_sentRequest = 0;
	switch (result.type()) {
	case mtpc_account_noPassword: { // should not happen
		_code->setFocus();
	} break;

	case mtpc_account_password: {
		auto &d = result.c_account_password();
		getData()->pwdSalt = qba(d.vcurrent_salt);
		getData()->hasRecovery = mtpIsTrue(d.vhas_recovery);
		getData()->pwdHint = qs(d.vhint);
		goReplace(new Intro::PwdCheckWidget(parentWidget(), getData()));
	} break;
	}
}

void CodeWidget::submit() {
	if (_sentRequest) return;

	hideError();

	_checkRequest->start(1000);

	_sentCode = _code->getLastText();
	getData()->pwdSalt = QByteArray();
	getData()->hasRecovery = false;
	getData()->pwdHint = QString();
	_sentRequest =
	    MTP::send(MTPauth_SignIn(MTP_string(getData()->phone), MTP_bytes(getData()->phoneHash), MTP_string(_sentCode)),
	              rpcDone(&CodeWidget::codeSubmitDone), rpcFail(&CodeWidget::codeSubmitFail));
}

void CodeWidget::onNoTelegramCode() {
	if (_noTelegramCodeRequestId) return;
	_noTelegramCodeRequestId =
	    MTP::send(MTPauth_ResendCode(MTP_string(getData()->phone), MTP_bytes(getData()->phoneHash)),
	              rpcDone(&CodeWidget::noTelegramCodeDone), rpcFail(&CodeWidget::noTelegramCodeFail));
}

void CodeWidget::noTelegramCodeDone(const MTPauth_SentCode &result) {
	if (result.type() != mtpc_auth_sentCode) {
		showCodeError(langFactory(lng_server_error));
		return;
	}

	auto &d = result.c_auth_sentCode();
	fillSentCodeData(d.vtype);
	_code->setDigitsCountMax(getData()->codeLength);
	if (d.has_next_type() && d.vnext_type.type() == mtpc_auth_codeTypeCall) {
		getData()->callStatus = Widget::Data::CallStatus::Waiting;
		getData()->callTimeout = d.has_timeout() ? d.vtimeout.v : 60;
	} else {
		getData()->callStatus = Widget::Data::CallStatus::Disabled;
		getData()->callTimeout = 0;
	}
	getData()->codeByTelegram = false;
	updateDescText();
}

bool CodeWidget::noTelegramCodeFail(const RPCError &error) {
	if (MTP::isFloodError(error)) {
		showCodeError(langFactory(lng_flood_error));
		return true;
	}
	if (MTP::isDefaultHandledError(error)) return false;

	if (cDebug()) { // internal server error
		auto text = error.type() + ": " + error.description();
		showCodeError([text] { return text; });
	} else {
		showCodeError(langFactory(lng_server_error));
	}
	return false;
}

} // namespace Intro
