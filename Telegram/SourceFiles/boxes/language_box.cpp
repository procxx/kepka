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
#include "boxes/language_box.h"
#include "boxes/confirm_box.h"
#include "lang/lang_cloud_manager.h"
#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "storage/localstorage.h"
#include "styles/style_boxes.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"

class LanguageBox::Inner : public TWidget, private base::Subscriber {
public:
	Inner(QWidget *parent, not_null<Languages *> languages);

	void setSelected(int index);
	void refresh();

private:
	void activateCurrent();
	void languageChanged(int languageIndex);

	not_null<Languages *> _languages;
	std::shared_ptr<Ui::RadiobuttonGroup> _group;
	std::vector<object_ptr<Ui::Radiobutton>> _buttons;
};

LanguageBox::Inner::Inner(QWidget *parent, not_null<Languages *> languages)
    : TWidget(parent)
    , _languages(languages) {
	_group = std::make_shared<Ui::RadiobuttonGroup>(0);
	_group->setChangedCallback([this](int value) { languageChanged(value); });
	subscribe(Lang::Current().updated(), [this] {
		activateCurrent();
		refresh();
	});
}

void LanguageBox::Inner::setSelected(int index) {
	_group->setValue(index);
}

void LanguageBox::Inner::refresh() {
	for (auto &button : _buttons) {
		button.destroy();
	}
	_buttons.clear();

	auto y = st::boxOptionListPadding.top() + st::langsButton.margin.top();
	_buttons.reserve(_languages->size());
	auto index = 0;
	for_const (auto &language, *_languages) {
		_buttons.emplace_back(this, _group, index++, language.nativeName, st::langsButton);
		auto button = _buttons.back().data();
		button->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left(), y);
		button->show();
		y += button->heightNoMargins() + st::boxOptionListSkip;
	}
	auto newHeight = y - st::boxOptionListSkip + st::boxOptionListPadding.bottom() + st::langsButton.margin.bottom();
	resize(st::langsWidth, newHeight);
}

void LanguageBox::Inner::languageChanged(int languageIndex) {
	Expects(languageIndex >= 0 && languageIndex < _languages->size());

	activateCurrent();
	auto languageId = (*_languages)[languageIndex].id;
	if (Lang::Current().id() != languageId) {
		// "custom" is applied each time it is passed to switchToLanguage().
		// So we check that the language really has changed.
		Lang::CurrentCloudManager().switchToLanguage(languageId);
	}
}

void LanguageBox::Inner::activateCurrent() {
	auto currentId = Lang::Current().id();
	for (auto i = 0, count = _languages->size(); i != count; ++i) {
		auto languageId = (*_languages)[i].id;
		auto isCurrent = (languageId == currentId) || (languageId == Lang::DefaultLanguageId() && currentId.isEmpty());
		if (isCurrent) {
			_group->setValue(i);
			return;
		}
	}
}

void LanguageBox::prepare() {
	refreshLang();
	subscribe(Lang::Current().updated(), [this] { refreshLang(); });

	_inner = setInnerWidget(object_ptr<Inner>(this, &_languages), st::boxLayerScroll);

	refresh();
	subscribe(Lang::CurrentCloudManager().languageListChanged(), [this] { refresh(); });
}

void LanguageBox::refreshLang() {
	clearButtons();
	addButton(langFactory(lng_box_ok), [this] { closeBox(); });

	setTitle(langFactory(lng_languages));

	update();
}

void LanguageBox::refresh() {
	refreshLanguages();

	_inner->refresh();
	setDimensions(st::langsWidth, std::min(_inner->height(), st::boxMaxListHeight));
}

void LanguageBox::refreshLanguages() {
	_languages = Languages();
	auto list = Lang::CurrentCloudManager().languageList();
	_languages.reserve(list.size() + 1);
	auto currentId = Lang::Current().id();
	auto currentIndex = -1;
	_languages.push_back({qsl("en"), qsl("English"), qsl("English")});
	for (auto &language : list) {
		auto isCurrent =
		    (language.id == currentId) || (language.id == Lang::DefaultLanguageId() && currentId.isEmpty());
		if (language.id != qstr("en")) {
			if (isCurrent) {
				currentIndex = _languages.size();
			}
			_languages.push_back(language);
		} else if (isCurrent) {
			currentIndex = 0;
		}
	}
	if (currentId == qstr("custom")) {
		_languages.insert(_languages.begin(), {currentId, qsl("Custom LangPack"), qsl("Custom LangPack")});
		currentIndex = 0;
	} else if (currentIndex < 0) {
		currentIndex = _languages.size();
		_languages.push_back({currentId, lang(lng_language_name), lang(lng_language_name)});
	}
	_inner->setSelected(currentIndex);
}
