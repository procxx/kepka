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
#include "window/window_controller.h"

#include "apiwrap.h"
#include "auth_session.h"
#include "boxes/calendar_box.h"
#include "mainwidget.h"
#include "styles/style_dialogs.h"
#include "styles/style_window.h"
#include "window/main_window.h"

namespace Window {

void Controller::enableGifPauseReason(GifPauseReason reason) {
	if (!(_gifPauseReasons & reason)) {
		auto notify = (static_cast<int>(_gifPauseReasons) < static_cast<int>(reason));
		_gifPauseReasons |= reason;
		if (notify) {
			_gifPauseLevelChanged.notify();
		}
	}
}

void Controller::disableGifPauseReason(GifPauseReason reason) {
	if (_gifPauseReasons & reason) {
		_gifPauseReasons &= ~reason;
		if (_gifPauseReasons < reason) {
			_gifPauseLevelChanged.notify();
		}
	}
}

bool Controller::isGifPausedAtLeastFor(GifPauseReason reason) const {
	if (reason == GifPauseReason::Any) {
		return (_gifPauseReasons != 0) || !window()->isActive();
	}
	return (static_cast<int>(_gifPauseReasons) >= 2 * static_cast<int>(reason)) || !window()->isActive();
}

int Controller::dialogsSmallColumnWidth() const {
	return st::dialogsPadding.x() + st::dialogsPhotoSize + st::dialogsPadding.x();
}

Controller::ColumnLayout Controller::computeColumnLayout() const {
	auto layout = Adaptive::WindowLayout::OneColumn;

	auto bodyWidth = window()->bodyWidget()->width();
	int dialogsWidth = std::round(bodyWidth * dialogsWidthRatio().value());
	auto chatWidth = bodyWidth - dialogsWidth;
	accumulate_max(chatWidth, st::windowMinWidth);
	dialogsWidth = bodyWidth - chatWidth;

	auto useOneColumnLayout = [bodyWidth, dialogsWidth] {
		auto someSectionShown = !App::main()->selectingPeer() && App::main()->isSectionShown();
		return (dialogsWidth < st::dialogsPadding.x() && (Adaptive::OneColumn() || someSectionShown)) ||
		       (bodyWidth < st::windowMinWidth + st::dialogsWidthMin);
	};

	auto useSmallColumnLayout = [dialogsWidth] {
		// Used if useOneColumnLayout() == false.
		return dialogsWidth < st::dialogsWidthMin / 2;
	};

	if (useOneColumnLayout()) {
		dialogsWidth = chatWidth = bodyWidth;
	} else if (useSmallColumnLayout()) {
		layout = Adaptive::WindowLayout::SmallColumn;
		auto forceWideDialogs = [this] {
			if (dialogsListDisplayForced().value()) {
				return true;
			} else if (dialogsListFocused().value()) {
				return true;
			}
			return !App::main()->isSectionShown();
		};
		if (forceWideDialogs()) {
			dialogsWidth = st::dialogsWidthMin;
		} else {
			dialogsWidth = dialogsSmallColumnWidth();
		}
		chatWidth = bodyWidth - dialogsWidth;
	} else {
		layout = Adaptive::WindowLayout::Normal;
		accumulate_max(dialogsWidth, st::dialogsWidthMin);
		chatWidth = bodyWidth - dialogsWidth;
	}
	return {bodyWidth, dialogsWidth, chatWidth, layout};
}

bool Controller::canProvideChatWidth(int requestedWidth) const {
	auto currentLayout = computeColumnLayout();
	auto extendBy = requestedWidth - currentLayout.chatWidth;
	if (extendBy <= 0) {
		return true;
	}
	return window()->canExtendWidthBy(extendBy);
}

void Controller::provideChatWidth(int requestedWidth) {
	auto currentLayout = computeColumnLayout();
	auto extendBy = requestedWidth - currentLayout.chatWidth;
	if (extendBy <= 0) {
		return;
	}
	window()->tryToExtendWidthBy(extendBy);
	auto newLayout = computeColumnLayout();
	if (newLayout.windowLayout != Adaptive::WindowLayout::OneColumn) {
		dialogsWidthRatio().set(double(newLayout.bodyWidth - requestedWidth) / newLayout.bodyWidth, true);
	}
}

void Controller::showJumpToDate(not_null<PeerData *> peer, QDate requestedDate) {
	Expects(peer != nullptr);
	auto currentPeerDate = [peer] {
		if (auto history = App::historyLoaded(peer)) {
			if (history->scrollTopItem) {
				return history->scrollTopItem->date.date();
			} else if (history->loadedAtTop() && !history->isEmpty() && history->peer->migrateFrom()) {
				if (auto migrated = App::historyLoaded(history->peer->migrateFrom())) {
					if (migrated->scrollTopItem) {
						// We're up in the migrated history.
						// So current date is the date of first message here.
						return history->blocks.front()->items.front()->date.date();
					}
				}
			} else if (!history->lastMsgDate.isNull()) {
				return history->lastMsgDate.date();
			}
		}
		return QDate::currentDate();
	};
	auto maxPeerDate = [peer] {
		if (auto history = App::historyLoaded(peer)) {
			if (!history->lastMsgDate.isNull()) {
				return history->lastMsgDate.date();
			}
		}
		return QDate::currentDate();
	};
	auto minPeerDate = [peer] {
		if (auto history = App::historyLoaded(peer)) {
			if (history->loadedAtTop()) {
				if (history->isEmpty()) {
					return QDate::currentDate();
				}
				return history->blocks.front()->items.front()->date.date();
			}
		}
		return QDate(2013, 8, 1); // Telegram was launched in August 2013 :)
	};
	auto highlighted = requestedDate.isNull() ? currentPeerDate() : requestedDate;
	auto month = highlighted;
	auto box = Box<CalendarBox>(month, highlighted, [peer](const QDate &date) { Auth().api().jumpToDate(peer, date); });
	box->setMinDate(minPeerDate());
	box->setMaxDate(maxPeerDate());
	Ui::show(std::move(box));
}

} // namespace Window
