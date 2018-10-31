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
template <typename Enum> class RadioenumGroup;
template <typename Enum> class Radioenum;
class InputArea;
} // namespace Ui

class ReportBox : public BoxContent, public RPCSender {
	Q_OBJECT

public:
	ReportBox(QWidget *, PeerData *peer);

private slots:
	void onReport();
	void onReasonResized();
	void onClose() {
		closeBox();
	}

protected:
	void prepare() override;
	void setInnerFocus() override;

	void resizeEvent(QResizeEvent *e) override;

private:
	enum class Reason {
		Spam,
		Violence,
		Pornography,
		Other,
	};
	void reasonChanged(Reason reason);
	void updateMaxHeight();

	void reportDone(const MTPBool &result);
	bool reportFail(const RPCError &error);

	PeerData *_peer;

	std::shared_ptr<Ui::RadioenumGroup<Reason>> _reasonGroup;
	object_ptr<Ui::Radioenum<Reason>> _reasonSpam;
	object_ptr<Ui::Radioenum<Reason>> _reasonViolence;
	object_ptr<Ui::Radioenum<Reason>> _reasonPornography;
	object_ptr<Ui::Radioenum<Reason>> _reasonOther;
	object_ptr<Ui::InputArea> _reasonOtherText = {nullptr};

	mtpRequestId _requestId = 0;
};
