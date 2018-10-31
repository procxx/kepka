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
#include "dialogs/dialogs_list.h"

#include "dialogs/dialogs_layout.h"
#include "mainwidget.h"
#include "styles/style_dialogs.h"

namespace Dialogs {

List::List(SortMode sortMode)
    : _last(std::make_unique<Row>(nullptr, nullptr, nullptr, 0))
    , _begin(_last.get())
    , _end(_last.get())
    , _sortMode(sortMode)
    , _current(_last.get()) {}

void List::adjustCurrent(qint32 y, qint32 h) const {
	if (isEmpty()) return;

	qint32 pos = (y > 0) ? (y / h) : 0;
	while (_current->_pos > pos && _current != _begin) {
		_current = _current->_prev;
	}
	while (_current->_pos + 1 <= pos && _current->_next != _end) {
		_current = _current->_next;
	}
}

Row *List::addToEnd(History *history) {
	Row *result = new Row(history, _end->_prev, _end, _end->_pos);
	_end->_pos++;
	if (_begin == _end) {
		_begin = _current = result;
	} else {
		_end->_prev->_next = result;
	}
	_rowByPeer.insert(history->peer->id, result);
	++_count;
	_end->_prev = result;
	if (_sortMode == SortMode::Date) {
		adjustByPos(result);
	}
	return result;
}

bool List::insertBefore(Row *row, Row *before) {
	if (row == before) return false;

	if (_current == row) {
		_current = row->_prev;
	}

	Row *updateTill = row->_prev;
	remove(row);

	// insert row
	row->_next = before; // update row
	row->_prev = before->_prev;
	row->_next->_prev = row; // update row->next
	if (row->_prev) { // update row->prev
		row->_prev->_next = row;
	} else {
		_begin = row;
	}

	// update y
	for (Row *n = row; n != updateTill; n = n->_next) {
		n->_next->_pos++;
		row->_pos--;
	}
	return true;
}

bool List::insertAfter(Row *row, Row *after) {
	if (row == after) return false;

	if (_current == row) {
		_current = row->_next;
	}

	Row *updateFrom = row->_next;
	remove(row);

	// insert row
	row->_prev = after; // update row
	row->_next = after->_next;
	row->_prev->_next = row; // update row->prev
	row->_next->_prev = row; // update row->next

	// update y
	for (Row *n = updateFrom; n != row; n = n->_next) {
		n->_pos--;
		row->_pos++;
	}
	return true;
}

Row *List::adjustByName(const PeerData *peer) {
	if (_sortMode != SortMode::Name) return nullptr;

	auto i = _rowByPeer.find(peer->id);
	if (i == _rowByPeer.cend()) return nullptr;

	Row *row = i.value(), *change = row;
	while (change->_prev && change->_prev->history()->peer->name > peer->name) {
		change = change->_prev;
	}
	if (!insertBefore(row, change)) {
		while (change->_next != _end && change->_next->history()->peer->name < peer->name) {
			change = change->_next;
		}
		insertAfter(row, change);
	}
	return row;
}

Row *List::addByName(History *history) {
	if (_sortMode != SortMode::Name) return nullptr;

	Row *row = addToEnd(history), *change = row;
	const QString &peerName(history->peer->name);
	while (change->_prev && change->_prev->history()->peer->name.compare(peerName, Qt::CaseInsensitive) > 0) {
		change = change->_prev;
	}
	if (!insertBefore(row, change)) {
		while (change->_next != _end &&
		       change->_next->history()->peer->name.compare(peerName, Qt::CaseInsensitive) < 0) {
			change = change->_next;
		}
		insertAfter(row, change);
	}
	return row;
}

void List::adjustByPos(Row *row) {
	if (_sortMode != SortMode::Date || !_begin) return;

	Row *change = row;
	if (change != _begin && _begin->history()->sortKeyInChatList() < row->history()->sortKeyInChatList()) {
		change = _begin;
	} else {
		while (change->_prev && change->_prev->history()->sortKeyInChatList() < row->history()->sortKeyInChatList()) {
			change = change->_prev;
		}
	}
	if (!insertBefore(row, change)) {
		if (change->_next != _end &&
		    _end->_prev->history()->sortKeyInChatList() > row->history()->sortKeyInChatList()) {
			change = _end->_prev;
		} else {
			while (change->_next != _end &&
			       change->_next->history()->sortKeyInChatList() > row->history()->sortKeyInChatList()) {
				change = change->_next;
			}
		}
		insertAfter(row, change);
	}
}

bool List::moveToTop(PeerId peerId) {
	auto i = _rowByPeer.find(peerId);
	if (i == _rowByPeer.cend()) return false;

	insertBefore(i.value(), _begin);
	return true;
}

bool List::del(PeerId peerId, Row *replacedBy) {
	auto i = _rowByPeer.find(peerId);
	if (i == _rowByPeer.cend()) return false;

	auto row = i.value();
	if (App::main()) {
		emit App::main()->dialogRowReplaced(row, replacedBy);
	}

	if (row == _current) {
		_current = row->_next;
	}
	for (auto change = row->_next; change != _end; change = change->_next) {
		--change->_pos;
	}
	--_end->_pos;
	remove(row);
	delete row;
	--_count;
	_rowByPeer.erase(i);

	return true;
}

void List::remove(Row *row) {
	row->_next->_prev = row->_prev; // update row->next
	if (row->_prev) { // update row->prev
		row->_prev->_next = row->_next;
	} else {
		_begin = row->_next;
	}
}

void List::clear() {
	while (_begin != _end) {
		_current = _begin;
		_begin = _begin->_next;
		delete _current;
	}
	_current = _begin;
	_rowByPeer.clear();
	_count = 0;
}

List::~List() {
	clear();
}

} // namespace Dialogs
