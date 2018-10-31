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
#include "boxes/self_destruction_box.h"

#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"

void SelfDestructionBox::prepare() {
	setTitle(langFactory(lng_self_destruct_title));

	_ttlValues = {30, 90, 180, 365};

	auto fake = object_ptr<Ui::FlatLabel>(this, lang(lng_self_destruct_description), Ui::FlatLabel::InitType::Simple,
	                                      st::boxLabel);
	auto boxHeight = st::boxOptionListPadding.top() + fake->height() + st::boxMediumSkip +
	                 _ttlValues.size() * (st::defaultRadio.diameter + st::boxOptionListSkip) - st::boxOptionListSkip +
	                 st::boxOptionListPadding.bottom() + st::boxPadding.bottom();
	fake.destroy();

	setDimensions(st::boxWidth, boxHeight);

	auto loading =
	    object_ptr<Ui::FlatLabel>(this, lang(lng_contacts_loading), Ui::FlatLabel::InitType::Simple, st::membersAbout);
	loading->moveToLeft((st::boxWidth - loading->width()) / 2, boxHeight / 3);

	addButton(langFactory(lng_cancel), [this] { closeBox(); });

	request(MTPaccount_GetAccountTTL())
	    .done([this, loading = std::move(loading)](const MTPAccountDaysTTL &result) mutable {
		    Expects(result.type() == mtpc_accountDaysTTL);
		    Expects(!_ttlValues.empty());

		    loading.destroy();
		    auto y = st::boxOptionListPadding.top();
		    _description.create(this, lang(lng_self_destruct_description), Ui::FlatLabel::InitType::Simple,
		                        st::boxLabel);
		    _description->moveToLeft(st::boxPadding.left(), y);
		    y += _description->height() + st::boxMediumSkip;

		    auto current = result.c_accountDaysTTL().vdays.v;
		    auto currentAdjusted = _ttlValues[0];
		    for (auto days : _ttlValues) {
			    if (qAbs(current - days) < qAbs(current - currentAdjusted)) {
				    currentAdjusted = days;
			    }
		    }
		    auto group = std::make_shared<Ui::RadiobuttonGroup>(currentAdjusted);
		    auto count = int(_ttlValues.size());
		    _options.reserve(count);
		    for (auto days : _ttlValues) {
			    _options.emplace_back(this, group, days,
			                          (days > 364) ? lng_self_destruct_years(lt_count, days / 365) :
			                                         lng_self_destruct_months(lt_count, std::max(days / 30, 1)),
			                          st::langsButton);
			    _options.back()->moveToLeft(st::boxPadding.left(), y);
			    y += _options.back()->heightNoMargins() + st::boxOptionListSkip;
		    }
		    showChildren();

		    clearButtons();
		    addButton(langFactory(lng_settings_save), [this, group] {
			    MTP::send(MTPaccount_SetAccountTTL(MTP_accountDaysTTL(MTP_int(group->value()))));
			    closeBox();
		    });
		    addButton(langFactory(lng_cancel), [this] { closeBox(); });
	    })
	    .send();
}
