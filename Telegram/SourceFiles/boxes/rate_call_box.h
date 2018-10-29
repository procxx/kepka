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
#include "mtproto/sender.h"

namespace Ui {
class InputArea;
class FlatLabel;
class IconButton;
} // namespace Ui

class RateCallBox : public BoxContent, private MTP::Sender {
	Q_OBJECT

public:
	RateCallBox(QWidget *, quint64 callId, quint64 callAccessHash);

private slots:
	void onSend();
	void onCommentResized();
	void onClose() {
		closeBox();
	}

protected:
	void prepare() override;
	void setInnerFocus() override;

	void resizeEvent(QResizeEvent *e) override;

private:
	void updateMaxHeight();
	void ratingChanged(int value);

	quint64 _callId = 0;
	quint64 _callAccessHash = 0;
	int _rating = 0;

	std::vector<object_ptr<Ui::IconButton>> _stars;
	object_ptr<Ui::InputArea> _comment = {nullptr};

	mtpRequestId _requestId = 0;
};
