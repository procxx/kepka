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

#include "base/timer.h"
#include "base/weak_unique_ptr.h"
#include "ui/twidget.h"

namespace Ui {
class IconButton;
class AbstractButton;
class LabelSimple;
class FlatLabel;
} // namespace Ui

namespace Calls {

class Call;

class TopBar : public TWidget, private base::Subscriber {
public:
	TopBar(QWidget *parent, const base::weak_unique_ptr<Call> &call);

	~TopBar();

protected:
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	void initControls();
	void updateInfoLabels();
	void setInfoLabels();
	void updateDurationText();
	void updateControlsGeometry();
	void startDurationUpdateTimer(TimeMs currentDuration);
	void setMuted(bool mute);

	base::weak_unique_ptr<Call> _call;

	bool _muted = false;
	object_ptr<Ui::LabelSimple> _durationLabel;
	object_ptr<Ui::FlatLabel> _fullInfoLabel;
	object_ptr<Ui::FlatLabel> _shortInfoLabel;
	object_ptr<Ui::LabelSimple> _hangupLabel;
	object_ptr<Ui::IconButton> _mute;
	object_ptr<Ui::AbstractButton> _info;
	object_ptr<Ui::IconButton> _hangup;

	base::Timer _updateDurationTimer;
};

} // namespace Calls
