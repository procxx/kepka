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
#include "chat_helpers/emoji_list_widget.h"

#include "app.h"
#include "lang/lang_keys.h"
#include "styles/style_chat_helpers.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/shadow.h"

namespace ChatHelpers {
namespace {

constexpr auto kEmojiPanelPerRow = Ui::Emoji::kPanelPerRow;
constexpr auto kEmojiPanelRowsPerPage = Ui::Emoji::kPanelRowsPerPage;

} // namespace

class EmojiListWidget::Footer : public TabbedSelector::InnerFooter {
public:
	Footer(not_null<EmojiListWidget *> parent);

	void setCurrentSectionIcon(Section section);

protected:
	void processPanelHideFinished() override;

private:
	void prepareSection(int &left, int top, int _width, Ui::IconButton *sectionIcon, Section section);
	void setActiveSection(Section section);

	not_null<EmojiListWidget *> _pan;
	std::array<object_ptr<Ui::IconButton>, kEmojiSectionCount> _sections;
};

EmojiListWidget::Footer::Footer(not_null<EmojiListWidget *> parent)
    : InnerFooter(parent)
    , _pan(parent)
    , _sections{{
          object_ptr<Ui::IconButton>(this, st::emojiCategoryRecent),
          object_ptr<Ui::IconButton>(this, st::emojiCategoryPeople),
          object_ptr<Ui::IconButton>(this, st::emojiCategoryNature),
          object_ptr<Ui::IconButton>(this, st::emojiCategoryFood),
          object_ptr<Ui::IconButton>(this, st::emojiCategoryActivity),
          object_ptr<Ui::IconButton>(this, st::emojiCategoryTravel),
          object_ptr<Ui::IconButton>(this, st::emojiCategoryObjects),
          object_ptr<Ui::IconButton>(this, st::emojiCategorySymbols),
      }} {
	auto left = (st::emojiPanWidth - _sections.size() * st::emojiCategory.width) / 2;
	for (auto i = 0; i != _sections.size(); ++i) {
		auto &button = _sections[i];
		button->moveToLeft(left, 0);
		left += button->width();
		button->setClickedCallback([this, value = static_cast<Section>(i)] { setActiveSection(value); });
	}
	setCurrentSectionIcon(Section::Recent);
}

void EmojiListWidget::Footer::processPanelHideFinished() {
	// Preserve panel state through visibility toggles.
	// setCurrentSectionIcon(Section::Recent);
}

void EmojiListWidget::Footer::setCurrentSectionIcon(Section section) {
	std::array<const style::icon *, kEmojiSectionCount> overrides = {{
	    &st::emojiRecentActive,
	    &st::emojiPeopleActive,
	    &st::emojiNatureActive,
	    &st::emojiFoodActive,
	    &st::emojiActivityActive,
	    &st::emojiTravelActive,
	    &st::emojiObjectsActive,
	    &st::emojiSymbolsActive,
	}};
	for (auto i = 0; i != _sections.size(); ++i) {
		_sections[i]->setIconOverride((section == static_cast<Section>(i)) ? overrides[i] : nullptr);
	}
}

void EmojiListWidget::Footer::setActiveSection(Ui::Emoji::Section section) {
	_pan->showEmojiSection(section);
}

EmojiColorPicker::EmojiColorPicker(QWidget *parent)
    : TWidget(parent) {
	setMouseTracking(true);

	auto w = st::emojiPanMargins.left() + st::emojiPanSize.width() + st::emojiColorsSep + st::emojiPanMargins.right();
	auto h = st::emojiPanMargins.top() + 2 * st::emojiColorsPadding + st::emojiPanSize.height() +
	         st::emojiPanMargins.bottom();
	resize(w, h);

	_hideTimer.setSingleShot(true);
	connect(&_hideTimer, SIGNAL(timeout()), this, SLOT(hideAnimated()));
}

void EmojiColorPicker::showEmoji(EmojiPtr emoji) {
	if (!emoji || !emoji->hasVariants()) {
		return;
	}
	_ignoreShow = false;

	_variants.resize(emoji->variantsCount() + 1);
	for (auto i = 0, size = _variants.size(); i != size; ++i) {
		_variants[i] = emoji->variant(i);
	}

	auto w = st::emojiPanMargins.left() + st::emojiPanSize.width() * _variants.size() +
	         (_variants.size() - 2) * st::emojiColorsPadding + st::emojiColorsSep + st::emojiPanMargins.right();
	auto h = st::emojiPanMargins.top() + 2 * st::emojiColorsPadding + st::emojiPanSize.height() +
	         st::emojiPanMargins.bottom();
	resize(w, h);

	if (!_cache.isNull()) _cache = QPixmap();
	showAnimated();
}

void EmojiColorPicker::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto opacity = _a_opacity.current(getms(), _hiding ? 0. : 1.);
	if (opacity < 1.) {
		if (opacity > 0.) {
			p.setOpacity(opacity);
		} else {
			return;
		}
	}
	if (e->rect() != rect()) {
		p.setClipRect(e->rect());
	}

	auto inner = rect().marginsRemoved(st::emojiPanMargins);
	if (!_cache.isNull()) {
		p.drawPixmap(0, 0, _cache);
		return;
	}
	Ui::Shadow::paint(p, inner, width(), st::defaultRoundShadow);
	App::roundRect(p, inner, st::boxBg, BoxCorners);

	auto x = st::emojiPanMargins.left() + 2 * st::emojiColorsPadding + st::emojiPanSize.width();
	if (rtl()) x = width() - x - st::emojiColorsSep;
	p.fillRect(x, st::emojiPanMargins.top() + st::emojiColorsPadding, st::emojiColorsSep,
	           inner.height() - st::emojiColorsPadding * 2, st::emojiColorsSepColor);

	if (_variants.isEmpty()) return;
	for (auto i = 0, count = _variants.size(); i != count; ++i) {
		drawVariant(p, i);
	}
}

void EmojiColorPicker::enterEventHook(QEvent *e) {
	_hideTimer.stop();
	if (_hiding) showAnimated();
	TWidget::enterEventHook(e);
}

void EmojiColorPicker::leaveEventHook(QEvent *e) {
	TWidget::leaveEventHook(e);
}

void EmojiColorPicker::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	_lastMousePos = e->globalPos();
	updateSelected();
	_pressedSel = _selected;
}

void EmojiColorPicker::mouseReleaseEvent(QMouseEvent *e) {
	handleMouseRelease(e->globalPos());
}

void EmojiColorPicker::handleMouseRelease(QPoint globalPos) {
	_lastMousePos = globalPos;
	qint32 pressed = _pressedSel;
	_pressedSel = -1;

	updateSelected();
	if (_selected >= 0 && (pressed < 0 || _selected == pressed)) {
		emit emojiSelected(_variants[_selected]);
	}
	_ignoreShow = true;
	hideAnimated();
}

void EmojiColorPicker::handleMouseMove(QPoint globalPos) {
	_lastMousePos = globalPos;
	updateSelected();
}

void EmojiColorPicker::mouseMoveEvent(QMouseEvent *e) {
	handleMouseMove(e->globalPos());
}

void EmojiColorPicker::animationCallback() {
	update();
	if (!_a_opacity.animating()) {
		_cache = QPixmap();
		if (_hiding) {
			hide();
			emit hidden();
		} else {
			_lastMousePos = QCursor::pos();
			updateSelected();
		}
	}
}

void EmojiColorPicker::hideFast() {
	clearSelection();
	_a_opacity.finish();
	_cache = QPixmap();
	hide();
	emit hidden();
}

void EmojiColorPicker::hideAnimated() {
	if (_cache.isNull()) {
		_cache = myGrab(this);
		clearSelection();
	}
	_hiding = true;
	_a_opacity.start([this] { animationCallback(); }, 1., 0., st::emojiPanDuration);
}

void EmojiColorPicker::showAnimated() {
	if (_ignoreShow) return;

	if (!isHidden() && !_hiding) {
		return;
	}
	_hiding = false;
	if (_cache.isNull()) {
		_cache = myGrab(this);
		clearSelection();
	}
	show();
	_a_opacity.start([this] { animationCallback(); }, 0., 1., st::emojiPanDuration);
}

void EmojiColorPicker::clearSelection() {
	_pressedSel = -1;
	setSelected(-1);
	_lastMousePos = mapToGlobal(QPoint(-10, -10));
}

void EmojiColorPicker::updateSelected() {
	auto newSelected = -1;
	auto p = mapFromGlobal(_lastMousePos);
	auto sx = rtl() ? (width() - p.x()) : p.x(), y = p.y() - st::emojiPanMargins.top() - st::emojiColorsPadding;
	if (y >= 0 && y < st::emojiPanSize.height()) {
		auto x = sx - st::emojiPanMargins.left() - st::emojiColorsPadding;
		if (x >= 0 && x < st::emojiPanSize.width()) {
			newSelected = 0;
		} else {
			x -= st::emojiPanSize.width() + 2 * st::emojiColorsPadding + st::emojiColorsSep;
			if (x >= 0 && x < st::emojiPanSize.width() * (_variants.size() - 1)) {
				newSelected = (x / st::emojiPanSize.width()) + 1;
			}
		}
	}

	setSelected(newSelected);
}

void EmojiColorPicker::setSelected(int newSelected) {
	if (_selected == newSelected) {
		return;
	}
	auto updateSelectedRect = [this] {
		if (_selected < 0) return;
		rtlupdate(st::emojiPanMargins.left() + st::emojiColorsPadding + _selected * st::emojiPanSize.width() +
		              (_selected ? 2 * st::emojiColorsPadding + st::emojiColorsSep : 0),
		          st::emojiPanMargins.top() + st::emojiColorsPadding, st::emojiPanSize.width(),
		          st::emojiPanSize.height());
	};
	updateSelectedRect();
	_selected = newSelected;
	updateSelectedRect();
	setCursor((_selected >= 0) ? style::cur_pointer : style::cur_default);
}

void EmojiColorPicker::drawVariant(Painter &p, int variant) {
	QPoint w(st::emojiPanMargins.left() + st::emojiColorsPadding + variant * st::emojiPanSize.width() +
	             (variant ? 2 * st::emojiColorsPadding + st::emojiColorsSep : 0),
	         st::emojiPanMargins.top() + st::emojiColorsPadding);
	if (variant == _selected) {
		QPoint tl(w);
		if (rtl()) tl.setX(width() - tl.x() - st::emojiPanSize.width());
		App::roundRect(p, QRect(tl, st::emojiPanSize), st::emojiPanHover, StickerHoverCorners);
	}
	auto esize = Ui::Emoji::Size(Ui::Emoji::Index() + 1);
	p.drawPixmapLeft(w.x() + (st::emojiPanSize.width() - (esize / cIntRetinaFactor())) / 2,
	                 w.y() + (st::emojiPanSize.height() - (esize / cIntRetinaFactor())) / 2, width(), App::emojiLarge(),
	                 QRect(_variants[variant]->x() * esize, _variants[variant]->y() * esize, esize, esize));
}

EmojiListWidget::EmojiListWidget(QWidget *parent, not_null<Window::Controller *> controller)
    : Inner(parent, controller)
    , _picker(this) {
	resize(st::emojiPanWidth - st::emojiScroll.width - st::buttonRadius, countHeight());

	setMouseTracking(true);
	setAttribute(Qt::WA_OpaquePaintEvent);

	_picker->hide();

	_esize = Ui::Emoji::Size(Ui::Emoji::Index() + 1);

	for (auto i = 0; i != kEmojiSectionCount; ++i) {
		_counts[i] = Ui::Emoji::GetSectionCount(static_cast<Section>(i));
	}

	_showPickerTimer.setSingleShot(true);
	connect(&_showPickerTimer, SIGNAL(timeout()), this, SLOT(onShowPicker()));
	connect(_picker, SIGNAL(emojiSelected(EmojiPtr)), this, SLOT(onColorSelected(EmojiPtr)));
	connect(_picker, SIGNAL(hidden()), this, SLOT(onPickerHidden()));
}

void EmojiListWidget::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	Inner::setVisibleTopBottom(visibleTop, visibleBottom);
	if (_footer) {
		_footer->setCurrentSectionIcon(currentSection(visibleTop));
	}
}

object_ptr<TabbedSelector::InnerFooter> EmojiListWidget::createFooter() {
	Expects(_footer == nullptr);
	auto result = object_ptr<Footer>(this);
	_footer = result;
	return std::move(result);
}

template <typename Callback> bool EmojiListWidget::enumerateSections(Callback callback) const {
	auto info = SectionInfo();
	for (auto i = 0; i != kEmojiSectionCount; ++i) {
		info.section = i;
		info.count = _counts[i];
		info.rowsCount = (info.count / kEmojiPanelPerRow) + ((info.count % kEmojiPanelPerRow) ? 1 : 0);
		info.rowsTop = info.top + (i == 0 ? st::emojiPanPadding : st::emojiPanHeader);
		info.rowsBottom = info.rowsTop + info.rowsCount * st::emojiPanSize.height();
		if (!callback(info)) {
			return false;
		}
		info.top = info.rowsBottom;
	}
	return true;
}

EmojiListWidget::SectionInfo EmojiListWidget::sectionInfo(int section) const {
	Expects(section >= 0 && section < kEmojiSectionCount);
	auto result = SectionInfo();
	enumerateSections([searchForSection = section, &result](const SectionInfo &info) {
		if (info.section == searchForSection) {
			result = info;
			return false;
		}
		return true;
	});
	return result;
}

EmojiListWidget::SectionInfo EmojiListWidget::sectionInfoByOffset(int yOffset) const {
	auto result = SectionInfo();
	enumerateSections([&result, yOffset](const SectionInfo &info) {
		if (yOffset < info.rowsBottom || info.section == kEmojiSectionCount - 1) {
			result = info;
			return false;
		}
		return true;
	});
	return result;
}

int EmojiListWidget::countHeight() {
	return sectionInfo(kEmojiSectionCount - 1).rowsBottom + st::emojiPanPadding;
}

void EmojiListWidget::ensureLoaded(int section) {
	Expects(section >= 0 && section < kEmojiSectionCount);
	if (!_emoji[section].isEmpty()) {
		return;
	}
	_emoji[section] = Ui::Emoji::GetSection(static_cast<Section>(section));
	_counts[section] = _emoji[section].size();
	if (static_cast<Section>(section) == Section::Recent) {
		return;
	}
	for (auto &emoji : _emoji[section]) {
		if (emoji->hasVariants()) {
			auto j = cEmojiVariants().constFind(emoji->nonColoredId());
			if (j != cEmojiVariants().cend()) {
				emoji = emoji->variant(j.value());
			}
		}
	}
}

void EmojiListWidget::paintEvent(QPaintEvent *e) {
	Painter p(this);
	QRect r = e ? e->rect() : rect();
	if (r != rect()) {
		p.setClipRect(r);
	}
	p.fillRect(r, st::emojiPanBg);

	auto fromColumn = floorclamp(r.x() - st::emojiPanPadding, st::emojiPanSize.width(), 0, kEmojiPanelPerRow);
	auto toColumn = ceilclamp(r.x() + r.width() - st::emojiPanPadding, st::emojiPanSize.width(), 0, kEmojiPanelPerRow);
	if (rtl()) {
		qSwap(fromColumn, toColumn);
		fromColumn = kEmojiPanelPerRow - fromColumn;
		toColumn = kEmojiPanelPerRow - toColumn;
	}

	enumerateSections([this, &p, r, fromColumn, toColumn](const SectionInfo &info) {
		if (r.top() >= info.rowsBottom) {
			return true;
		} else if (r.top() + r.height() <= info.top) {
			return false;
		}
		if (info.section > 0 && r.top() < info.rowsTop) {
			p.setFont(st::emojiPanHeaderFont);
			p.setPen(st::emojiPanHeaderFg);
			p.drawTextLeft(st::emojiPanHeaderLeft - st::buttonRadius, info.top + st::emojiPanHeaderTop, width(),
			               lang(LangKey(lng_emoji_category0 + info.section)));
		}
		if (r.top() + r.height() > info.rowsTop) {
			ensureLoaded(info.section);
			auto fromRow = floorclamp(r.y() - info.rowsTop, st::emojiPanSize.height(), 0, info.rowsCount);
			auto toRow = ceilclamp(r.y() + r.height() - info.rowsTop, st::emojiPanSize.height(), 0, info.rowsCount);
			for (auto i = fromRow; i < toRow; ++i) {
				for (auto j = fromColumn; j < toColumn; ++j) {
					auto index = i * kEmojiPanelPerRow + j;
					if (index >= info.count) break;

					auto selected = (!_picker->isHidden() && info.section * MatrixRowShift + index == _pickerSel) ||
					                (info.section * MatrixRowShift + index == _selected);

					auto w = QPoint(st::emojiPanPadding + j * st::emojiPanSize.width(),
					                info.rowsTop + i * st::emojiPanSize.height());
					if (selected) {
						auto tl = w;
						if (rtl()) tl.setX(width() - tl.x() - st::emojiPanSize.width());
						App::roundRect(p, QRect(tl, st::emojiPanSize), st::emojiPanHover, StickerHoverCorners);
					}
					auto sourceRect = QRect(_emoji[info.section][index]->x() * _esize,
					                        _emoji[info.section][index]->y() * _esize, _esize, _esize);
					auto imageLeft = w.x() + (st::emojiPanSize.width() - (_esize / cIntRetinaFactor())) / 2;
					auto imageTop = w.y() + (st::emojiPanSize.height() - (_esize / cIntRetinaFactor())) / 2;
					p.drawPixmapLeft(imageLeft, imageTop, width(), App::emojiLarge(), sourceRect);
				}
			}
		}
		return true;
	});
}

bool EmojiListWidget::checkPickerHide() {
	if (!_picker->isHidden() && _pickerSel >= 0) {
		_picker->hideAnimated();
		_pickerSel = -1;
		updateSelected();
		return true;
	}
	return false;
}

void EmojiListWidget::mousePressEvent(QMouseEvent *e) {
	_lastMousePos = e->globalPos();
	updateSelected();
	if (checkPickerHide() || e->button() != Qt::LeftButton) {
		return;
	}
	_pressedSel = _selected;

	if (_selected >= 0) {
		auto section = (_selected / MatrixRowShift);
		auto sel = _selected % MatrixRowShift;
		if (section < kEmojiSectionCount && sel < _emoji[section].size() && _emoji[section][sel]->hasVariants()) {
			_pickerSel = _selected;
			setCursor(style::cur_default);
			if (!cEmojiVariants().contains(_emoji[section][sel]->nonColoredId())) {
				onShowPicker();
			} else {
				_showPickerTimer.start(500);
			}
		}
	}
}

void EmojiListWidget::mouseReleaseEvent(QMouseEvent *e) {
	qint32 pressed = _pressedSel;
	_pressedSel = -1;

	_lastMousePos = e->globalPos();
	if (!_picker->isHidden()) {
		if (_picker->rect().contains(_picker->mapFromGlobal(_lastMousePos))) {
			return _picker->handleMouseRelease(QCursor::pos());
		} else if (_pickerSel >= 0) {
			auto section = (_pickerSel / MatrixRowShift);
			auto sel = _pickerSel % MatrixRowShift;
			if (section < kEmojiSectionCount && sel < _emoji[section].size() && _emoji[section][sel]->hasVariants()) {
				if (cEmojiVariants().contains(_emoji[section][sel]->nonColoredId())) {
					_picker->hideAnimated();
					_pickerSel = -1;
				}
			}
		}
	}
	updateSelected();

	if (_showPickerTimer.isActive()) {
		_showPickerTimer.stop();
		_pickerSel = -1;
		_picker->hide();
	}

	if (_selected < 0 || _selected != pressed) return;

	if (_selected >= kEmojiSectionCount * MatrixRowShift) {
		return;
	}

	auto section = (_selected / MatrixRowShift);
	auto sel = _selected % MatrixRowShift;
	if (sel < _emoji[section].size()) {
		auto emoji = _emoji[section][sel];
		if (emoji->hasVariants() && !_picker->isHidden()) return;

		selectEmoji(emoji);
	}
}

void EmojiListWidget::selectEmoji(EmojiPtr emoji) {
	Ui::Emoji::AddRecent(emoji);
	emit selected(emoji);
}

void EmojiListWidget::onShowPicker() {
	if (_pickerSel < 0) return;

	auto section = (_pickerSel / MatrixRowShift);
	auto sel = _pickerSel % MatrixRowShift;
	if (section < kEmojiSectionCount && sel < _emoji[section].size() && _emoji[section][sel]->hasVariants()) {
		_picker->showEmoji(_emoji[section][sel]);

		auto y = emojiRect(section, sel).y();
		y -= _picker->height() - st::buttonRadius + getVisibleTop();
		if (y < st::emojiPanHeader) {
			y += _picker->height() - st::buttonRadius + st::emojiPanSize.height() - st::buttonRadius;
		}
		auto xmax = width() - _picker->width();
		auto coef = double(sel % kEmojiPanelPerRow) / double(kEmojiPanelPerRow - 1);
		if (rtl()) coef = 1. - coef;
		_picker->move(std::round(xmax * coef), y);

		emit disableScroll(true);
	}
}

void EmojiListWidget::onPickerHidden() {
	_pickerSel = -1;
	update();
	emit disableScroll(false);

	_lastMousePos = QCursor::pos();
	updateSelected();
}

QRect EmojiListWidget::emojiRect(int section, int sel) {
	auto info = sectionInfo(section);
	auto countTillItem = (sel - (sel % kEmojiPanelPerRow));
	auto rowsToSkip = (countTillItem / kEmojiPanelPerRow) + ((countTillItem % kEmojiPanelPerRow) ? 1 : 0);
	auto x = st::emojiPanPadding + ((sel % kEmojiPanelPerRow) * st::emojiPanSize.width());
	auto y = info.rowsTop + rowsToSkip * st::emojiPanSize.height();
	return QRect(x, y, st::emojiPanSize.width(), st::emojiPanSize.height());
}

void EmojiListWidget::onColorSelected(EmojiPtr emoji) {
	if (emoji->hasVariants()) {
		cRefEmojiVariants().insert(emoji->nonColoredId(), emoji->variantIndex(emoji));
	}
	if (_pickerSel >= 0) {
		auto section = (_pickerSel / MatrixRowShift);
		auto sel = _pickerSel % MatrixRowShift;
		if (section >= 0 && section < kEmojiSectionCount) {
			_emoji[section][sel] = emoji;
			rtlupdate(emojiRect(section, sel));
		}
	}
	selectEmoji(emoji);
	_picker->hideAnimated();
}

void EmojiListWidget::mouseMoveEvent(QMouseEvent *e) {
	_lastMousePos = e->globalPos();
	if (!_picker->isHidden()) {
		if (_picker->rect().contains(_picker->mapFromGlobal(_lastMousePos))) {
			return _picker->handleMouseMove(QCursor::pos());
		} else {
			_picker->clearSelection();
		}
	}
	updateSelected();
}

void EmojiListWidget::leaveEventHook(QEvent *e) {
	clearSelection();
}

void EmojiListWidget::leaveToChildEvent(QEvent *e, QWidget *child) {
	clearSelection();
}

void EmojiListWidget::enterFromChildEvent(QEvent *e, QWidget *child) {
	_lastMousePos = QCursor::pos();
	updateSelected();
}

void EmojiListWidget::clearSelection() {
	_lastMousePos = mapToGlobal(QPoint(-10, -10));
	_pressedSel = -1;
	setSelected(-1);
}

Ui::Emoji::Section EmojiListWidget::currentSection(int yOffset) const {
	return static_cast<Section>(sectionInfoByOffset(yOffset).section);
}

TabbedSelector::InnerFooter *EmojiListWidget::getFooter() const {
	return _footer;
}

void EmojiListWidget::processHideFinished() {
	if (!_picker->isHidden()) {
		_picker->hideFast();
		_pickerSel = -1;
	}
	clearSelection();
}

void EmojiListWidget::refreshRecent() {
	clearSelection();
	_emoji[0] = Ui::Emoji::GetSection(Section::Recent);
	_counts[0] = _emoji[0].size();
	auto h = countHeight();
	if (h != height()) {
		resize(width(), h);
		update();
	}
}

bool EmojiListWidget::event(QEvent *e) {
	if (e->type() == QEvent::ParentChange) {
		if (_picker->parentWidget() != parentWidget()) {
			_picker->setParent(parentWidget());
		}
		_picker->raise();
	}
	return Inner::event(e);
}

void EmojiListWidget::updateSelected() {
	if (_pressedSel >= 0 || _pickerSel >= 0) return;

	auto newSelected = -1;
	auto p = mapFromGlobal(_lastMousePos);
	auto info = sectionInfoByOffset(p.y());
	if (p.y() >= info.rowsTop && p.y() < info.rowsBottom) {
		auto sx = (rtl() ? width() - p.x() : p.x()) - st::emojiPanPadding;
		if (sx >= 0 && sx < kEmojiPanelPerRow * st::emojiPanSize.width()) {
			newSelected = std::floor((p.y() - info.rowsTop) / st::emojiPanSize.height()) * kEmojiPanelPerRow +
			              std::floor(sx / st::emojiPanSize.width());
			if (newSelected >= _emoji[info.section].size()) {
				newSelected = -1;
			} else {
				newSelected += info.section * MatrixRowShift;
			}
		}
	}

	setSelected(newSelected);
}

void EmojiListWidget::setSelected(int newSelected) {
	if (_selected == newSelected) {
		return;
	}
	auto updateSelected = [this]() {
		if (_selected < 0) return;
		rtlupdate(emojiRect(_selected / MatrixRowShift, _selected % MatrixRowShift));
	};
	updateSelected();
	_selected = newSelected;
	updateSelected();

	setCursor((_selected >= 0) ? style::cur_pointer : style::cur_default);
	if (_selected >= 0 && !_picker->isHidden()) {
		if (_selected != _pickerSel) {
			_picker->hideAnimated();
		} else {
			_picker->showAnimated();
		}
	}
}

void EmojiListWidget::showEmojiSection(Section section) {
	clearSelection();

	refreshRecent();

	auto y = 0;
	enumerateSections([&y, sectionForSearch = section](const SectionInfo &info) {
		if (static_cast<Section>(info.section) == sectionForSearch) {
			y = info.top;
			return false;
		}
		return true;
	});
	emit scrollToY(y);

	_lastMousePos = QCursor::pos();

	update();
}

} // namespace ChatHelpers
