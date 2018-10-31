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
#include "history/history_admin_log_filter.h"

#include "app.h"
#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "ui/effects/ripple_animation.h"
#include "ui/widgets/checkbox.h"

namespace AdminLog {
namespace {

class UserCheckbox : public Ui::RippleButton {
public:
	UserCheckbox(QWidget *parent, not_null<UserData *> user, bool checked);

	bool checked() const {
		return _check->checked();
	}
	base::Observable<bool> checkedChanged;

	enum class NotifyAboutChange {
		Notify,
		DontNotify,
	};
	void setChecked(bool checked, NotifyAboutChange notify = NotifyAboutChange::Notify);

	void finishAnimations();

	QMargins getMargins() const override {
		return _st.margin;
	}

protected:
	void paintEvent(QPaintEvent *e) override;

	int resizeGetHeight(int newWidth) override;

	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

private:
	const style::Checkbox &_st;
	std::unique_ptr<Ui::AbstractCheckView> _check;

	QRect _checkRect;

	not_null<UserData *> _user;
	QString _statusText;
	bool _statusOnline = false;
};

UserCheckbox::UserCheckbox(QWidget *parent, not_null<UserData *> user, bool checked)
    : Ui::RippleButton(parent, st::defaultBoxCheckbox.ripple)
    , _st(st::adminLogFilterUserCheckbox)
    , _check(std::make_unique<Ui::CheckView>(st::defaultCheck, checked, [this] { rtlupdate(_checkRect); }))
    , _user(user) {
	setCursor(style::cur_pointer);
	setClickedCallback([this] {
		if (isDisabled()) return;
		setChecked(!this->checked());
	});
	auto now = unixtime();
	_statusText = App::onlineText(_user, now);
	_statusOnline = App::onlineColorUse(_user, now);
	auto checkSize = _check->getSize();
	_checkRect = {QPoint(_st.margin.left(), (st::contactsPhotoSize - checkSize.height()) / 2), checkSize};
}

void UserCheckbox::setChecked(bool checked, NotifyAboutChange notify) {
	if (_check->checked() != checked) {
		_check->setCheckedAnimated(checked);
		if (notify == NotifyAboutChange::Notify) {
			checkedChanged.notify(checked, true);
		}
	}
}

void UserCheckbox::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();
	auto active = _check->currentAnimationValue(ms);
	auto color = anim::color(_st.rippleBg, _st.rippleBgActive, active);
	paintRipple(p, _st.rippleAreaPosition.x(),
	            _st.rippleAreaPosition.y() + (_checkRect.y() - st::defaultBoxCheckbox.margin.top()), ms, &color);

	auto realCheckRect = myrtlrect(_checkRect);
	if (realCheckRect.intersects(e->rect())) {
		_check->paint(p, _checkRect.left(), _checkRect.top(), width());
	}
	if (realCheckRect.contains(e->rect())) return;

	auto userpicLeft = _checkRect.x() + _checkRect.width() + st::adminLogFilterUserpicLeft;
	auto userpicTop = 0;
	_user->paintUserpicLeft(p, userpicLeft, userpicTop, width(), st::contactsPhotoSize);

	auto nameLeft = userpicLeft + st::contactsPhotoSize + st::contactsPadding.left();
	auto nameTop = userpicTop + st::contactsNameTop;
	auto nameWidth = width() - nameLeft - st::contactsPadding.right();
	p.setPen(st::contactsNameFg);
	_user->nameText.drawLeftElided(p, nameLeft, nameTop, nameWidth, width());

	auto statusLeft = nameLeft;
	auto statusTop = userpicTop + st::contactsStatusTop;
	p.setFont(st::contactsStatusFont);
	p.setPen(_statusOnline ? st::contactsStatusFgOnline : st::contactsStatusFg);
	p.drawTextLeft(statusLeft, statusTop, width(), _statusText);
}

void UserCheckbox::finishAnimations() {
	_check->finishAnimation();
}

int UserCheckbox::resizeGetHeight(int newWidth) {
	return st::contactsPhotoSize;
}

QImage UserCheckbox::prepareRippleMask() const {
	return _check->prepareRippleMask();
}

QPoint UserCheckbox::prepareRippleStartPosition() const {
	auto position = mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition -
	                QPoint(0, _checkRect.y() - st::defaultBoxCheckbox.margin.top());
	return _check->checkRippleStartPosition(position) ? position : DisabledRippleStartPosition();
}

} // namespace

class FilterBox::Inner : public TWidget, private base::Subscriber {
public:
	Inner(QWidget *parent, not_null<ChannelData *> channel, const std::vector<not_null<UserData *>> &admins,
	      const FilterValue &filter, Fn<void()> changedCallback);

	template <typename Widget> QPointer<Widget> addRow(object_ptr<Widget> widget, int marginTop) {
		widget->setParent(this);
		widget->show();
		auto row = Row();
		row.widget = std::move(widget);
		row.marginTop = marginTop;
		_rows.push_back(std::move(row));
		return static_cast<Widget *>(_rows.back().widget.data());
	}

	bool canSave() const;
	FilterValue filter() const;

protected:
	int resizeGetHeight(int newWidth) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void createControls(const std::vector<not_null<UserData *>> &admins, const FilterValue &filter);
	void createAllActionsCheckbox(const FilterValue &filter);
	void createActionsCheckboxes(const FilterValue &filter);
	void createAllUsersCheckbox(const FilterValue &filter);
	void createAdminsCheckboxes(const std::vector<not_null<UserData *>> &admins, const FilterValue &filter);

	not_null<ChannelData *> _channel;

	QPointer<Ui::Checkbox> _allFlags;
	QMap<MTPDchannelAdminLogEventsFilter::Flags, QPointer<Ui::Checkbox>> _filterFlags;

	QPointer<Ui::Checkbox> _allUsers;
	QMap<not_null<UserData *>, QPointer<UserCheckbox>> _admins;
	bool _restoringInvariant = false;

	struct Row {
		object_ptr<TWidget> widget = {nullptr};
		int marginTop = 0;
	};
	std::vector<Row> _rows;

	Fn<void()> _changedCallback;
};

FilterBox::Inner::Inner(QWidget *parent, not_null<ChannelData *> channel,
                        const std::vector<not_null<UserData *>> &admins, const FilterValue &filter,
                        Fn<void()> changedCallback)
    : TWidget(parent)
    , _channel(channel)
    , _changedCallback(std::move(changedCallback)) {
	createControls(admins, filter);
}

void FilterBox::Inner::createControls(const std::vector<not_null<UserData *>> &admins, const FilterValue &filter) {
	createAllActionsCheckbox(filter);
	createActionsCheckboxes(filter);
	createAllUsersCheckbox(filter);
	createAdminsCheckboxes(admins, filter);
}

void FilterBox::Inner::createAllActionsCheckbox(const FilterValue &filter) {
	auto checked = (filter.flags == 0);
	_allFlags = addRow(
	    object_ptr<Ui::Checkbox>(this, lang(lng_admin_log_filter_all_actions), checked, st::adminLogFilterCheckbox),
	    st::adminLogFilterCheckbox.margin.top());
	subscribe(_allFlags->checkedChanged, [this](bool checked) {
		if (!std::exchange(_restoringInvariant, true)) {
			auto allChecked = _allFlags->checked();
			for_const (auto &&checkbox, _filterFlags) { checkbox->setChecked(allChecked); }
			_restoringInvariant = false;
			if (_changedCallback) {
				_changedCallback();
			}
		}
	});
}

void FilterBox::Inner::createActionsCheckboxes(const FilterValue &filter) {
	using Flag = MTPDchannelAdminLogEventsFilter::Flag;
	using Flags = MTPDchannelAdminLogEventsFilter::Flags;
	auto addFlag = [this, &filter](Flags flag, QString &&text) {
		auto checked = (filter.flags == 0) || (filter.flags & flag);
		auto checkbox = addRow(object_ptr<Ui::Checkbox>(this, std::move(text), checked, st::defaultBoxCheckbox),
		                       st::adminLogFilterLittleSkip);
		_filterFlags.insert(flag, checkbox);
		subscribe(checkbox->checkedChanged, [this](bool checked) {
			if (!std::exchange(_restoringInvariant, true)) {
				auto allChecked = true;
				for_const (auto &&checkbox, _filterFlags) {
					if (!checkbox->checked()) {
						allChecked = false;
						break;
					}
				}
				_allFlags->setChecked(allChecked);
				_restoringInvariant = false;
				if (_changedCallback) {
					_changedCallback();
				}
			}
		});
	};
	auto isGroup = _channel->isMegagroup();
	if (isGroup) {
		addFlag(Flag::f_ban | Flag::f_unban | Flag::f_kick | Flag::f_unkick, lang(lng_admin_log_filter_restrictions));
	}
	addFlag(Flag::f_promote | Flag::f_demote, lang(lng_admin_log_filter_admins_new));
	addFlag(Flag::f_join | Flag::f_invite, lang(lng_admin_log_filter_members_new));
	addFlag(Flag::f_info | Flag::f_settings,
	        lang(_channel->isMegagroup() ? lng_admin_log_filter_info_group : lng_admin_log_filter_info_channel));
	addFlag(Flag::f_delete, lang(lng_admin_log_filter_messages_deleted));
	addFlag(Flag::f_edit, lang(lng_admin_log_filter_messages_edited));
	if (isGroup) {
		addFlag(Flag::f_pinned, lang(lng_admin_log_filter_messages_pinned));
	}
	addFlag(Flag::f_leave, lang(lng_admin_log_filter_members_removed));
}

void FilterBox::Inner::createAllUsersCheckbox(const FilterValue &filter) {
	_allUsers = addRow(object_ptr<Ui::Checkbox>(this, lang(lng_admin_log_filter_all_admins), filter.allUsers,
	                                            st::adminLogFilterCheckbox),
	                   st::adminLogFilterSkip);
	subscribe(_allUsers->checkedChanged, [this](bool checked) {
		if (checked && !std::exchange(_restoringInvariant, true)) {
			for_const (auto &&checkbox, _admins) { checkbox->setChecked(true); }
			_restoringInvariant = false;
			if (_changedCallback) {
				_changedCallback();
			}
		}
	});
}

void FilterBox::Inner::createAdminsCheckboxes(const std::vector<not_null<UserData *>> &admins,
                                              const FilterValue &filter) {
	for (auto user : admins) {
		auto checked = filter.allUsers || base::contains(filter.admins, user);
		auto checkbox = addRow(object_ptr<UserCheckbox>(this, user, checked), st::adminLogFilterLittleSkip);
		subscribe(checkbox->checkedChanged, [this](bool checked) {
			if (!std::exchange(_restoringInvariant, true)) {
				auto allChecked = true;
				for_const (auto &&checkbox, _admins) {
					if (!checkbox->checked()) {
						allChecked = false;
						break;
					}
				}
				if (!allChecked) {
					_allUsers->setChecked(allChecked);
				}
				_restoringInvariant = false;
				if (_changedCallback) {
					_changedCallback();
				}
			}
		});
		_admins.insert(user, checkbox);
	}
}

bool FilterBox::Inner::canSave() const {
	for (auto i = _filterFlags.cbegin(), e = _filterFlags.cend(); i != e; ++i) {
		if (i.value()->checked()) {
			return true;
		}
	}
	return false;
}

FilterValue FilterBox::Inner::filter() const {
	auto result = FilterValue();
	auto allChecked = true;
	for (auto i = _filterFlags.cbegin(), e = _filterFlags.cend(); i != e; ++i) {
		if (i.value()->checked()) {
			result.flags |= i.key();
		} else {
			allChecked = false;
		}
	}
	if (allChecked) {
		result.flags = 0;
	}
	result.allUsers = _allUsers->checked();
	if (!result.allUsers) {
		result.admins.reserve(_admins.size());
		for (auto i = _admins.cbegin(), e = _admins.cend(); i != e; ++i) {
			if (i.value()->checked()) {
				result.admins.push_back(i.key());
			}
		}
	}
	return result;
}

int FilterBox::Inner::resizeGetHeight(int newWidth) {
	auto newHeight = 0;
	auto rowWidth = newWidth - st::boxPadding.left() - st::boxPadding.right();
	for (auto &&row : _rows) {
		newHeight += row.marginTop;
		row.widget->resizeToNaturalWidth(rowWidth);
		newHeight += row.widget->heightNoMargins();
	}
	return newHeight;
}

void FilterBox::Inner::resizeEvent(QResizeEvent *e) {
	auto top = 0;
	for (auto &&row : _rows) {
		top += row.marginTop;
		row.widget->moveToLeft(st::boxPadding.left(), top);
		top += row.widget->heightNoMargins();
	}
}

FilterBox::FilterBox(QWidget *, not_null<ChannelData *> channel, const std::vector<not_null<UserData *>> &admins,
                     const FilterValue &filter, Fn<void(FilterValue &&filter)> saveCallback)
    : BoxContent()
    , _channel(channel)
    , _admins(admins)
    , _initialFilter(filter)
    , _saveCallback(std::move(saveCallback)) {}

void FilterBox::prepare() {
	setTitle(langFactory(lng_admin_log_filter_title));

	_inner = setInnerWidget(object_ptr<Inner>(this, _channel, _admins, _initialFilter, [this] { refreshButtons(); }));
	_inner->resizeToWidth(st::boxWideWidth);

	refreshButtons();
	setDimensions(st::boxWideWidth, std::min(_inner->height(), st::boxMaxListHeight));
}

void FilterBox::refreshButtons() {
	clearButtons();
	if (_inner->canSave()) {
		addButton(langFactory(lng_settings_save), [this] {
			if (_saveCallback) {
				_saveCallback(_inner->filter());
			}
		});
	}
	addButton(langFactory(lng_cancel), [this] { closeBox(); });
}

void FilterBox::resizeToContent() {
	_inner->resizeToWidth(st::boxWideWidth);
	setDimensions(_inner->width(), _inner->height());
}

} // namespace AdminLog
