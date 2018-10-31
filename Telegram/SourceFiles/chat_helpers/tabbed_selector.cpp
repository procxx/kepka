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
#include "chat_helpers/tabbed_selector.h"

#include "apiwrap.h"
#include "app.h"
#include "chat_helpers/emoji_list_widget.h"
#include "chat_helpers/gifs_list_widget.h"
#include "chat_helpers/stickers.h"
#include "chat_helpers/stickers_list_widget.h"
#include "lang/lang_keys.h"
#include "mainwindow.h"
#include "observer_peer.h"
#include "storage/localstorage.h"
#include "styles/style_chat_helpers.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"

namespace ChatHelpers {
namespace {

constexpr auto kSaveChosenTabTimeout = 1000;

} // namespace

class TabbedSelector::SlideAnimation : public Ui::RoundShadowAnimation {
public:
	enum class Direction {
		LeftToRight,
		RightToLeft,
	};
	void setFinalImages(Direction direction, QImage &&left, QImage &&right, QRect inner, bool wasSectionIcons);

	void start();
	void paintFrame(QPainter &p, double dt, double opacity);

private:
	Direction _direction = Direction::LeftToRight;
	QPixmap _leftImage, _rightImage;
	int _width = 0;
	int _height = 0;
	int _innerLeft = 0;
	int _innerTop = 0;
	int _innerRight = 0;
	int _innerBottom = 0;
	int _innerWidth = 0;
	int _innerHeight = 0;

	int _painterInnerLeft = 0;
	int _painterInnerTop = 0;
	int _painterInnerWidth = 0;
	int _painterInnerBottom = 0;
	int _painterCategoriesTop = 0;
	int _painterInnerHeight = 0;
	int _painterInnerRight = 0;

	int _frameIntsPerLineAdd = 0;
	bool _wasSectionIcons = false;
};

void TabbedSelector::SlideAnimation::setFinalImages(Direction direction, QImage &&left, QImage &&right, QRect inner,
                                                    bool wasSectionIcons) {
	Expects(!started());
	_direction = direction;
	_leftImage =
	    QPixmap::fromImage(std::move(left).convertToFormat(QImage::Format_ARGB32_Premultiplied), Qt::ColorOnly);
	_rightImage =
	    QPixmap::fromImage(std::move(right).convertToFormat(QImage::Format_ARGB32_Premultiplied), Qt::ColorOnly);

	Assert(!_leftImage.isNull());
	Assert(!_rightImage.isNull());
	_width = _leftImage.width();
	_height = _rightImage.height();
	Assert(!(_width % cIntRetinaFactor()));
	Assert(!(_height % cIntRetinaFactor()));
	Assert(_leftImage.devicePixelRatio() == _rightImage.devicePixelRatio());
	Assert(_rightImage.width() == _width);
	Assert(_rightImage.height() == _height);
	Assert(QRect(0, 0, _width, _height).contains(inner));
	_innerLeft = inner.x();
	_innerTop = inner.y();
	_innerWidth = inner.width();
	_innerHeight = inner.height();
	Assert(!(_innerLeft % cIntRetinaFactor()));
	Assert(!(_innerTop % cIntRetinaFactor()));
	Assert(!(_innerWidth % cIntRetinaFactor()));
	Assert(!(_innerHeight % cIntRetinaFactor()));
	_innerRight = _innerLeft + _innerWidth;
	_innerBottom = _innerTop + _innerHeight;

	_painterInnerLeft = _innerLeft / cIntRetinaFactor();
	_painterInnerTop = _innerTop / cIntRetinaFactor();
	_painterInnerRight = _innerRight / cIntRetinaFactor();
	_painterInnerBottom = _innerBottom / cIntRetinaFactor();
	_painterInnerWidth = _innerWidth / cIntRetinaFactor();
	_painterInnerHeight = _innerHeight / cIntRetinaFactor();
	_painterCategoriesTop = _painterInnerBottom - st::emojiCategory.height;

	_wasSectionIcons = wasSectionIcons;
}

void TabbedSelector::SlideAnimation::start() {
	Assert(!_leftImage.isNull());
	Assert(!_rightImage.isNull());
	RoundShadowAnimation::start(_width, _height, _leftImage.devicePixelRatio());
	auto checkCorner = [this](const Corner &corner) {
		if (!corner.valid()) return;
		Assert(corner.width <= _innerWidth);
		Assert(corner.height <= _innerHeight);
	};
	checkCorner(_topLeft);
	checkCorner(_topRight);
	checkCorner(_bottomLeft);
	checkCorner(_bottomRight);
	_frameIntsPerLineAdd = (_width - _innerWidth) + _frameIntsPerLineAdded;
}

void TabbedSelector::SlideAnimation::paintFrame(QPainter &p, double dt, double opacity) {
	Expects(started());
	Expects(dt >= 0.);

	_frameAlpha = anim::interpolate(1, 256, opacity);

	auto leftToRight = (_direction == Direction::LeftToRight);

	auto easeOut = anim::easeOutCirc(1., dt);
	auto easeIn = anim::easeInCirc(1., dt);

	auto arrivingCoord = anim::interpolate(_innerWidth, 0, easeOut);
	auto departingCoord = anim::interpolate(0, _innerWidth, easeIn);
	if (auto decrease = (arrivingCoord % cIntRetinaFactor())) {
		arrivingCoord -= decrease;
	}
	if (auto decrease = (departingCoord % cIntRetinaFactor())) {
		departingCoord -= decrease;
	}
	auto arrivingAlpha = easeIn;
	auto departingAlpha = 1. - easeOut;
	auto leftCoord = (leftToRight ? arrivingCoord : departingCoord) * -1;
	auto leftAlpha = (leftToRight ? arrivingAlpha : departingAlpha);
	auto rightCoord = (leftToRight ? departingCoord : arrivingCoord);
	auto rightAlpha = (leftToRight ? departingAlpha : arrivingAlpha);

	// _innerLeft ..(left).. leftTo ..(both).. bothTo ..(none).. noneTo ..(right).. _innerRight
	auto leftTo = _innerLeft + snap(_innerWidth + leftCoord, 0, _innerWidth);
	auto rightFrom = _innerLeft + snap(rightCoord, 0, _innerWidth);
	auto painterRightFrom = rightFrom / cIntRetinaFactor();
	if (opacity < 1.) {
		_frame.fill(Qt::transparent);
	}
	{
		Painter p(&_frame);
		p.setOpacity(opacity);
		p.fillRect(_painterInnerLeft, _painterInnerTop, _painterInnerWidth, _painterCategoriesTop - _painterInnerTop,
		           st::emojiPanBg);
		p.fillRect(_painterInnerLeft, _painterCategoriesTop, _painterInnerWidth,
		           _painterInnerBottom - _painterCategoriesTop,
		           _wasSectionIcons ? st::emojiPanCategories : st::emojiPanBg);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		if (leftTo > _innerLeft) {
			p.setOpacity(opacity * leftAlpha);
			p.drawPixmap(_painterInnerLeft, _painterInnerTop, _leftImage, _innerLeft - leftCoord, _innerTop,
			             leftTo - _innerLeft, _innerHeight);
		}
		if (rightFrom < _innerRight) {
			p.setOpacity(opacity * rightAlpha);
			p.drawPixmap(painterRightFrom, _painterInnerTop, _rightImage, _innerLeft, _innerTop,
			             _innerRight - rightFrom, _innerHeight);
		}
	}

	// Draw corners
	// paintCorner(_topLeft, _innerLeft, _innerTop);
	// paintCorner(_topRight, _innerRight - _topRight.width, _innerTop);
	paintCorner(_bottomLeft, _innerLeft, _innerBottom - _bottomLeft.height);
	paintCorner(_bottomRight, _innerRight - _bottomRight.width, _innerBottom - _bottomRight.height);

	// Draw shadow upon the transparent
	auto outerLeft = _innerLeft;
	auto outerTop = _innerTop;
	auto outerRight = _innerRight;
	auto outerBottom = _innerBottom;
	if (_shadow.valid()) {
		outerLeft -= _shadow.extend.left();
		outerTop -= _shadow.extend.top();
		outerRight += _shadow.extend.right();
		outerBottom += _shadow.extend.bottom();
	}
	if (cIntRetinaFactor() > 1) {
		if (auto skipLeft = (outerLeft % cIntRetinaFactor())) {
			outerLeft -= skipLeft;
		}
		if (auto skipTop = (outerTop % cIntRetinaFactor())) {
			outerTop -= skipTop;
		}
		if (auto skipRight = (outerRight % cIntRetinaFactor())) {
			outerRight += (cIntRetinaFactor() - skipRight);
		}
		if (auto skipBottom = (outerBottom % cIntRetinaFactor())) {
			outerBottom += (cIntRetinaFactor() - skipBottom);
		}
	}

	if (opacity == 1.) {
		// Fill above the frame top with transparent.
		auto fillTopInts = (_frameInts + outerTop * _frameIntsPerLine + outerLeft);
		auto fillWidth = (outerRight - outerLeft) * sizeof(quint32);
		for (auto fillTop = _innerTop - outerTop; fillTop != 0; --fillTop) {
			memset(fillTopInts, 0, fillWidth);
			fillTopInts += _frameIntsPerLine;
		}

		// Fill to the left and to the right of the frame with transparent.
		auto fillLeft = (_innerLeft - outerLeft) * sizeof(quint32);
		auto fillRight = (outerRight - _innerRight) * sizeof(quint32);
		if (fillLeft || fillRight) {
			auto fillInts = _frameInts + _innerTop * _frameIntsPerLine;
			for (auto y = _innerTop; y != _innerBottom; ++y) {
				memset(fillInts + outerLeft, 0, fillLeft);
				memset(fillInts + _innerRight, 0, fillRight);
				fillInts += _frameIntsPerLine;
			}
		}

		// Fill below the frame bottom with transparent.
		auto fillBottomInts = (_frameInts + _innerBottom * _frameIntsPerLine + outerLeft);
		for (auto fillBottom = outerBottom - _innerBottom; fillBottom != 0; --fillBottom) {
			memset(fillBottomInts, 0, fillWidth);
			fillBottomInts += _frameIntsPerLine;
		}
	}
	if (_shadow.valid()) {
		paintShadow(outerLeft, outerTop, outerRight, outerBottom);
	}

	// Debug
	//	frameInts = _frameInts;
	//	auto pattern = anim::shifted((static_cast<quint32>(0xFF) << 24) | (static_cast<quint32>(0xFF) << 16) |
	//	                             (static_cast<quint32>(0xFF) << 8) | static_cast<quint32>(0xFF));
	//	for (auto y = 0; y != _finalHeight; ++y) {
	//		for (auto x = 0; x != _finalWidth; ++x) {
	//			auto source = *frameInts;
	//			auto sourceAlpha = (source >> 24);
	//			*frameInts = anim::unshifted(anim::shifted(source) * 256 + pattern * (256 - sourceAlpha));
	//			++frameInts;
	//		}
	//		frameInts += _frameIntsPerLineAdded;
	//	}

	p.drawImage(outerLeft / cIntRetinaFactor(), outerTop / cIntRetinaFactor(), _frame, outerLeft, outerTop,
	            outerRight - outerLeft, outerBottom - outerTop);
}

TabbedSelector::Tab::Tab(SelectorTab type, object_ptr<Inner> widget)
    : _type(type)
    , _widget(std::move(widget))
    , _weak(_widget)
    , _footer(_widget->createFooter()) {
	_footer->setParent(_widget->parentWidget());
}

object_ptr<TabbedSelector::Inner> TabbedSelector::Tab::takeWidget() {
	return std::move(_widget);
}

void TabbedSelector::Tab::returnWidget(object_ptr<Inner> widget) {
	_widget = std::move(widget);
	Ensures(_widget == _weak);
}

void TabbedSelector::Tab::saveScrollTop() {
	_scrollTop = widget()->getVisibleTop();
}

TabbedSelector::TabbedSelector(QWidget *parent, not_null<Window::Controller *> controller)
    : TWidget(parent)
    , _tabsSlider(this, st::emojiTabs)
    , _topShadow(this, st::shadowFg)
    , _bottomShadow(this, st::shadowFg)
    , _scroll(this, st::emojiScroll)
    , _tabs{{
          Tab{SelectorTab::Emoji, object_ptr<EmojiListWidget>(this, controller)},
          Tab{SelectorTab::Stickers, object_ptr<StickersListWidget>(this, controller)},
          Tab{SelectorTab::Gifs, object_ptr<GifsListWidget>(this, controller)},
      }}
    , _currentTabType(Auth().data().selectorTab()) {
	resize(st::emojiPanWidth, st::emojiPanMaxHeight);

	for (auto &tab : _tabs) {
		tab.footer()->hide();
		tab.widget()->hide();
	}

	createTabsSlider();

	_scroll->setGeometryToLeft(st::buttonRadius, marginTop(), st::emojiPanWidth - st::buttonRadius,
	                           height() - marginTop() - marginBottom());
	setWidgetToScrollArea();

	_bottomShadow->setGeometry(_tabsSlider->x(), _scroll->y() + _scroll->height() - st::lineWidth, _tabsSlider->width(),
	                           st::lineWidth);

	for (auto &tab : _tabs) {
		auto widget = tab.widget();
		connect(widget, &Inner::scrollToY, this, [this, tab = &tab](int y) {
			if (tab == currentTab()) {
				scrollToY(y);
			} else {
				tab->saveScrollTop(y);
			}
		});
		connect(widget, &Inner::disableScroll, this, [this, tab = &tab](bool disabled) {
			if (tab == currentTab()) {
				_scroll->disableScroll(disabled);
			}
		});
	}

	connect(stickers(), SIGNAL(scrollUpdated()), this, SLOT(onScroll()));
	connect(_scroll, SIGNAL(scrolled()), this, SLOT(onScroll()));
	connect(emoji(), SIGNAL(selected(EmojiPtr)), this, SIGNAL(emojiSelected(EmojiPtr)));
	connect(stickers(), SIGNAL(selected(DocumentData *)), this, SIGNAL(stickerSelected(DocumentData *)));
	connect(stickers(), SIGNAL(checkForHide()), this, SIGNAL(checkForHide()));
	connect(gifs(), SIGNAL(selected(DocumentData *)), this, SIGNAL(stickerSelected(DocumentData *)));
	connect(gifs(), SIGNAL(selected(PhotoData *)), this, SIGNAL(photoSelected(PhotoData *)));
	connect(gifs(), SIGNAL(selected(InlineBots::Result *, UserData *)), this,
	        SIGNAL(inlineResultSelected(InlineBots::Result *, UserData *)));
	connect(gifs(), SIGNAL(cancelled()), this, SIGNAL(cancelled()));

	_topShadow->raise();
	_bottomShadow->raise();
	_tabsSlider->raise();

	subscribe(Notify::PeerUpdated(), Notify::PeerUpdatedHandler(Notify::PeerUpdate::Flag::ChannelRightsChanged,
	                                                            [this](const Notify::PeerUpdate &update) {
		                                                            if (update.peer == _currentPeer) {
			                                                            checkRestrictedPeer();
		                                                            }
	                                                            }));

	//	setAttribute(Qt::WA_AcceptTouchEvents);
	setAttribute(Qt::WA_OpaquePaintEvent, false);
	showAll();
}

void TabbedSelector::resizeEvent(QResizeEvent *e) {
	auto contentHeight = height() - marginTop() - marginBottom();
	if (e->oldSize().height() > height()) {
		_scroll->resize(_scroll->width(), contentHeight);
		auto scrollTop = _scroll->scrollTop();
		currentTab()->widget()->setVisibleTopBottom(scrollTop, scrollTop + contentHeight);
	} else {
		auto scrollTop = _scroll->scrollTop();
		currentTab()->widget()->setVisibleTopBottom(scrollTop, scrollTop + contentHeight);
		_scroll->resize(_scroll->width(), contentHeight);
	}
	_bottomShadow->setGeometry(_tabsSlider->x(), _scroll->y() + _scroll->height() - st::lineWidth, _tabsSlider->width(),
	                           st::lineWidth);
	if (_restrictedLabel) {
		_restrictedLabel->move((width() - _restrictedLabel->width()), (height() / 3 - _restrictedLabel->height() / 2));
	}

	_footerTop = height() - st::emojiCategory.height;
	for (auto &tab : _tabs) {
		tab.footer()->move(_tabsSlider->x(), _footerTop);
	}

	update();
}

void TabbedSelector::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();

	auto switching = (_slideAnimation != nullptr);
	if (switching) {
		paintSlideFrame(p, ms);
		if (!_a_slide.animating()) {
			_slideAnimation.reset();
			afterShown();
			emit slideFinished();
		}
	} else {
		paintContent(p);
	}
}

void TabbedSelector::paintSlideFrame(Painter &p, TimeMs ms) {
	if (_roundRadius > 0) {
		auto topPart = QRect(0, 0, width(), _tabsSlider->height() + _roundRadius);
		App::roundRect(p, topPart, st::emojiPanBg, ImageRoundRadius::Small, RectPart::FullTop | RectPart::NoTopBottom);
	} else {
		p.fillRect(0, 0, width(), _tabsSlider->height(), st::emojiPanBg);
	}

	auto slideDt = _a_slide.current(ms, 1.);
	_slideAnimation->paintFrame(p, slideDt, 1.);
}

void TabbedSelector::paintContent(Painter &p) {
	auto &bottomBg = hasSectionIcons() ? st::emojiPanCategories : st::emojiPanBg;
	if (_roundRadius > 0) {
		auto topPart = QRect(0, 0, width(), _tabsSlider->height() + _roundRadius);
		App::roundRect(p, topPart, st::emojiPanBg, ImageRoundRadius::Small, RectPart::FullTop | RectPart::NoTopBottom);

		auto bottomPart = QRect(0, _footerTop - _roundRadius, width(), st::emojiCategory.height + _roundRadius);
		auto bottomParts = RectPart::NoTopBottom | RectPart::FullBottom;
		App::roundRect(p, bottomPart, bottomBg, ImageRoundRadius::Small, bottomParts);
	} else {
		p.fillRect(0, 0, width(), _tabsSlider->height(), st::emojiPanBg);
		p.fillRect(0, _footerTop, width(), st::emojiCategory.height, bottomBg);
	}

	auto sidesTop = marginTop();
	auto sidesHeight = height() - sidesTop - marginBottom();
	if (_restrictedLabel) {
		p.fillRect(0, sidesTop, width(), sidesHeight, st::emojiPanBg);
	} else {
		p.fillRect(myrtlrect(width() - st::emojiScroll.width, sidesTop, st::emojiScroll.width, sidesHeight),
		           st::emojiPanBg);
		p.fillRect(myrtlrect(0, sidesTop, st::buttonRadius, sidesHeight), st::emojiPanBg);
	}
}

int TabbedSelector::marginTop() const {
	return _tabsSlider->height() - st::lineWidth;
}

int TabbedSelector::marginBottom() const {
	return st::emojiCategory.height;
}

void TabbedSelector::refreshStickers() {
	stickers()->refreshStickers();
	if (isHidden() || _currentTabType != SelectorTab::Stickers) {
		stickers()->preloadImages();
	}
}

bool TabbedSelector::preventAutoHide() const {
	return stickers()->preventAutoHide();
}

QImage TabbedSelector::grabForAnimation() {
	auto slideAnimationData = base::take(_slideAnimation);
	auto slideAnimation = base::take(_a_slide);

	showAll();
	_topShadow->hide();
	_tabsSlider->hide();
	myEnsureResized(this);

	auto result = QImage(size() * cIntRetinaFactor(), QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(cRetinaFactor());
	result.fill(Qt::transparent);
	render(&result);

	_a_slide = base::take(slideAnimation);
	_slideAnimation = base::take(slideAnimationData);

	return result;
}

bool TabbedSelector::wheelEventFromFloatPlayer(QEvent *e) {
	return _scroll->viewportEvent(e);
}

QRect TabbedSelector::rectForFloatPlayer() {
	return mapToGlobal(_scroll->geometry());
}

TabbedSelector::~TabbedSelector() = default;

void TabbedSelector::hideFinished() {
	for (auto &tab : _tabs) {
		tab.widget()->panelHideFinished();
	}
	_a_slide.finish();
	_slideAnimation.reset();
}

void TabbedSelector::showStarted() {
	Auth().api().updateStickers();
	currentTab()->widget()->refreshRecent();
	currentTab()->widget()->preloadImages();
	_a_slide.finish();
	_slideAnimation.reset();
	showAll();
}

void TabbedSelector::beforeHiding() {
	if (!_scroll->isHidden()) {
		currentTab()->widget()->beforeHiding();
		if (_beforeHidingCallback) {
			_beforeHidingCallback(_currentTabType);
		}
	}
}

void TabbedSelector::afterShown() {
	if (!_a_slide.animating()) {
		showAll();
		currentTab()->widget()->afterShown();
		if (_afterShownCallback) {
			_afterShownCallback(_currentTabType);
		}
	}
}

void TabbedSelector::stickersInstalled(quint64 setId) {
	_tabsSlider->setActiveSection(static_cast<int>(SelectorTab::Stickers));
	stickers()->showStickerSet(setId);
}

void TabbedSelector::showMegagroupSet(ChannelData *megagroup) {
	stickers()->showMegagroupSet(megagroup);
}

void TabbedSelector::setCurrentPeer(PeerData *peer) {
	gifs()->setInlineQueryPeer(peer);
	_currentPeer = peer;
	checkRestrictedPeer();
}

void TabbedSelector::checkRestrictedPeer() {
	if (auto megagroup = _currentPeer ? _currentPeer->asMegagroup() : nullptr) {
		auto restricted =
		    (_currentTabType == SelectorTab::Stickers) ?
		        megagroup->restrictedRights().is_send_stickers() :
		        (_currentTabType == SelectorTab::Gifs) ? megagroup->restrictedRights().is_send_gifs() : false;
		if (restricted) {
			if (!_restrictedLabel) {
				auto text = (_currentTabType == SelectorTab::Stickers) ?
				                lang(lng_restricted_send_stickers) :
				                (_currentTabType == SelectorTab::Gifs) ? lang(lng_restricted_send_gifs) : QString();
				_restrictedLabel.create(this, text, Ui::FlatLabel::InitType::Simple, st::stickersRestrictedLabel);
				_restrictedLabel->show();
				_restrictedLabel->move((width() - _restrictedLabel->width()),
				                       (height() / 3 - _restrictedLabel->height() / 2));
				currentTab()->footer()->hide();
				_scroll->hide();
				_bottomShadow->hide();
				update();
			}
			return;
		}
	}
	if (_restrictedLabel) {
		_restrictedLabel.destroy();
		if (!_a_slide.animating()) {
			currentTab()->footer()->show();
			_scroll->show();
			_bottomShadow->setVisible(_currentTabType == SelectorTab::Gifs);
			update();
		}
	}
}

bool TabbedSelector::isRestrictedView() {
	checkRestrictedPeer();
	return (_restrictedLabel != nullptr);
}

void TabbedSelector::showAll() {
	if (isRestrictedView()) {
		_restrictedLabel->show();
	} else {
		currentTab()->footer()->show();
		_scroll->show();
		_bottomShadow->setVisible(_currentTabType == SelectorTab::Gifs);
	}
	_topShadow->show();
	_tabsSlider->show();
}

void TabbedSelector::hideForSliding() {
	hideChildren();
	_tabsSlider->show();
	_topShadow->show();
	currentTab()->widget()->clearSelection();
}

void TabbedSelector::onScroll() {
	auto scrollTop = _scroll->scrollTop();
	auto scrollBottom = scrollTop + _scroll->height();
	currentTab()->widget()->setVisibleTopBottom(scrollTop, scrollBottom);
}

void TabbedSelector::setRoundRadius(int radius) {
	_roundRadius = radius;
	_tabsSlider->setRippleTopRoundRadius(_roundRadius);
}

void TabbedSelector::createTabsSlider() {
	auto sections = QStringList();
	sections.push_back(lang(lng_switch_emoji).toUpper());
	sections.push_back(lang(lng_switch_stickers).toUpper());
	sections.push_back(lang(lng_switch_gifs).toUpper());
	_tabsSlider->setSections(sections);

	_tabsSlider->setActiveSectionFast(static_cast<int>(_currentTabType));
	_tabsSlider->setSectionActivatedCallback([this] { switchTab(); });

	_tabsSlider->resizeToWidth(width());
	_tabsSlider->moveToLeft(0, 0);
	_topShadow->setGeometry(_tabsSlider->x(), _tabsSlider->bottomNoMargins() - st::lineWidth, _tabsSlider->width(),
	                        st::lineWidth);
}

bool TabbedSelector::hasSectionIcons() const {
	return (_currentTabType != SelectorTab::Gifs) && !_restrictedLabel;
}

void TabbedSelector::switchTab() {
	auto tab = _tabsSlider->activeSection();
	Assert(tab >= 0 && tab < Tab::kCount);
	auto newTabType = static_cast<SelectorTab>(tab);
	if (_currentTabType == newTabType) {
		return;
	}

	auto wasSectionIcons = hasSectionIcons();
	auto wasTab = _currentTabType;
	currentTab()->saveScrollTop();

	beforeHiding();

	auto wasCache = grabForAnimation();

	auto widget = _scroll->takeWidget<Inner>();
	widget->setParent(this);
	widget->hide();
	currentTab()->footer()->hide();
	currentTab()->returnWidget(std::move(widget));

	_currentTabType = newTabType;
	_restrictedLabel.destroy();
	checkRestrictedPeer();

	currentTab()->widget()->refreshRecent();
	currentTab()->widget()->preloadImages();
	setWidgetToScrollArea();

	auto nowCache = grabForAnimation();

	auto direction =
	    (wasTab > _currentTabType) ? SlideAnimation::Direction::LeftToRight : SlideAnimation::Direction::RightToLeft;
	if (direction == SlideAnimation::Direction::LeftToRight) {
		std::swap(wasCache, nowCache);
	}
	_slideAnimation = std::make_unique<SlideAnimation>();
	auto slidingRect = QRect(_tabsSlider->x() * cIntRetinaFactor(), _scroll->y() * cIntRetinaFactor(),
	                         _tabsSlider->width() * cIntRetinaFactor(), (height() - _scroll->y()) * cIntRetinaFactor());
	_slideAnimation->setFinalImages(direction, std::move(wasCache), std::move(nowCache), slidingRect, wasSectionIcons);
	auto corners = App::cornersMask(ImageRoundRadius::Small);
	_slideAnimation->setCornerMasks(corners[0], corners[1], corners[2], corners[3]);
	_slideAnimation->start();

	hideForSliding();

	getTab(wasTab)->widget()->hideFinished();

	_a_slide.start([this] { update(); }, 0., 1., st::emojiPanSlideDuration, anim::linear);
	update();

	Auth().data().setSelectorTab(_currentTabType);
	Auth().saveDataDelayed(kSaveChosenTabTimeout);
}

not_null<EmojiListWidget *> TabbedSelector::emoji() const {
	return static_cast<EmojiListWidget *>(getTab(SelectorTab::Emoji)->widget().get());
}

not_null<StickersListWidget *> TabbedSelector::stickers() const {
	return static_cast<StickersListWidget *>(getTab(SelectorTab::Stickers)->widget().get());
}

not_null<GifsListWidget *> TabbedSelector::gifs() const {
	return static_cast<GifsListWidget *>(getTab(SelectorTab::Gifs)->widget().get());
}

void TabbedSelector::setWidgetToScrollArea() {
	_scroll->setOwnedWidget(currentTab()->takeWidget());
	_scroll->disableScroll(false);
	currentTab()->widget()->moveToLeft(0, 0);
	currentTab()->widget()->show();
	scrollToY(currentTab()->getScrollTop());
	onScroll();
}

void TabbedSelector::scrollToY(int y) {
	_scroll->scrollToY(y);

	// Qt render glitch workaround, shadow sometimes disappears if we just scroll to y.
	_topShadow->update();
}

TabbedSelector::Inner::Inner(QWidget *parent, not_null<Window::Controller *> controller)
    : TWidget(parent)
    , _controller(controller) {}

void TabbedSelector::Inner::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	auto oldVisibleHeight = getVisibleBottom() - getVisibleTop();
	_visibleTop = visibleTop;
	_visibleBottom = visibleBottom;
	auto visibleHeight = getVisibleBottom() - getVisibleTop();
	if (visibleHeight != oldVisibleHeight) {
		resize(st::emojiPanWidth - st::emojiScroll.width - st::buttonRadius, countHeight());
	}
}

void TabbedSelector::Inner::hideFinished() {
	processHideFinished();
	if (auto footer = getFooter()) {
		footer->processHideFinished();
	}
}

void TabbedSelector::Inner::panelHideFinished() {
	hideFinished();
	processPanelHideFinished();
	if (auto footer = getFooter()) {
		footer->processPanelHideFinished();
	}
}

TabbedSelector::InnerFooter::InnerFooter(QWidget *parent)
    : TWidget(parent) {
	resize(st::emojiPanWidth, st::emojiCategory.height);
}

} // namespace ChatHelpers
