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

#include "profile/profile_block_widget.h"
#include "styles/style_profile.h"
#include "ui/text/text.h"

namespace Ui {
class RippleAnimation;
class PopupMenu;
} // namespace Ui

namespace Notify {
struct PeerUpdate;
} // namespace Notify

namespace Profile {

class PeerListWidget : public BlockWidget {
public:
	PeerListWidget(QWidget *parent, PeerData *peer, const QString &title,
	               const style::ProfilePeerListItem &st = st::profileMemberItem, const QString &removeText = QString());

	void setVisibleTopBottom(int visibleTop, int visibleBottom) override;

	struct Item {
		explicit Item(PeerData *peer);
		~Item();

		PeerData *const peer;
		Text name;
		QString statusText;
		bool statusHasOnlineColor = false;
		enum class AdminState {
			None,
			Admin,
			Creator,
		};
		AdminState adminState = AdminState::None;
		bool hasRemoveLink = false;
		std::unique_ptr<Ui::RippleAnimation> ripple;
	};
	virtual int getListTop() const {
		return contentTop();
	}

	int getListLeft() const;

	const QList<Item *> &items() const {
		return _items;
	}
	int itemsCount() const {
		return _items.size();
	}

	// Does not take ownership of item.
	void addItem(Item *item) {
		if (!item) return;
		_items.push_back(item);
	}
	void clearItems() {
		_items.clear();
	}
	void reserveItemsForSize(int size) {
		_items.reserve(size);
	}
	template <typename Predicate> void sortItems(Predicate predicate) {
		qSort(_items.begin(), _items.end(), std::move(predicate));
	}

	void setPreloadMoreCallback(Fn<void()> callback) {
		_preloadMoreCallback = std::move(callback);
	}
	void setSelectedCallback(Fn<void(PeerData *)> callback) {
		_selectedCallback = std::move(callback);
	}
	void setRemovedCallback(Fn<void(PeerData *)> callback) {
		_removedCallback = std::move(callback);
	}
	void setUpdateItemCallback(Fn<void(Item *)> callback) {
		_updateItemCallback = std::move(callback);
	}

protected:
	void paintOutlinedRect(Painter &p, int x, int y, int w, int h) const;
	void refreshVisibility();

	// Resizes content and counts natural widget height for the desired width.
	int resizeGetHeight(int newWidth) override;

	void paintContents(Painter &p) override;

	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
	void enterEventHook(QEvent *e) override;
	void enterFromChildEvent(QEvent *e, QWidget *child) override {
		enterEventHook(e);
	}
	void leaveEventHook(QEvent *e) override;
	void leaveToChildEvent(QEvent *e, QWidget *child) override {
		leaveEventHook(e);
	}

	virtual Ui::PopupMenu *fillPeerMenu(PeerData *peer) {
		return nullptr;
	}

private:
	void mousePressReleased(Qt::MouseButton button);
	void updateSelection();
	void setSelected(int selected, bool selectedRemove);
	void repaintSelectedRow();
	void repaintRow(int index);
	void preloadPhotos();
	int rowWidth() const;

	void paintItem(Painter &p, int x, int y, Item *item, bool selected, bool selectedRemove, TimeMs ms);

	const style::ProfilePeerListItem &_st;

	Fn<void()> _preloadMoreCallback;
	Fn<void(PeerData *)> _selectedCallback;
	Fn<void(PeerData *)> _removedCallback;
	Fn<void(Item *)> _updateItemCallback;

	QList<Item *> _items;

	int _visibleTop = 0;
	int _visibleBottom = 0;

	int _selected = -1;
	int _pressed = -1;
	Qt::MouseButton _pressButton = Qt::LeftButton;
	bool _selectedRemove = false;
	bool _pressedRemove = false;
	QPoint _mousePosition;

	QString _removeText;
	int _removeWidth = 0;

	Ui::PopupMenu *_menu = nullptr;
	int _menuRowIndex = -1;
};

} // namespace Profile
