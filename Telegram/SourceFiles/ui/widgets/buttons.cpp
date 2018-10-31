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
#include "ui/widgets/buttons.h"

#include "app.h"
#include "lang/lang_instance.h"
#include "ui/effects/cross_animation.h"
#include "ui/effects/ripple_animation.h"

namespace Ui {

LinkButton::LinkButton(QWidget *parent, const QString &text, const style::LinkButton &st)
    : AbstractButton(parent)
    , _text(text)
    , _textWidth(st.font->width(_text))
    , _st(st) {
	resize(_textWidth, _st.font->height);
	setCursor(style::cur_pointer);
}

int LinkButton::naturalWidth() const {
	return _textWidth;
}

void LinkButton::paintEvent(QPaintEvent *e) {
	Painter p(this);
	auto &font = (isOver() ? _st.overFont : _st.font);
	auto &pen = (isOver() ? _st.overColor : _st.color);
	p.setFont(font);
	p.setPen(pen);
	if (_textWidth > width()) {
		p.drawText(0, font->ascent, font->elided(_text, width()));
	} else {
		p.drawText(0, font->ascent, _text);
	}
}

void LinkButton::setText(const QString &text) {
	_text = text;
	_textWidth = _st.font->width(_text);
	resize(_textWidth, _st.font->height);
	update();
}

void LinkButton::onStateChanged(State was, StateChangeSource source) {
	update();
}

RippleButton::RippleButton(QWidget *parent, const style::RippleAnimation &st)
    : AbstractButton(parent)
    , _st(st) {}

void RippleButton::setForceRippled(bool rippled, SetForceRippledWay way) {
	if (_forceRippled != rippled) {
		_forceRippled = rippled;
		if (_forceRippled) {
			ensureRipple();
			if (_ripple->empty()) {
				_ripple->addFading();
			} else {
				_ripple->lastUnstop();
			}
		} else if (_ripple) {
			_ripple->lastStop();
		}
	}
	if (way == SetForceRippledWay::SkipAnimation && _ripple) {
		_ripple->lastFinish();
	}
	update();
}

void RippleButton::paintRipple(QPainter &p, int x, int y, TimeMs ms, const QColor *colorOverride) {
	if (_ripple) {
		_ripple->paint(p, x, y, width(), ms, colorOverride);
		if (_ripple->empty()) {
			_ripple.reset();
		}
	}
}

void RippleButton::onStateChanged(State was, StateChangeSource source) {
	update();

	auto wasDown = static_cast<bool>(was & StateFlag::Down);
	auto down = isDown();
	if (!_st.showDuration || down == wasDown || _forceRippled) {
		return;
	}

	if (down && (source == StateChangeSource::ByPress)) {
		// Start a ripple only from mouse press.
		auto position = prepareRippleStartPosition();
		if (position != DisabledRippleStartPosition()) {
			ensureRipple();
			_ripple->add(position);
		}
	} else if (!down && _ripple) {
		// Finish ripple anyway.
		_ripple->lastStop();
	}
}

void RippleButton::ensureRipple() {
	if (!_ripple) {
		_ripple = std::make_unique<RippleAnimation>(_st, prepareRippleMask(), [this] { update(); });
	}
}

QImage RippleButton::prepareRippleMask() const {
	return RippleAnimation::rectMask(size());
}

QPoint RippleButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos());
}

void RippleButton::resetRipples() {
	_ripple.reset();
}

RippleButton::~RippleButton() = default;

FlatButton::FlatButton(QWidget *parent, const QString &text, const style::FlatButton &st)
    : RippleButton(parent, st.ripple)
    , _text(text)
    , _st(st) {
	if (_st.width < 0) {
		_width = textWidth() - _st.width;
	} else if (!_st.width) {
		_width = textWidth() + _st.height - _st.font->height;
	} else {
		_width = _st.width;
	}
	resize(_width, _st.height);
}

void FlatButton::setText(const QString &text) {
	_text = text;
	update();
}

void FlatButton::setWidth(qint32 w) {
	_width = w;
	if (_width < 0) {
		_width = textWidth() - _st.width;
	} else if (!_width) {
		_width = textWidth() + _st.height - _st.font->height;
	}
	resize(_width, height());
}

qint32 FlatButton::textWidth() const {
	return _st.font->width(_text);
}

void FlatButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);
	update();
}

void FlatButton::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	QRect r(0, height() - _st.height, width(), _st.height);
	p.fillRect(r, isOver() ? _st.overBgColor : _st.bgColor);

	paintRipple(p, 0, 0, getms());

	p.setFont(isOver() ? _st.overFont : _st.font);
	p.setRenderHint(QPainter::TextAntialiasing);
	p.setPen(isOver() ? _st.overColor : _st.color);

	r.setTop(_st.textTop);
	p.drawText(r, _text, style::al_top);
}

class RoundButton::Numbers {
public:
	Numbers(const style::RoundButton &st, Fn<void()> animationCallback);

	void setWidthChangedCallback(Fn<void()> callback) {
		_widthChangedCallback = std::move(callback);
	}
	void setText(const QString &text, int value);
	void stepAnimation(TimeMs ms);
	void finishAnimation();

	void paint(Painter &p, int x, int y, int outerWidth);
	int countWidth() const;

private:
	struct Digit {
		QChar from = 0;
		QChar to = 0;
		int fromWidth = 0;
		int toWidth = 0;
	};

	void animationCallback();
	void realSetText(QString text, int value);

	const style::RoundButton &_st;

	QList<Digit> _digits;
	int _digitWidth = 0;

	int _fromWidth = 0;
	int _toWidth = 0;

	Animation _a_ready;
	QString _delayedText;
	int _delayedValue = 0;

	int _value = 0;
	bool _growing = false;

	Fn<void()> _animationCallback;
	Fn<void()> _widthChangedCallback;
};

RoundButton::Numbers::Numbers(const style::RoundButton &st, Fn<void()> animationCallback)
    : _st(st)
    , _animationCallback(std::move(animationCallback)) {
	for (auto ch = '0'; ch != '9'; ++ch) {
		accumulate_max(_digitWidth, _st.font->m.width(ch));
	}
}

void RoundButton::Numbers::setText(const QString &text, int value) {
	if (_a_ready.animating(getms())) {
		_delayedText = text;
		_delayedValue = value;
	} else {
		realSetText(text, value);
	}
}

void RoundButton::Numbers::animationCallback() {
	if (_animationCallback) {
		_animationCallback();
	}
	if (_widthChangedCallback) {
		_widthChangedCallback();
	}
	if (!_a_ready.animating()) {
		if (!_delayedText.isEmpty()) {
			setText(_delayedText, _delayedValue);
		}
	}
}

void RoundButton::Numbers::realSetText(QString text, int value) {
	_delayedText = QString();
	_delayedValue = 0;

	_growing = (value > _value);
	_value = value;

	auto newSize = text.size();
	while (_digits.size() < newSize) {
		_digits.push_front(Digit());
	}
	while (_digits.size() > newSize && !_digits.front().to.unicode()) {
		_digits.pop_front();
	}
	auto oldSize = _digits.size();
	auto animating = false;
	for (auto i = 0, size = _digits.size(); i != size; ++i) {
		auto &digit = _digits[i];
		digit.from = digit.to;
		digit.fromWidth = digit.toWidth;
		digit.to = (newSize + i < size) ? QChar(0) : text[newSize + i - size];
		digit.toWidth = digit.to.unicode() ? _st.font->m.width(digit.to) : 0;
		if (digit.from != digit.to) {
			animating = true;
		}
		if (!digit.from.unicode()) {
			--oldSize;
		}
	}
	_fromWidth = oldSize * _digitWidth;
	_toWidth = newSize * _digitWidth;
	if (animating) {
		_a_ready.start([this] { animationCallback(); }, 0., 1., _st.numbersDuration);
	}
}

int RoundButton::Numbers::countWidth() const {
	return anim::interpolate(_fromWidth, _toWidth, anim::easeOutCirc(1., _a_ready.current(1.)));
}

void RoundButton::Numbers::stepAnimation(TimeMs ms) {
	_a_ready.step(ms);
}

void RoundButton::Numbers::finishAnimation() {
	auto width = countWidth();
	_a_ready.finish();
	if (_widthChangedCallback && countWidth() != width) {
		_widthChangedCallback();
	}
	if (!_delayedText.isEmpty()) {
		setText(_delayedText, _delayedValue);
	}
}

void RoundButton::Numbers::paint(Painter &p, int x, int y, int outerWidth) {
	auto digitsCount = _digits.size();
	if (!digitsCount) return;

	auto progress = anim::easeOutCirc(1., _a_ready.current(1.));
	auto width = anim::interpolate(_fromWidth, _toWidth, progress);

	QString singleChar('0');
	if (rtl()) x = outerWidth - x - width;
	x += width - _digits.size() * _digitWidth;
	auto fromTop = anim::interpolate(0, _st.font->height, progress) * (_growing ? 1 : -1);
	auto toTop = anim::interpolate(_st.font->height, 0, progress) * (_growing ? -1 : 1);
	for (auto i = 0; i != digitsCount; ++i) {
		auto &digit = _digits[i];
		auto from = digit.from;
		auto to = digit.to;
		if (from.unicode()) {
			p.setOpacity(1. - progress);
			singleChar[0] = from;
			p.drawText(x + (_digitWidth - digit.fromWidth) / 2, y + fromTop + _st.font->ascent, singleChar);
		}
		if (to.unicode()) {
			p.setOpacity(progress);
			singleChar[0] = to;
			p.drawText(x + (_digitWidth - digit.toWidth) / 2, y + toTop + _st.font->ascent, singleChar);
		}
		x += _digitWidth;
	}
	p.setOpacity(1.);
}

RoundButton::RoundButton(QWidget *parent, Fn<QString()> textFactory, const style::RoundButton &st)
    : RippleButton(parent, st.ripple)
    , _textFactory(std::move(textFactory))
    , _st(st) {
	subscribe(Lang::Current().updated(), [this] { refreshText(); });
	refreshText();
}

void RoundButton::setTextTransform(TextTransform transform) {
	_transform = transform;
	refreshText();
}

void RoundButton::setText(Fn<QString()> textFactory) {
	_textFactory = std::move(textFactory);
	refreshText();
}

void RoundButton::setNumbersText(const QString &numbersText, int numbers) {
	if (numbersText.isEmpty()) {
		_numbers.reset();
	} else {
		if (!_numbers) {
			_numbers = std::make_unique<Numbers>(_st, [this] { numbersAnimationCallback(); });
		}
		_numbers->setText(numbersText, numbers);
	}
	refreshText();
}

void RoundButton::setWidthChangedCallback(Fn<void()> callback) {
	if (!_numbers) {
		_numbers = std::make_unique<Numbers>(_st, [this] { numbersAnimationCallback(); });
	}
	_numbers->setWidthChangedCallback(std::move(callback));
}

void RoundButton::stepNumbersAnimation(TimeMs ms) {
	if (_numbers) {
		_numbers->stepAnimation(ms);
	}
}

void RoundButton::finishNumbersAnimation() {
	if (_numbers) {
		_numbers->finishAnimation();
	}
}

void RoundButton::numbersAnimationCallback() {
	resizeToText();
	update();
}

void RoundButton::setFullWidth(int newFullWidth) {
	_fullWidthOverride = newFullWidth;
	resizeToText();
}

void RoundButton::refreshText() {
	_text = computeFullText();
	_textWidth = _text.isEmpty() ? 0 : _st.font->width(_text);

	resizeToText();
	update();
}

QString RoundButton::computeFullText() const {
	auto result = _textFactory ? _textFactory() : QString();
	return (_transform == TextTransform::ToUpper) ? result.toUpper() : result;
}

void RoundButton::resizeToText() {
	int innerWidth = contentWidth();
	if (_fullWidthOverride < 0) {
		resize(innerWidth - _fullWidthOverride, _st.height + _st.padding.top() + _st.padding.bottom());
	} else if (_st.width <= 0) {
		resize(innerWidth - _st.width + _st.padding.left() + _st.padding.right(),
		       _st.height + _st.padding.top() + _st.padding.bottom());
	} else {
		if (_st.width < innerWidth + (_st.height - _st.font->height)) {
			_text = _st.font->elided(computeFullText(), std::max(_st.width - (_st.height - _st.font->height), 1));
			_textWidth = _st.font->width(_text);
		}
		resize(_st.width + _st.padding.left() + _st.padding.right(),
		       _st.height + _st.padding.top() + _st.padding.bottom());
	}
}

int RoundButton::contentWidth() const {
	auto result = _textWidth;
	if (_numbers) {
		result += (result ? _st.numbersSkip : 0) + _numbers->countWidth();
	}
	return result;
}

void RoundButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto innerWidth = contentWidth();
	auto rounded = rect().marginsRemoved(_st.padding);
	if (_fullWidthOverride < 0) {
		rounded = QRect(0, rounded.top(), innerWidth - _fullWidthOverride, rounded.height());
	}
	App::roundRect(p, myrtlrect(rounded), _st.textBg, ImageRoundRadius::Small);

	auto over = isOver();
	auto down = isDown();
	if (over || down) {
		App::roundRect(p, myrtlrect(rounded), _st.textBgOver, ImageRoundRadius::Small);
	}

	auto ms = getms();
	paintRipple(p, rounded.x(), rounded.y(), ms);

	p.setFont(_st.font);
	int textLeft = _st.padding.left() + ((width() - innerWidth - _st.padding.left() - _st.padding.right()) / 2);
	if (_fullWidthOverride < 0) {
		textLeft = -_fullWidthOverride / 2;
	}
	int textTop = _st.padding.top() + _st.textTop;
	if (!_text.isEmpty()) {
		p.setPen((over || down) ? _st.textFgOver : _st.textFg);
		p.drawTextLeft(textLeft, textTop, width(), _text);
	}
	if (_numbers) {
		textLeft += _textWidth + (_textWidth ? _st.numbersSkip : 0);
		p.setPen((over || down) ? _st.numbersTextFgOver : _st.numbersTextFg);
		_numbers->paint(p, textLeft, textTop, width());
	}
	_st.icon.paint(p, QPoint(_st.padding.left(), _st.padding.top()), width());
}

QImage RoundButton::prepareRippleMask() const {
	auto innerWidth = contentWidth();
	auto rounded = rtlrect(rect().marginsRemoved(_st.padding), width());
	if (_fullWidthOverride < 0) {
		rounded = QRect(0, rounded.top(), innerWidth - _fullWidthOverride, rounded.height());
	}
	return RippleAnimation::roundRectMask(rounded.size(), st::buttonRadius);
}

QPoint RoundButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos()) - QPoint(_st.padding.left(), _st.padding.top());
}

RoundButton::~RoundButton() = default;

IconButton::IconButton(QWidget *parent, const style::IconButton &st)
    : RippleButton(parent, st.ripple)
    , _st(st) {
	resize(_st.width, _st.height);
}

void IconButton::setIconOverride(const style::icon *iconOverride, const style::icon *iconOverOverride) {
	_iconOverride = iconOverride;
	_iconOverrideOver = iconOverOverride;
	update();
}

void IconButton::setRippleColorOverride(const style::color *colorOverride) {
	_rippleColorOverride = colorOverride;
}

void IconButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();

	paintRipple(p, _st.rippleAreaPosition.x(), _st.rippleAreaPosition.y(), ms,
	            _rippleColorOverride ? &(*_rippleColorOverride)->c : nullptr);

	auto down = isDown();
	auto overIconOpacity = (down || forceRippled()) ? 1. : _a_over.current(getms(), isOver() ? 1. : 0.);
	auto overIcon = [this] {
		if (_iconOverrideOver) {
			return _iconOverrideOver;
		} else if (!_st.iconOver.empty()) {
			return &_st.iconOver;
		} else if (_iconOverride) {
			return _iconOverride;
		}
		return &_st.icon;
	};
	auto justIcon = [this] {
		if (_iconOverride) {
			return _iconOverride;
		}
		return &_st.icon;
	};
	auto icon = (overIconOpacity == 1.) ? overIcon() : justIcon();
	auto position = _st.iconPosition;
	if (position.x() < 0) {
		position.setX((width() - icon->width()) / 2);
	}
	if (position.y() < 0) {
		position.setY((height() - icon->height()) / 2);
	}
	icon->paint(p, position, width());
	if (overIconOpacity > 0. && overIconOpacity < 1.) {
		auto iconOver = overIcon();
		if (iconOver != icon) {
			p.setOpacity(overIconOpacity);
			iconOver->paint(p, position, width());
		}
	}
}

void IconButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	auto over = isOver();
	auto wasOver = static_cast<bool>(was & StateFlag::Over);
	if (over != wasOver) {
		if (_st.duration) {
			auto from = over ? 0. : 1.;
			auto to = over ? 1. : 0.;
			_a_over.start([this] { update(); }, from, to, _st.duration);
		} else {
			update();
		}
	}
}

QPoint IconButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition;
}

QImage IconButton::prepareRippleMask() const {
	return RippleAnimation::ellipseMask(QSize(_st.rippleAreaSize, _st.rippleAreaSize));
}

LeftOutlineButton::LeftOutlineButton(QWidget *parent, const QString &text, const style::OutlineButton &st)
    : RippleButton(parent, st.ripple)
    , _text(text)
    , _fullText(text)
    , _textWidth(st.font->width(_text))
    , _fullTextWidth(_textWidth)
    , _st(st) {
	resizeToWidth(_textWidth + _st.padding.left() + _st.padding.right());

	setCursor(style::cur_pointer);
}

void LeftOutlineButton::setText(const QString &text) {
	_text = text;
	_fullText = text;
	_fullTextWidth = _textWidth = _st.font->width(_text);
	resizeToWidth(width());
	update();
}

int LeftOutlineButton::resizeGetHeight(int newWidth) {
	int availableWidth = std::max(newWidth - _st.padding.left() - _st.padding.right(), 1);
	if ((availableWidth < _fullTextWidth) || (_textWidth < availableWidth)) {
		_text = _st.font->elided(_fullText, availableWidth);
		_textWidth = _st.font->width(_text);
	}
	return _st.padding.top() + _st.font->height + _st.padding.bottom();
}

void LeftOutlineButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto over = isOver();
	auto down = isDown();
	if (width() > _st.outlineWidth) {
		p.fillRect(rtlrect(_st.outlineWidth, 0, width() - _st.outlineWidth, height(), width()),
		           (over || down) ? _st.textBgOver : _st.textBg);
		paintRipple(p, 0, 0, getms());
		p.fillRect(rtlrect(0, 0, _st.outlineWidth, height(), width()),
		           (over || down) ? _st.outlineFgOver : _st.outlineFg);
	}
	p.setFont(_st.font);
	p.setPen((over || down) ? _st.textFgOver : _st.textFg);
	p.drawTextLeft(_st.padding.left(), _st.padding.top(), width(), _text, _textWidth);
}

CrossButton::CrossButton(QWidget *parent, const style::CrossButton &st)
    : RippleButton(parent, st.ripple)
    , _st(st)
    , _a_loading(animation(this, &CrossButton::step_loading)) {
	resize(_st.width, _st.height);
	setCursor(style::cur_pointer);
	hide();
}

void CrossButton::step_loading(TimeMs ms, bool timer) {
	if (stopLoadingAnimation(ms)) {
		_a_loading.stop();
	}
	if (timer) {
		update();
	}
}

void CrossButton::toggleAnimated(bool visible) {
	if (_shown == visible) {
		return;
	}
	_shown = visible;
	if (isHidden()) {
		show();
	}
	_a_show.start([this] { animationCallback(); }, _shown ? 0. : 1., _shown ? 1. : 0., _st.duration);
}

void CrossButton::animationCallback() {
	update();
	if (!_shown && !_a_show.animating()) {
		hide();
	}
}

void CrossButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();
	auto over = isOver();
	auto shown = _a_show.current(ms, _shown ? 1. : 0.);
	p.setOpacity(shown);

	paintRipple(p, _st.crossPosition.x(), _st.crossPosition.y(), ms);

	auto loading = 0.;
	if (_a_loading.animating()) {
		if (stopLoadingAnimation(ms)) {
			_a_loading.stop();
		} else {
			loading = ((ms - _loadingStartMs) % _st.loadingPeriod) / double(_st.loadingPeriod);
		}
	}
	CrossAnimation::paint(p, _st.cross, over ? _st.crossFgOver : _st.crossFg, _st.crossPosition.x(),
	                      _st.crossPosition.y(), width(), shown, loading);
}

bool CrossButton::stopLoadingAnimation(TimeMs ms) {
	if (!_loadingStopMs) {
		return false;
	}
	auto stopPeriod = (_loadingStopMs - _loadingStartMs) / _st.loadingPeriod;
	auto currentPeriod = (ms - _loadingStartMs) / _st.loadingPeriod;
	if (currentPeriod != stopPeriod) {
		Assert(currentPeriod > stopPeriod);
		return true;
	}
	return false;
}

void CrossButton::setLoadingAnimation(bool enabled) {
	if (enabled) {
		_loadingStopMs = 0;
		if (!_a_loading.animating()) {
			_loadingStartMs = getms();
			_a_loading.start();
		}
	} else if (_a_loading.animating()) {
		_loadingStopMs = getms();
		if (!((_loadingStopMs - _loadingStartMs) % _st.loadingPeriod)) {
			_a_loading.stop();
		}
	}
}

void CrossButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	auto over = isOver();
	auto wasOver = static_cast<bool>(was & StateFlag::Over);
	if (over != wasOver) {
		update();
	}
}

QPoint CrossButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos()) - _st.crossPosition;
}

QImage CrossButton::prepareRippleMask() const {
	return RippleAnimation::ellipseMask(QSize(_st.cross.size, _st.cross.size));
}

} // namespace Ui
