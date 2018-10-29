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
#include "intro/introstart.h"

#include "application.h"
#include "config.h"
#include "intro/introphone.h"
#include "lang/lang_keys.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"

namespace Intro {

StartWidget::StartWidget(QWidget *parent, Widget::Data *data)
    : Step(parent, data, true) {
	setMouseTracking(true);
	setTitleText([] { return str_const_toString(AppName); });
	setDescriptionText(langFactory(lng_intro_about));
	show();
}

void StartWidget::submit() {
	goNext(new Intro::PhoneWidget(parentWidget(), getData()));
}

QString StartWidget::nextButtonText() const {
	return lang(lng_start_msgs);
}

} // namespace Intro
