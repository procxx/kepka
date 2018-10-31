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
#include "history/history.h"

namespace Ui {
class Checkbox;
class FlatLabel;
} // namespace Ui

class InformBox;
class ConfirmBox : public BoxContent, public ClickHandlerHost {
public:
	ConfirmBox(QWidget *, const QString &text, FnMut<void()> confirmedCallback = FnMut<void()>(),
	           FnMut<void()> cancelledCallback = FnMut<void()>());
	ConfirmBox(QWidget *, const QString &text, const QString &confirmText,
	           FnMut<void()> confirmedCallback = FnMut<void()>(), FnMut<void()> cancelledCallback = FnMut<void()>());
	ConfirmBox(QWidget *, const QString &text, const QString &confirmText, const style::RoundButton &confirmStyle,
	           FnMut<void()> confirmedCallback = FnMut<void()>(), FnMut<void()> cancelledCallback = FnMut<void()>());
	ConfirmBox(QWidget *, const QString &text, const QString &confirmText, const QString &cancelText,
	           FnMut<void()> confirmedCallback = FnMut<void()>(), FnMut<void()> cancelledCallback = FnMut<void()>());
	ConfirmBox(QWidget *, const QString &text, const QString &confirmText, const style::RoundButton &confirmStyle,
	           const QString &cancelText, FnMut<void()> confirmedCallback = FnMut<void()>(),
	           FnMut<void()> cancelledCallback = FnMut<void()>());

	void updateLink();

	// If strict cancel is set the cancelledCallback is only called if the cancel button was pressed.
	void setStrictCancel(bool strictCancel) {
		_strictCancel = strictCancel;
	}

	// ClickHandlerHost interface
	void clickHandlerActiveChanged(const ClickHandlerPtr &p, bool active) override;
	void clickHandlerPressedChanged(const ClickHandlerPtr &p, bool pressed) override;

protected:
	void prepare() override;

	void keyPressEvent(QKeyEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	struct InformBoxTag {};
	ConfirmBox(const InformBoxTag &, const QString &text, const QString &doneText, Fn<void()> closedCallback);
	FnMut<void()> generateInformCallback(Fn<void()> closedCallback);
	friend class InformBox;

	void confirmed();
	void init(const QString &text);
	void textUpdated();

	QString _confirmText;
	QString _cancelText;
	const style::RoundButton &_confirmStyle;
	bool _informative = false;

	Text _text;
	int _textWidth = 0;
	int _textHeight = 0;

	void updateHover();

	QPoint _lastMousePos;

	bool _confirmed = false;
	bool _cancelled = false;
	bool _strictCancel = false;
	FnMut<void()> _confirmedCallback;
	FnMut<void()> _cancelledCallback;
};

class InformBox : public ConfirmBox {
public:
	InformBox(QWidget *, const QString &text, Fn<void()> closedCallback = Fn<void()>());
	InformBox(QWidget *, const QString &text, const QString &doneText, Fn<void()> closedCallback = Fn<void()>());
};

class MaxInviteBox : public BoxContent {
public:
	MaxInviteBox(QWidget *, not_null<ChannelData *> channel);

protected:
	void prepare() override;

	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	void updateSelected(const QPoint &cursorGlobalPosition);

	not_null<ChannelData *> _channel;

	Text _text;
	qint32 _textWidth, _textHeight;

	QRect _invitationLink;
	bool _linkOver = false;

	QPoint _lastMousePos;
};

class ConvertToSupergroupBox : public BoxContent, public RPCSender {
public:
	ConvertToSupergroupBox(QWidget *, ChatData *chat);

protected:
	void prepare() override;

	void keyPressEvent(QKeyEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	void convertToSupergroup();
	void convertDone(const MTPUpdates &updates);
	bool convertFail(const RPCError &error);

	ChatData *_chat;
	Text _text, _note;
	qint32 _textWidth, _textHeight;
};

class PinMessageBox : public BoxContent, public RPCSender {
public:
	PinMessageBox(QWidget *, ChannelData *channel, MsgId msgId);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	void pinMessage();
	void pinDone(const MTPUpdates &updates);
	bool pinFail(const RPCError &error);

	ChannelData *_channel;
	MsgId _msgId;

	object_ptr<Ui::FlatLabel> _text;
	object_ptr<Ui::Checkbox> _notify = {nullptr};

	mtpRequestId _requestId = 0;
};

class DeleteMessagesBox : public BoxContent, public RPCSender {
public:
	DeleteMessagesBox(QWidget *, HistoryItem *item, bool suggestModerateActions);
	DeleteMessagesBox(QWidget *, const SelectedItemSet &selected);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	void deleteAndClear();

	QVector<FullMsgId> _ids;
	bool _singleItem = false;
	UserData *_moderateFrom = nullptr;
	ChannelData *_moderateInChannel = nullptr;
	bool _moderateBan = false;
	bool _moderateDeleteAll = false;

	object_ptr<Ui::FlatLabel> _text = {nullptr};
	object_ptr<Ui::Checkbox> _forEveryone = {nullptr};
	object_ptr<Ui::Checkbox> _banUser = {nullptr};
	object_ptr<Ui::Checkbox> _reportSpam = {nullptr};
	object_ptr<Ui::Checkbox> _deleteAll = {nullptr};
};

class ConfirmInviteBox : public BoxContent, public RPCSender {
public:
	ConfirmInviteBox(QWidget *, const QString &title, bool isChannel, const MTPChatPhoto &photo, int count,
	                 const QVector<UserData *> &participants);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	object_ptr<Ui::FlatLabel> _title;
	object_ptr<Ui::FlatLabel> _status;
	ImagePtr _photo;
	EmptyUserpic _photoEmpty;
	QVector<UserData *> _participants;

	int _userWidth = 0;
};
