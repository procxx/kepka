/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

#include "ui/twidget.h"
#include "ui/abstract_button.h"
#include "ui/effects/panel_animation.h"
#include "mtproto/sender.h"
#include "inline_bots/inline_bot_layout_item.h"

namespace Ui {
class ScrollArea;
class IconButton;
class LinkButton;
class RoundButton;
class FlatLabel;
class RippleAnimation;
} // namesapce Ui

namespace Window {
class Controller;
} // namespace Window

namespace InlineBots {

class Result;

namespace Layout {

class ItemBase;

namespace internal {

constexpr int kInlineItemsMaxPerRow = 5;

using Results = std::vector<std::unique_ptr<Result>>;

struct CacheEntry {
	QString nextOffset;
	QString switchPmText, switchPmStartToken;
	Results results;
};

class Inner : public TWidget, public Context, private base::Subscriber {
	Q_OBJECT

public:
	Inner(QWidget *parent, not_null<Window::Controller*> controller);

	void hideFinish(bool completely);

	void clearSelection();

	int refreshInlineRows(PeerData *queryPeer, UserData *bot, const CacheEntry *results, bool resultsDeleted);
	void inlineBotChanged();
	void hideInlineRowsPanel();
	void clearInlineRowsPanel();

	void setVisibleTopBottom(int visibleTop, int visibleBottom) override;
	void preloadImages();

	void inlineItemLayoutChanged(const ItemBase *layout) override;
	void inlineItemRepaint(const ItemBase *layout) override;
	bool inlineItemVisible(const ItemBase *layout) override;

	int countHeight();

	void setResultSelectedCallback(base::lambda<void(Result *result, UserData *bot)> callback) {
		_resultSelectedCallback = std::move(callback);
	}

	~Inner();

protected:
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void leaveToChildEvent(QEvent *e, QWidget *child) override;
	void enterFromChildEvent(QEvent *e, QWidget *child) override;

private slots:
	void onPreview();
	void onUpdateInlineItems();
	void onSwitchPm();

signals:
	void emptyInlineRows();

private:
	static constexpr bool kRefreshIconsScrollAnimation = true;
	static constexpr bool kRefreshIconsNoAnimation = false;

	void updateSelected();
	void checkRestrictedPeer();
	bool isRestrictedView();

	void paintInlineItems(Painter &p, const QRect &r);

	void refreshSwitchPmButton(const CacheEntry *entry);

	not_null<Window::Controller*> _controller;

	int _visibleTop = 0;
	int _visibleBottom = 0;

	UserData *_inlineBot = nullptr;
	PeerData *_inlineQueryPeer = nullptr;
	TimeMs _lastScrolled = 0;
	QTimer _updateInlineItems;
	bool _inlineWithThumb = false;

	object_ptr<Ui::RoundButton> _switchPmButton = { nullptr };
	QString _switchPmStartToken;

	object_ptr<Ui::FlatLabel> _restrictedLabel = { nullptr };

	struct Row {
		int height = 0;
		QVector<ItemBase*> items;
	};
	QVector<Row> _rows;
	void clearInlineRows(bool resultsDeleted);

	std::map<Result*, std::unique_ptr<ItemBase>> _inlineLayouts;
	ItemBase *layoutPrepareInlineResult(Result *result, int32_t position);

	bool inlineRowsAddItem(Result *result, Row &row, int32_t &sumWidth);
	bool inlineRowFinalize(Row &row, int32_t &sumWidth, bool force = false);

	Row &layoutInlineRow(Row &row, int32_t sumWidth = 0);
	void deleteUnusedInlineLayouts();

	int validateExistingInlineRows(const Results &results);
	void selectInlineResult(int row, int column);

	int _selected = -1;
	int _pressed = -1;
	QPoint _lastMousePos;

	QTimer _previewTimer;
	bool _previewShown = false;

	base::lambda<void(Result *result, UserData *bot)> _resultSelectedCallback;

};

} // namespace internal

class Widget : public TWidget, private MTP::Sender {
	Q_OBJECT

public:
	Widget(QWidget *parent, not_null<Window::Controller*> controller);

	void moveBottom(int bottom);

	void hideFast();
	bool hiding() const {
		return _hiding;
	}

	void queryInlineBot(UserData *bot, PeerData *peer, QString query);
	void clearInlineBot();

	bool overlaps(const QRect &globalRect) const;

	void showAnimated();
	void hideAnimated();

	void setResultSelectedCallback(base::lambda<void(Result *result, UserData *bot)> callback) {
		_inner->setResultSelectedCallback(std::move(callback));
	}

	~Widget();

protected:
	void paintEvent(QPaintEvent *e) override;

private slots:
	void onWndActiveChanged();

	void onScroll();

	void onInlineRequest();
	void onEmptyInlineRows();

private:
	void moveByBottom();
	void paintContent(Painter &p);

	style::margins innerPadding() const;

	// Rounded rect which has shadow around it.
	QRect innerRect() const;

	// Inner rect with removed st::buttonRadius from top and bottom.
	// This one is allowed to be not rounded.
	QRect horizontalRect() const;

	// Inner rect with removed st::buttonRadius from left and right.
	// This one is allowed to be not rounded.
	QRect verticalRect() const;

	QImage grabForPanelAnimation();
	void startShowAnimation();
	void startOpacityAnimation(bool hiding);
	void prepareCache();

	class Container;
	void opacityAnimationCallback();

	void hideFinished();
	void showStarted();

	void updateContentHeight();

	void inlineBotChanged();
	int showInlineRows(bool newResults);
	void recountContentMaxHeight();
	bool refreshInlineRows(int *added = nullptr);
	void inlineResultsDone(const MTPmessages_BotResults &result);

	not_null<Window::Controller*> _controller;

	int _contentMaxHeight = 0;
	int _contentHeight = 0;
	bool _horizontal = false;

	int _width = 0;
	int _height = 0;
	int _bottom = 0;

	std::unique_ptr<Ui::PanelAnimation> _showAnimation;
	Animation _a_show;

	bool _hiding = false;
	QPixmap _cache;
	Animation _a_opacity;
	bool _inPanelGrab = false;

	object_ptr<Ui::ScrollArea> _scroll;
	QPointer<internal::Inner> _inner;

	std::map<QString, std::unique_ptr<internal::CacheEntry>> _inlineCache;
	QTimer _inlineRequestTimer;

	UserData *_inlineBot = nullptr;
	PeerData *_inlineQueryPeer = nullptr;
	QString _inlineQuery, _inlineNextQuery, _inlineNextOffset;
	mtpRequestId _inlineRequestId = 0;

};

} // namespace Layout
} // namespace InlineBots
