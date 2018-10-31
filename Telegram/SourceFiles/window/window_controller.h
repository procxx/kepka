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

#include "base/flags.h"
#include "base/observer.h"
#include "core/basic_types.h"

#include "facades.h"

namespace Window {

enum class GifPauseReason {
	Any = 0,
	InlineResults = (1 << 0),
	SavedGifs = (1 << 1),
	Layer = (1 << 2),
	RoundPlaying = (1 << 3),
	MediaPreview = (1 << 4),
};
using GifPauseReasons = base::flags<GifPauseReason>;
inline constexpr bool is_flag_type(GifPauseReason) {
	return true;
};

class MainWindow;

class Controller {
public:
	static constexpr auto kDefaultDialogsWidthRatio = 5. / 14;

	Controller(not_null<MainWindow *> window)
	    : _window(window) {}

	not_null<MainWindow *> window() const {
		return _window;
	}

	// This is needed for History TopBar updating when searchInPeer
	// is changed in the DialogsWidget of the current window.
	base::Observable<PeerData *> &searchInPeerChanged() {
		return _searchInPeerChanged;
	}

	// This is needed while we have one HistoryWidget and one TopBarWidget
	// for all histories we show in a window. Once each history is shown
	// in its own HistoryWidget with its own TopBarWidget this can be removed.
	base::Observable<PeerData *> &historyPeerChanged() {
		return _historyPeerChanged;
	}

	void enableGifPauseReason(GifPauseReason reason);
	void disableGifPauseReason(GifPauseReason reason);
	base::Observable<void> &gifPauseLevelChanged() {
		return _gifPauseLevelChanged;
	}
	bool isGifPausedAtLeastFor(GifPauseReason reason) const;
	base::Observable<void> &floatPlayerAreaUpdated() {
		return _floatPlayerAreaUpdated;
	}

	struct ColumnLayout {
		int bodyWidth;
		int dialogsWidth;
		int chatWidth;
		Adaptive::WindowLayout windowLayout;
	};
	ColumnLayout computeColumnLayout() const;
	int dialogsSmallColumnWidth() const;
	bool canProvideChatWidth(int requestedWidth) const;
	void provideChatWidth(int requestedWidth);

	void showJumpToDate(not_null<PeerData *> peer, QDate requestedDate);

	base::Variable<double> &dialogsWidthRatio() {
		return _dialogsWidthRatio;
	}
	const base::Variable<double> &dialogsWidthRatio() const {
		return _dialogsWidthRatio;
	}
	base::Variable<bool> &dialogsListFocused() {
		return _dialogsListFocused;
	}
	const base::Variable<bool> &dialogsListFocused() const {
		return _dialogsListFocused;
	}
	base::Variable<bool> &dialogsListDisplayForced() {
		return _dialogsListDisplayForced;
	}
	const base::Variable<bool> &dialogsListDisplayForced() const {
		return _dialogsListDisplayForced;
	}

private:
	not_null<MainWindow *> _window;

	base::Observable<PeerData *> _searchInPeerChanged;
	base::Observable<PeerData *> _historyPeerChanged;

	GifPauseReasons _gifPauseReasons = 0;
	base::Observable<void> _gifPauseLevelChanged;
	base::Observable<void> _floatPlayerAreaUpdated;

	base::Variable<double> _dialogsWidthRatio = {kDefaultDialogsWidthRatio};
	base::Variable<bool> _dialogsListFocused = {false};
	base::Variable<bool> _dialogsListDisplayForced = {false};
};

} // namespace Window
