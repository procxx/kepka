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
#include "boxes/about_box.h"
#include <QDesktopServices>

#include "application.h"
#include "boxes/confirm_box.h"
#include "config.h"
#include "lang/lang_keys.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "platform/platform_file_utilities.h"
#include "styles/style_boxes.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"

AboutBox::AboutBox(QWidget *parent)
    : _version(this,
               lng_about_version(lt_version, QString::fromLatin1(AppVersionStr.c_str()) +
                                                 (cAlphaVersion() ? " alpha" : "") +
                                                 (cBetaVersion() ? qsl(" beta %1").arg(cBetaVersion()) : QString())),
               st::aboutVersionLink)
    , _text1(this, lang(lng_about_text_1), Ui::FlatLabel::InitType::Rich, st::aboutLabel)
    , _text2(this, lang(lng_about_text_2), Ui::FlatLabel::InitType::Rich, st::aboutLabel)
    , _text3(this, st::aboutLabel) {}

void AboutBox::prepare() {
	setTitle([] { return str_const_toString(AppName); });

	addButton(langFactory(lng_close), [this] { closeBox(); });

	_text3->setRichText(
	    lng_about_text_3(lt_faq_open, qsl("[a href=\"%1\"]").arg(telegramFaqLink()), lt_faq_close, qsl("[/a]")));

	_version->setClickedCallback([this] { showVersionHistory(); });

	setDimensions(st::aboutWidth, st::aboutTextTop + _text1->height() + st::aboutSkip + _text2->height() +
	                                  st::aboutSkip + _text3->height());
}

void AboutBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	_version->moveToLeft(st::boxPadding.left(), st::aboutVersionTop);
	_text1->moveToLeft(st::boxPadding.left(), st::aboutTextTop);
	_text2->moveToLeft(st::boxPadding.left(), _text1->y() + _text1->height() + st::aboutSkip);
	_text3->moveToLeft(st::boxPadding.left(), _text2->y() + _text2->height() + st::aboutSkip);
}

void AboutBox::showVersionHistory() {
	QDesktopServices::openUrl(lang(lng_url_changelog));
}

void AboutBox::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
		closeBox();
	} else {
		BoxContent::keyPressEvent(e);
	}
}

QString telegramFaqLink() {
	auto result = qsl("https://telegram.org/faq");
	auto language = Lang::Current().id();
	for (auto faqLanguage : {"de", "es", "it", "ko", "br"}) {
		if (language.startsWith(QLatin1String(faqLanguage))) {
			result.append('/').append(faqLanguage);
		}
	}
	return result;
}

QString currentVersionText() {
	auto result = QString::fromLatin1(AppVersionStr.c_str());
	if (cAlphaVersion()) {
		result += " alpha";
	}
	if (cBetaVersion()) {
		result += qsl(" beta %1").arg(cBetaVersion() % 1000);
	}
	return result;
}
