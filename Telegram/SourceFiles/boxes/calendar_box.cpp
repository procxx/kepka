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
#include "boxes/calendar_box.h"

#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "styles/style_dialogs.h"
#include "ui/effects/ripple_animation.h"
#include "ui/widgets/buttons.h"

namespace {

constexpr auto kDaysInWeek = 7;

} // namespace

class CalendarBox::Context {
public:
	Context(QDate month, QDate highlighted);

	void start() {
		_month.setForced(_month.value(), true);
	}

	void setMinDate(QDate date);
	void setMaxDate(QDate date);

	int minDayIndex() const {
		return _minDayIndex;
	}
	int maxDayIndex() const {
		return _maxDayIndex;
	}

	void skipMonth(int skip);
	void showMonth(QDate month);

	int highlightedIndex() const {
		return _highlightedIndex;
	}
	int rowsCount() const {
		return _rowsCount;
	}
	int daysShift() const {
		return _daysShift;
	}
	int daysCount() const {
		return _daysCount;
	}
	bool isEnabled(int index) const {
		return (index >= _minDayIndex) && (index <= _maxDayIndex);
	}

	const base::Variable<QDate> &month() {
		return _month;
	}

	QDate dateFromIndex(int index) const;
	QString labelFromIndex(int index) const;

private:
	void applyMonth(const QDate &month, bool forced = false);

	static int daysShiftForMonth(QDate month);
	static int rowsCountForMonth(QDate month);

	base::Variable<QDate> _month;
	QDate _min, _max;
	QDate _highlighted;

	int _highlightedIndex = 0;
	int _minDayIndex = 0;
	int _maxDayIndex = 0;
	int _daysCount = 0;
	int _daysShift = 0;
	int _rowsCount = 0;
};

CalendarBox::Context::Context(QDate month, QDate highlighted)
    : _highlighted(highlighted) {
	showMonth(month);
}

void CalendarBox::Context::setMinDate(QDate date) {
	_min = date;
	applyMonth(_month.value(), true);
}

void CalendarBox::Context::setMaxDate(QDate date) {
	_max = date;
	applyMonth(_month.value(), true);
}

void CalendarBox::Context::showMonth(QDate month) {
	if (month.day() != 1) {
		month = QDate(month.year(), month.month(), 1);
	}
	applyMonth(month);
}

void CalendarBox::Context::applyMonth(const QDate &month, bool forced) {
	if (forced) {
		_month.setForced(month);
	} else {
		_month.set(month);
	}
	_daysCount = month.daysInMonth();
	_daysShift = daysShiftForMonth(month);
	_rowsCount = rowsCountForMonth(month);
	_highlightedIndex = month.daysTo(_highlighted);
	_minDayIndex = _min.isNull() ? INT_MIN : month.daysTo(_min);
	_maxDayIndex = _max.isNull() ? INT_MAX : month.daysTo(_max);
}

void CalendarBox::Context::skipMonth(int skip) {
	auto year = _month.value().year();
	auto month = _month.value().month();
	month += skip;
	while (month < 1) {
		--year;
		month += 12;
	}
	while (month > 12) {
		++year;
		month -= 12;
	}
	showMonth(QDate(year, month, 1));
}

int CalendarBox::Context::daysShiftForMonth(QDate month) {
	Assert(!month.isNull());
	constexpr auto kMaxRows = 6;
	auto inMonthIndex = month.day() - 1;
	auto inWeekIndex = month.dayOfWeek() - 1;
	return ((kMaxRows * kDaysInWeek) + inWeekIndex - inMonthIndex) % kDaysInWeek;
}

int CalendarBox::Context::rowsCountForMonth(QDate month) {
	Assert(!month.isNull());
	auto daysShift = daysShiftForMonth(month);
	auto daysCount = month.daysInMonth();
	auto cellsCount = daysShift + daysCount;
	auto result = (cellsCount / kDaysInWeek);
	if (cellsCount % kDaysInWeek) ++result;
	return result;
}

QDate CalendarBox::Context::dateFromIndex(int index) const {
	constexpr auto kMonthsCount = 12;
	auto month = _month.value().month();
	auto year = _month.value().year();
	while (index < 0) {
		if (!--month) {
			month += kMonthsCount;
			--year;
		}
		index += QDate(year, month, 1).daysInMonth();
	}
	for (auto maxIndex = QDate(year, month, 1).daysInMonth(); index >= maxIndex;
	     maxIndex = QDate(year, month, 1).daysInMonth()) {
		index -= maxIndex;
		if (month++ == kMonthsCount) {
			month -= kMonthsCount;
			++year;
		}
	}
	return QDate(year, month, index + 1);
}

QString CalendarBox::Context::labelFromIndex(int index) const {
	auto day = [this, index] {
		if (index >= 0 && index < daysCount()) {
			return index + 1;
		}
		return dateFromIndex(index).day();
	};
	return QString::number(day());
}

class CalendarBox::Inner : public TWidget, public RPCSender, private base::Subscriber {
public:
	Inner(QWidget *parent, Context *context);

	int countHeight() {
		auto innerHeight = st::calendarDaysHeight + _context->rowsCount() * st::calendarCellSize.height();
		return st::calendarPadding.top() + innerHeight + st::calendarPadding.bottom();
	}

	void setDateChosenCallback(Fn<void(QDate)> callback) {
		_dateChosenCallback = std::move(callback);
	}

	~Inner();

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	void monthChanged(QDate month);
	void setPressed(int pressed);

	int rowsLeft() const;
	int rowsTop() const;
	void resizeToCurrent();
	void paintDayNames(Painter &p, QRect clip);
	void paintRows(Painter &p, QRect clip);

	Context *_context = nullptr;

	std::map<int, std::unique_ptr<Ui::RippleAnimation>> _ripples;

	Fn<void(QDate)> _dateChosenCallback;

	static constexpr auto kEmptySelection = -kDaysInWeek;
	int _selected = kEmptySelection;
	int _pressed = kEmptySelection;
};

CalendarBox::Inner::Inner(QWidget *parent, Context *context)
    : TWidget(parent)
    , _context(context) {
	setMouseTracking(true);
	subscribe(context->month(), [this](QDate month) { monthChanged(month); });
}

void CalendarBox::Inner::monthChanged(QDate month) {
	_ripples.clear();
	resizeToCurrent();
	update();
}

void CalendarBox::Inner::resizeToCurrent() {
	resize(st::boxWideWidth, countHeight());
}

void CalendarBox::Inner::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto clip = e->rect();

	paintDayNames(p, clip);
	paintRows(p, clip);
}

void CalendarBox::Inner::paintDayNames(Painter &p, QRect clip) {
	p.setFont(st::calendarDaysFont);
	p.setPen(st::calendarDaysFg);
	auto y = st::calendarPadding.top();
	auto x = rowsLeft();
	if (!myrtlrect(x, y, st::calendarCellSize.width() * kDaysInWeek, st::calendarDaysHeight).intersects(clip)) {
		return;
	}
	for (auto i = 0; i != kDaysInWeek; ++i, x += st::calendarCellSize.width()) {
		auto rect = myrtlrect(x, y, st::calendarCellSize.width(), st::calendarDaysHeight);
		if (!rect.intersects(clip)) {
			continue;
		}
		p.drawText(rect, langDayOfWeek(i + 1), style::al_top);
	}
}

int CalendarBox::Inner::rowsLeft() const {
	return st::calendarPadding.left();
}

int CalendarBox::Inner::rowsTop() const {
	return st::calendarPadding.top() + st::calendarDaysHeight;
}

void CalendarBox::Inner::paintRows(Painter &p, QRect clip) {
	p.setFont(st::calendarDaysFont);
	auto ms = getms();
	auto y = rowsTop();
	auto index = -_context->daysShift();
	auto highlightedIndex = _context->highlightedIndex();
	for (auto row = 0, rowsCount = _context->rowsCount(), daysCount = _context->daysCount(); row != rowsCount;
	     ++row, y += st::calendarCellSize.height()) {
		auto x = rowsLeft();
		if (!myrtlrect(x, y, st::calendarCellSize.width() * kDaysInWeek, st::calendarCellSize.height())
		         .intersects(clip)) {
			index += kDaysInWeek;
			continue;
		}
		for (auto col = 0; col != kDaysInWeek; ++col, ++index, x += st::calendarCellSize.width()) {
			auto rect = myrtlrect(x, y, st::calendarCellSize.width(), st::calendarCellSize.height());
			auto grayedOut = (index < 0 || index >= daysCount || !rect.intersects(clip));
			auto highlighted = (index == highlightedIndex);
			auto enabled = _context->isEnabled(index);
			auto innerLeft = x + (st::calendarCellSize.width() - st::calendarCellInner) / 2;
			auto innerTop = y + (st::calendarCellSize.height() - st::calendarCellInner) / 2;
			if (highlighted) {
				PainterHighQualityEnabler hq(p);
				p.setPen(Qt::NoPen);
				p.setBrush(grayedOut ? st::windowBgOver : st::dialogsBgActive);
				p.drawEllipse(myrtlrect(innerLeft, innerTop, st::calendarCellInner, st::calendarCellInner));
				p.setBrush(Qt::NoBrush);
			}
			auto it = _ripples.find(index);
			if (it != _ripples.cend()) {
				auto colorOverride = [highlighted, grayedOut] {
					if (highlighted) {
						return grayedOut ? st::windowBgRipple : st::dialogsRippleBgActive;
					}
					return st::windowBgOver;
				};
				it->second->paint(p, innerLeft, innerTop, width(), ms, &(colorOverride()->c));
				if (it->second->empty()) {
					_ripples.erase(it);
				}
			}
			if (highlighted) {
				p.setPen(grayedOut ? st::windowSubTextFg : st::dialogsNameFgActive);
			} else if (enabled) {
				p.setPen(grayedOut ? st::windowSubTextFg : st::boxTextFg);
			} else {
				p.setPen(st::windowSubTextFg);
			}
			p.drawText(rect, _context->labelFromIndex(index), style::al_center);
		}
	}
}

void CalendarBox::Inner::mouseMoveEvent(QMouseEvent *e) {
	auto point = e->pos();
	auto row = floorclamp(point.y() - rowsTop(), st::calendarCellSize.height(), 0, _context->rowsCount());
	auto col = floorclamp(point.x() - rowsLeft(), st::calendarCellSize.width(), 0, kDaysInWeek);
	auto index = row * kDaysInWeek + col - _context->daysShift();
	if (_context->isEnabled(index)) {
		_selected = index;
		setCursor(style::cur_pointer);
	} else {
		_selected = kEmptySelection;
		setCursor(style::cur_default);
	}
}

void CalendarBox::Inner::mousePressEvent(QMouseEvent *e) {
	setPressed(_selected);
	if (_selected != kEmptySelection) {
		auto index = _selected + _context->daysShift();
		Assert(index >= 0);

		auto row = index / kDaysInWeek;
		auto col = index % kDaysInWeek;
		auto cell =
		    QRect(rowsLeft() + col * st::calendarCellSize.width(), rowsTop() + row * st::calendarCellSize.height(),
		          st::calendarCellSize.width(), st::calendarCellSize.height());
		auto it = _ripples.find(_selected);
		if (it == _ripples.cend()) {
			auto mask = Ui::RippleAnimation::ellipseMask(QSize(st::calendarCellInner, st::calendarCellInner));
			auto update = [this, cell] { rtlupdate(cell); };
			it = _ripples
			         .emplace(_selected, std::make_unique<Ui::RippleAnimation>(st::defaultRippleAnimation,
			                                                                   std::move(mask), std::move(update)))
			         .first;
		}
		auto ripplePosition = QPoint(cell.x() + (st::calendarCellSize.width() - st::calendarCellInner) / 2,
		                             cell.y() + (st::calendarCellSize.height() - st::calendarCellInner) / 2);
		it->second->add(e->pos() - ripplePosition);
	}
}

void CalendarBox::Inner::mouseReleaseEvent(QMouseEvent *e) {
	auto pressed = _pressed;
	setPressed(kEmptySelection);
	if (pressed != kEmptySelection && pressed == _selected) {
		_dateChosenCallback(_context->dateFromIndex(pressed));
	}
}

void CalendarBox::Inner::setPressed(int pressed) {
	if (_pressed != pressed) {
		if (_pressed != kEmptySelection) {
			auto it = _ripples.find(_pressed);
			if (it != _ripples.cend()) {
				it->second->lastStop();
			}
		}
		_pressed = pressed;
	}
}

CalendarBox::Inner::~Inner() = default;

class CalendarBox::Title : public TWidget, private base::Subscriber {
public:
	Title(QWidget *parent, Context *context)
	    : TWidget(parent)
	    , _context(context) {
		subscribe(_context->month(), [this](QDate date) { monthChanged(date); });
	}

protected:
	void paintEvent(QPaintEvent *e);

private:
	void monthChanged(QDate month);

	Context *_context = nullptr;

	QString _text;
	int _textWidth = 0;
};

void CalendarBox::Title::monthChanged(QDate month) {
	_text = langMonthOfYearFull(month.month(), month.year());
	_textWidth = st::calendarTitleFont->width(_text);
	update();
}

void CalendarBox::Title::paintEvent(QPaintEvent *e) {
	Painter p(this);

	p.setFont(st::calendarTitleFont);
	p.setPen(st::boxTitleFg);
	p.drawTextLeft((width() - _textWidth) / 2, (height() - st::calendarTitleFont->height) / 2, width(), _text,
	               _textWidth);
}

CalendarBox::CalendarBox(QWidget *, QDate month, QDate highlighted, Fn<void(QDate date)> callback)
    : _context(std::make_unique<Context>(month, highlighted))
    , _inner(this, _context.get())
    , _title(this, _context.get())
    , _previous(this, st::calendarPrevious)
    , _next(this, st::calendarNext)
    , _callback(std::move(callback)) {}

void CalendarBox::setMinDate(QDate date) {
	_context->setMinDate(date);
}

void CalendarBox::setMaxDate(QDate date) {
	_context->setMaxDate(date);
}

void CalendarBox::prepare() {
	_previous->setClickedCallback([this] {
		if (isPreviousEnabled()) {
			_context->skipMonth(-1);
		}
	});
	_next->setClickedCallback([this] {
		if (isNextEnabled()) {
			_context->skipMonth(1);
		}
	});

	//	_inner = setInnerWidget(object_ptr<Inner>(this, _context.get()), st::calendarScroll, st::calendarTitleHeight);
	_inner->setDateChosenCallback(std::move(_callback));

	addButton(langFactory(lng_close), [this] { closeBox(); });

	subscribe(_context->month(), [this](QDate month) { monthChanged(month); });

	_context->start();
}

bool CalendarBox::isPreviousEnabled() const {
	return (_context->minDayIndex() < 0);
}

bool CalendarBox::isNextEnabled() const {
	return (_context->maxDayIndex() >= _context->daysCount());
}

void CalendarBox::monthChanged(QDate month) {
	setDimensions(st::boxWideWidth, st::calendarTitleHeight + _inner->countHeight());
	auto previousEnabled = isPreviousEnabled();
	_previous->setIconOverride(previousEnabled ? nullptr : &st::calendarPreviousDisabled);
	_previous->setRippleColorOverride(previousEnabled ? nullptr : &st::boxBg);
	_previous->setCursor(previousEnabled ? style::cur_pointer : style::cur_default);
	auto nextEnabled = isNextEnabled();
	_next->setIconOverride(nextEnabled ? nullptr : &st::calendarNextDisabled);
	_next->setRippleColorOverride(nextEnabled ? nullptr : &st::boxBg);
	_next->setCursor(nextEnabled ? style::cur_pointer : style::cur_default);
}

void CalendarBox::resizeEvent(QResizeEvent *e) {
	_previous->moveToLeft(0, 0);
	_next->moveToRight(0, 0);
	_title->setGeometryToLeft(_previous->width(), 0, width() - _previous->width() - _next->width(),
	                          st::calendarTitleHeight);
	_inner->setGeometryToLeft(0, st::calendarTitleHeight, width(), height() - st::calendarTitleHeight);
	BoxContent::resizeEvent(e);
}

CalendarBox::~CalendarBox() = default;
