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
#pragma once

#include "ui/widgets/buttons.h"
#include "styles/style_window.h"
#include "styles/style_widgets.h"

class PeerData;

namespace Ui {

class HistoryDownButton : public RippleButton {
public:
	HistoryDownButton(QWidget *parent, const style::TwoIconButton &st);

	void setUnreadCount(int unreadCount);
	int unreadCount() const {
		return _unreadCount;
	}

protected:
	void paintEvent(QPaintEvent *e) override;

	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

private:
	const style::TwoIconButton &_st;

	int _unreadCount = 0;

};

class EmojiButton : public RippleButton {
public:
	EmojiButton(QWidget *parent, const style::IconButton &st);

	void setLoading(bool loading);
	void setColorOverrides(const style::icon *iconOverride, const style::color *colorOverride, const style::color *rippleOverride);

protected:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;

	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

private:
	void step_loading(TimeMs ms, bool timer) {
		if (timer) {
			update();
		}
	}

	const style::IconButton &_st;

	bool _loading = false;
	Animation a_loading;
	BasicAnimation _a_loading;

	const style::icon *_iconOverride = nullptr;
	const style::color *_colorOverride = nullptr;
	const style::color *_rippleOverride = nullptr;

};

class SendButton : public RippleButton {
public:
	SendButton(QWidget *parent);

	enum class Type {
		Send,
		Save,
		Record,
		Cancel,
	};
	Type type() const {
		return _type;
	}
	void setType(Type state);
	void setRecordActive(bool recordActive);
	void finishAnimation();

	void setRecordStartCallback(base::lambda<void()> callback) {
		_recordStartCallback = std::move(callback);
	}
	void setRecordUpdateCallback(base::lambda<void(QPoint globalPos)> callback) {
		_recordUpdateCallback = std::move(callback);
	}
	void setRecordStopCallback(base::lambda<void(bool active)> callback) {
		_recordStopCallback = std::move(callback);
	}
	void setRecordAnimationCallback(base::lambda<void()> callback) {
		_recordAnimationCallback = std::move(callback);
	}

	double recordActiveRatio() {
		return _a_recordActive.current(getms(), _recordActive ? 1. : 0.);
	}

protected:
	void mouseMoveEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;

	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

private:
	void recordAnimationCallback();
	QPixmap grabContent();

	Type _type = Type::Send;
	bool _recordActive = false;
	QPixmap _contentFrom, _contentTo;

	Animation _a_typeChanged;
	Animation _a_recordActive;

	bool _recording = false;
	base::lambda<void()> _recordStartCallback;
	base::lambda<void(bool active)> _recordStopCallback;
	base::lambda<void(QPoint globalPos)> _recordUpdateCallback;
	base::lambda<void()> _recordAnimationCallback;

};

class PeerAvatarButton : public AbstractButton {
public:
	PeerAvatarButton(QWidget *parent,PeerData *peer, const style::PeerAvatarButton &st);

	void setPeer(PeerData *peer) {
		_peer = peer;
		update();
	}

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	PeerData *_peer = nullptr;
	const style::PeerAvatarButton &_st;

};

class NewAvatarButton : public RippleButton {
public:
	NewAvatarButton(QWidget *parent, int size, QPoint position);

	void setImage(const QImage &image);

protected:
	void paintEvent(QPaintEvent *e) override;

	QImage prepareRippleMask() const override;

private:
	QPixmap _image;
	QPoint _position;

};

} // namespace Ui
