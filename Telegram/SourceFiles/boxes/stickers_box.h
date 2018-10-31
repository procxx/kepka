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

#include "base/timer.h"
#include "boxes/abstract_box.h"
#include "mtproto/sender.h"

class ConfirmBox;

namespace style {
struct RippleAnimation;
} // namespace style

namespace Ui {
class PlainShadow;
class RippleAnimation;
class SettingsSlider;
class SlideAnimation;
class UsernameInput;
class CrossButton;
} // namespace Ui

class StickersBox : public BoxContent, public RPCSender {
public:
	enum class Section {
		Installed,
		Featured,
		Archived,
	};
	StickersBox(QWidget *, Section section);
	StickersBox(QWidget *, not_null<ChannelData *> megagroup);

	void setInnerFocus() override;

	~StickersBox();

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	class Inner;
	class Tab {
	public:
		Tab() = default;

		template <typename... Args> Tab(int index, Args &&... args);

		object_ptr<Inner> takeWidget();
		void returnWidget(object_ptr<Inner> widget);

		Inner *widget() {
			return _weak;
		}
		int index() const {
			return _index;
		}

		void saveScrollTop();
		int getScrollTop() const {
			return _scrollTop;
		}

	private:
		int _index = 0;
		object_ptr<Inner> _widget = {nullptr};
		QPointer<Inner> _weak;
		int _scrollTop = 0;
	};

	void handleStickersUpdated();
	void refreshTabs();
	void rebuildList(Tab *tab = nullptr);
	void updateTabsGeometry();
	void switchTab();
	void installSet(quint64 setId);
	int getTopSkip() const;
	void saveChanges();

	QPixmap grabContentCache();

	void installDone(const MTPmessages_StickerSetInstallResult &result);
	bool installFail(quint64 setId, const RPCError &error);

	void preloadArchivedSets();
	void requestArchivedSets();
	void loadMoreArchived();
	void getArchivedDone(quint64 offsetId, const MTPmessages_ArchivedStickers &result);

	object_ptr<Ui::SettingsSlider> _tabs = {nullptr};
	QList<Section> _tabIndices;

	class CounterWidget;
	object_ptr<CounterWidget> _unreadBadge = {nullptr};

	Section _section;

	Tab _installed;
	Tab _featured;
	Tab _archived;
	Tab *_tab = nullptr;

	ChannelData *_megagroupSet = nullptr;

	std::unique_ptr<Ui::SlideAnimation> _slideAnimation;
	object_ptr<BoxLayerTitleShadow> _titleShadow = {nullptr};

	mtpRequestId _archivedRequestId = 0;
	bool _archivedLoaded = false;
	bool _allArchivedLoaded = false;
	bool _someArchivedLoaded = false;

	Stickers::Order _localOrder;
	Stickers::Order _localRemoved;
};

int stickerPacksCount(bool includeArchivedOfficial = false);

// This class is hold in header because it requires Qt preprocessing.
class StickersBox::Inner : public TWidget, private base::Subscriber, private MTP::Sender {
	Q_OBJECT

public:
	using Section = StickersBox::Section;
	Inner(QWidget *parent, Section section);
	Inner(QWidget *parent, not_null<ChannelData *> megagroup);

	base::Observable<int> scrollToY;
	void setInnerFocus();

	void saveGroupSet();

	void rebuild();
	void updateSize(int newWidth = 0);
	void updateRows(); // refresh only pack cover stickers
	bool appendSet(const Stickers::Set &set);

	Stickers::Order getOrder() const;
	Stickers::Order getFullOrder() const;
	Stickers::Order getRemovedSets() const;

	void setFullOrder(const Stickers::Order &order);
	void setRemovedSets(const Stickers::Order &removed);

	void setInstallSetCallback(Fn<void(quint64 setId)> callback) {
		_installSetCallback = std::move(callback);
	}
	void setLoadMoreCallback(Fn<void()> callback) {
		_loadMoreCallback = std::move(callback);
	}

	void setVisibleTopBottom(int visibleTop, int visibleBottom) override;
	void setMinHeight(int newWidth, int minHeight);

	int getVisibleTop() const {
		return _visibleTop;
	}

	~Inner();

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void leaveToChildEvent(QEvent *e, QWidget *child) override;

signals:
	void draggingScrollDelta(int delta);

public slots:
	void onUpdateSelected();

private:
	struct Row {
		Row(quint64 id, DocumentData *sticker, qint32 count, const QString &title, int titleWidth, bool installed,
		    bool official, bool unread, bool archived, bool removed, qint32 pixw, qint32 pixh);
		bool isRecentSet() const {
			return (id == Stickers::CloudRecentSetId);
		}
		~Row();

		quint64 id = 0;
		DocumentData *sticker = nullptr;
		qint32 count = 0;
		QString title;
		int titleWidth = 0;
		bool installed = false;
		bool official = false;
		bool unread = false;
		bool archived = false;
		bool removed = false;
		qint32 pixw = 0;
		qint32 pixh = 0;
		anim::value yadd;
		std::unique_ptr<Ui::RippleAnimation> ripple;
	};

	template <typename Check> Stickers::Order collectSets(Check check) const;

	void checkLoadMore();
	void updateScrollbarWidth();
	int getRowIndex(quint64 setId) const;
	void setRowRemoved(int index, bool removed);

	void setSelected(int selected);
	void setActionDown(int newActionDown);
	void setPressed(int pressed);
	void setup();
	QRect relativeButtonRect(bool removeButton) const;
	void ensureRipple(const style::RippleAnimation &st, QImage mask, bool removeButton);

	void step_shifting(TimeMs ms, bool timer);
	void paintRow(Painter &p, Row *set, int index, TimeMs ms);
	void paintFakeButton(Painter &p, Row *set, int index, TimeMs ms);
	void clear();
	void setActionSel(qint32 actionSel);
	double aboveShadowOpacity() const;

	void readVisibleSets();

	void updateControlsGeometry();
	void rebuildAppendSet(const Stickers::Set &set, int maxNameWidth);
	void fillSetCover(const Stickers::Set &set, DocumentData **outSticker, int *outWidth, int *outHeight) const;
	int fillSetCount(const Stickers::Set &set) const;
	QString fillSetTitle(const Stickers::Set &set, int maxNameWidth, int *outTitleWidth) const;
	void fillSetFlags(const Stickers::Set &set, bool *outInstalled, bool *outOfficial, bool *outUnread,
	                  bool *outArchived);
	void rebuildMegagroupSet();
	void handleMegagroupSetAddressChange();
	void setMegagroupSelectedSet(const MTPInputStickerSet &set);

	int countMaxNameWidth() const;

	Section _section;

	qint32 _rowHeight;

	std::vector<std::unique_ptr<Row>> _rows;
	QList<TimeMs> _animStartTimes;
	TimeMs _aboveShadowFadeStart = 0;
	anim::value _aboveShadowFadeOpacity;
	BasicAnimation _a_shifting;

	Fn<void(quint64 setId)> _installSetCallback;
	Fn<void()> _loadMoreCallback;

	int _visibleTop = 0;
	int _visibleBottom = 0;
	int _itemsTop = 0;

	int _actionSel = -1;
	int _actionDown = -1;

	QString _addText;
	int _addWidth = 0;
	QString _undoText;
	int _undoWidth = 0;

	int _buttonHeight = 0;

	QPoint _mouse;
	bool _inDragArea = false;
	int _selected = -1;
	int _pressed = -1;
	QPoint _dragStart;
	int _started = -1;
	int _dragging = -1;
	int _above = -1;

	int _minHeight = 0;

	int _scrollbar = 0;
	ChannelData *_megagroupSet = nullptr;
	MTPInputStickerSet _megagroupSetInput = MTP_inputStickerSetEmpty();
	std::unique_ptr<Row> _megagroupSelectedSet;
	object_ptr<Ui::UsernameInput> _megagroupSetField = {nullptr};
	object_ptr<BoxLayerTitleShadow> _megagroupSelectedShadow = {nullptr};
	object_ptr<Ui::CrossButton> _megagroupSelectedRemove = {nullptr};
	object_ptr<BoxContentDivider> _megagroupDivider = {nullptr};
	object_ptr<Ui::FlatLabel> _megagroupSubTitle = {nullptr};
	base::Timer _megagroupSetAddressChangedTimer;
	mtpRequestId _megagroupSetRequestId = 0;
};
