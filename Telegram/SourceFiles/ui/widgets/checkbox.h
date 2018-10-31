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

#include "styles/style_widgets.h"
#include "ui/text/text.h"
#include "ui/widgets/buttons.h"

namespace Ui {

class AbstractCheckView {
public:
	AbstractCheckView(int duration, bool checked, Fn<void()> updateCallback);

	void setCheckedFast(bool checked);
	void setCheckedAnimated(bool checked);
	void finishAnimation();
	void setUpdateCallback(Fn<void()> updateCallback);
	bool checked() const {
		return _checked;
	}
	double currentAnimationValue(TimeMs ms);

	virtual QSize getSize() const = 0;

	// Zero instead of ms value means that animation was already updated for this time.
	// It can be passed to currentAnimationValue() safely.
	virtual void paint(Painter &p, int left, int top, int outerWidth, TimeMs ms) = 0;
	virtual QImage prepareRippleMask() const = 0;
	virtual bool checkRippleStartPosition(QPoint position) const = 0;

	void paint(Painter &p, int left, int top, int outerWidth) {
		// Pass zero in ms if the animation was already updated for this time.
		paint(p, left, top, outerWidth, 0);
	}

	virtual ~AbstractCheckView() = default;

private:
	int _duration = 0;
	bool _checked = false;
	Fn<void()> _updateCallback;
	Animation _toggleAnimation;
};

class CheckView : public AbstractCheckView {
public:
	CheckView(const style::Check &st, bool checked, Fn<void()> updateCallback);

	void setStyle(const style::Check &st);

	QSize getSize() const override;
	void paint(Painter &p, int left, int top, int outerWidth, TimeMs ms) override;
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;

private:
	QSize rippleSize() const;

	not_null<const style::Check *> _st;
};

class RadioView : public AbstractCheckView {
public:
	RadioView(const style::Radio &st, bool checked, Fn<void()> updateCallback);

	void setStyle(const style::Radio &st);

	QSize getSize() const override;
	void paint(Painter &p, int left, int top, int outerWidth, TimeMs ms) override;
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;

private:
	QSize rippleSize() const;

	not_null<const style::Radio *> _st;
};

class ToggleView : public AbstractCheckView {
public:
	ToggleView(const style::Toggle &st, bool checked, Fn<void()> updateCallback);

	void setStyle(const style::Toggle &st);

	QSize getSize() const override;
	void paint(Painter &p, int left, int top, int outerWidth, TimeMs ms) override;
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;

private:
	void paintXV(Painter &p, int left, int top, int outerWidth, double toggled, const QBrush &brush);
	QSize rippleSize() const;

	not_null<const style::Toggle *> _st;
};

class Checkbox : public RippleButton {
public:
	Checkbox(QWidget *parent, const QString &text, bool checked = false,
	         const style::Checkbox &st = st::defaultCheckbox, const style::Check &checkSt = st::defaultCheck);
	Checkbox(QWidget *parent, const QString &text, bool checked, const style::Checkbox &st,
	         const style::Toggle &toggleSt);
	Checkbox(QWidget *parent, const QString &text, const style::Checkbox &st, std::unique_ptr<AbstractCheckView> check);

	void setText(const QString &text);

	bool checked() const;
	enum class NotifyAboutChange {
		Notify,
		DontNotify,
	};
	void setChecked(bool checked, NotifyAboutChange notify = NotifyAboutChange::Notify);
	base::Observable<bool> checkedChanged;

	void finishAnimations();

	QMargins getMargins() const override {
		return _st.margin;
	}
	int naturalWidth() const override;

protected:
	void paintEvent(QPaintEvent *e) override;

	void onStateChanged(State was, StateChangeSource source) override;
	int resizeGetHeight(int newWidth) override;

	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

	virtual void handlePress();

	void updateCheck() {
		rtlupdate(_checkRect);
	}

private:
	void resizeToText();
	QPixmap grabCheckCache() const;

	const style::Checkbox &_st;
	std::unique_ptr<AbstractCheckView> _check;
	QPixmap _checkCache;

	Text _text;
	QRect _checkRect;
};

class Radiobutton;

class RadiobuttonGroup {
public:
	RadiobuttonGroup() = default;
	RadiobuttonGroup(int value)
	    : _value(value)
	    , _hasValue(true) {}

	void setChangedCallback(Fn<void(int value)> callback) {
		_changedCallback = std::move(callback);
	}

	bool hasValue() const {
		return _hasValue;
	}
	int value() const {
		return _value;
	}
	void setValue(int value);

private:
	friend class Radiobutton;
	void registerButton(Radiobutton *button) {
		if (!base::contains(_buttons, button)) {
			_buttons.push_back(button);
		}
	}
	void unregisterButton(Radiobutton *button) {
		_buttons.erase(std::remove(_buttons.begin(), _buttons.end(), button), _buttons.end());
	}

	int _value = 0;
	bool _hasValue = false;
	Fn<void(int value)> _changedCallback;
	std::vector<Radiobutton *> _buttons;
};

class Radiobutton : public Checkbox, private base::Subscriber {
public:
	Radiobutton(QWidget *parent, const std::shared_ptr<RadiobuttonGroup> &group, int value, const QString &text,
	            const style::Checkbox &st = st::defaultCheckbox, const style::Radio &radioSt = st::defaultRadio);
	~Radiobutton();

protected:
	void handlePress() override;

private:
	// Hide the names from Checkbox.
	bool checked() const;
	void setChecked(bool checked, NotifyAboutChange notify);
	void checkedChanged();
	Checkbox *checkbox() {
		return this;
	}
	const Checkbox *checkbox() const {
		return this;
	}

	friend class RadiobuttonGroup;
	void handleNewGroupValue(int value);

	std::shared_ptr<RadiobuttonGroup> _group;
	int _value = 0;
};

template <typename Enum> class Radioenum;

template <typename Enum> class RadioenumGroup {
public:
	RadioenumGroup() = default;
	RadioenumGroup(Enum value)
	    : _group(static_cast<int>(value)) {}

	template <typename Callback> void setChangedCallback(Callback &&callback) {
		_group.setChangedCallback([callback](int value) { callback(static_cast<Enum>(value)); });
	}

	bool hasValue() const {
		return _group.hasValue();
	}
	Enum value() const {
		return static_cast<Enum>(_group.value());
	}
	void setValue(Enum value) {
		_group.setValue(static_cast<int>(value));
	}

private:
	template <typename OtherEnum> friend class Radioenum;

	RadiobuttonGroup _group;
};

template <typename Enum> class Radioenum : public Radiobutton {
public:
	Radioenum(QWidget *parent, const std::shared_ptr<RadioenumGroup<Enum>> &group, Enum value, const QString &text,
	          const style::Checkbox &st = st::defaultCheckbox)
	    : Radiobutton(parent, std::shared_ptr<RadiobuttonGroup>(group, &group->_group), static_cast<int>(value), text,
	                  st) {}
};

} // namespace Ui
