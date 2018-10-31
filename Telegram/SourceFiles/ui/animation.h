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

#define _USE_MATH_DEFINES // for M_PI in MSVC (GCC and Clang has it as extension)

#include <QBrush>
#include <QColor>
#include <QPainterPath>
#include <QPen>
#include <QTimer>
#include <cmath>
#include <cstdint>

#include "core/basic_types.h"
#include "core/utils.h"
#include "mtproto/facade.h"
#include "style/style_core_types.h"

namespace Media {
namespace Clip {

class Reader;
class ReaderPointer {
public:
	ReaderPointer(std::nullptr_t = nullptr) {}
	explicit ReaderPointer(Reader *pointer)
	    : _pointer(pointer) {}
	ReaderPointer(const ReaderPointer &other) = delete;
	ReaderPointer &operator=(const ReaderPointer &other) = delete;
	ReaderPointer(ReaderPointer &&other)
	    : _pointer(base::take(other._pointer)) {}
	ReaderPointer &operator=(ReaderPointer &&other) {
		swap(other);
		return *this;
	}
	void swap(ReaderPointer &other) {
		qSwap(_pointer, other._pointer);
	}
	Reader *get() const {
		return valid() ? _pointer : nullptr;
	}
	Reader *operator->() const {
		return get();
	}
	void setBad() {
		reset();
		_pointer = BadPointer;
	}
	void reset() {
		ReaderPointer temp;
		swap(temp);
	}
	bool isBad() const {
		return (_pointer == BadPointer);
	}
	bool valid() const {
		return _pointer && !isBad();
	}
	explicit operator bool() const {
		return valid();
	}
	static inline ReaderPointer Bad() {
		ReaderPointer result;
		result.setBad();
		return result;
	}
	~ReaderPointer();

private:
	Reader *_pointer = nullptr;
	static Reader *const BadPointer;
};

class Manager;

enum Notification {
	NotificationReinit,
	NotificationRepaint,
};

} // namespace Clip
} // namespace Media

namespace anim {

using transition = Fn<double(double delta, double dt)>;

extern transition linear;
extern transition sineInOut;
extern transition halfSine;
extern transition easeOutBack;
extern transition easeInCirc;
extern transition easeOutCirc;
extern transition easeInCubic;
extern transition easeOutCubic;
extern transition easeInQuint;
extern transition easeOutQuint;

inline transition bumpy(double bump) {
	auto dt0 = (bump - sqrt(bump * (bump - 1.)));
	auto k = (1 / (2 * dt0 - 1));
	return [bump, dt0, k](double delta, double dt) { return delta * (bump - k * (dt - dt0) * (dt - dt0)); };
}

// Basic animated value.
class value {
public:
	using ValueType = double;

	value() = default;
	value(double from)
	    : _cur(from)
	    , _from(from) {}
	value(double from, double to)
	    : _cur(from)
	    , _from(from)
	    , _delta(to - from) {}
	void start(double to) {
		_from = _cur;
		_delta = to - _from;
	}
	void restart() {
		_delta = _from + _delta - _cur;
		_from = _cur;
	}

	double from() const {
		return _from;
	}
	double current() const {
		return _cur;
	}
	double to() const {
		return _from + _delta;
	}
	void add(double delta) {
		_from += delta;
		_cur += delta;
	}
	value &update(double dt, transition func) {
		_cur = _from + func(_delta, dt);
		return *this;
	}
	void finish() {
		_cur = _from + _delta;
		_from = _cur;
		_delta = 0;
	}

private:
	double _cur = 0.;
	double _from = 0.;
	double _delta = 0.;
};

void startManager();
void stopManager();
void registerClipManager(Media::Clip::Manager *manager);

FORCE_INLINE int interpolate(int a, int b, double b_ratio) {
	return qRound(a + double(b - a) * b_ratio);
}

#ifdef ARCH_CPU_32_BITS
#define SHIFTED_USE_32BIT
#endif // ARCH_CPU_32_BITS

#ifdef SHIFTED_USE_32BIT

using ShiftedMultiplier = quint32;

struct Shifted {
	Shifted() = default;
	Shifted(quint32 low, quint32 high)
	    : low(low)
	    , high(high) {}
	quint32 low = 0;
	quint32 high = 0;
};

FORCE_INLINE Shifted operator+(Shifted a, Shifted b) {
	return Shifted(a.low + b.low, a.high + b.high);
}

FORCE_INLINE Shifted operator*(Shifted shifted, ShiftedMultiplier multiplier) {
	return Shifted(shifted.low * multiplier, shifted.high * multiplier);
}

FORCE_INLINE Shifted operator*(ShiftedMultiplier multiplier, Shifted shifted) {
	return Shifted(shifted.low * multiplier, shifted.high * multiplier);
}

FORCE_INLINE Shifted shifted(quint32 components) {
	return Shifted((components & 0x000000FFU) | ((components & 0x0000FF00U) << 8),
	               ((components & 0x00FF0000U) >> 16) | ((components & 0xFF000000U) >> 8));
}

FORCE_INLINE quint32 unshifted(Shifted components) {
	return ((components.low & 0x0000FF00U) >> 8) | ((components.low & 0xFF000000U) >> 16) |
	       ((components.high & 0x0000FF00U) << 8) | (components.high & 0xFF000000U);
}

FORCE_INLINE Shifted reshifted(Shifted components) {
	return Shifted((components.low >> 8) & 0x00FF00FFU, (components.high >> 8) & 0x00FF00FFU);
}

FORCE_INLINE Shifted shifted(QColor color) {
	// Make it premultiplied.
	auto alpha = static_cast<quint32>((color.alpha() & 0xFF) + 1);
	auto components =
	    Shifted(static_cast<quint32>(color.blue() & 0xFF) | (static_cast<quint32>(color.green() & 0xFF) << 16),
	            static_cast<quint32>(color.red() & 0xFF) | (static_cast<quint32>(255) << 16));
	return reshifted(components * alpha);
}

FORCE_INLINE quint32 getPremultiplied(QColor color) {
	// Make it premultiplied.
	auto alpha = static_cast<quint32>((color.alpha() & 0xFF) + 1);
	auto components =
	    Shifted(static_cast<quint32>(color.blue() & 0xFF) | (static_cast<quint32>(color.green() & 0xFF) << 16),
	            static_cast<quint32>(color.red() & 0xFF) | (static_cast<quint32>(255) << 16));
	return unshifted(components * alpha);
}

FORCE_INLINE quint32 getAlpha(Shifted components) {
	return (components.high & 0x00FF0000U) >> 16;
}

FORCE_INLINE Shifted non_premultiplied(QColor color) {
	return Shifted(static_cast<quint32>(color.blue() & 0xFF) | (static_cast<quint32>(color.green() & 0xFF) << 16),
	               static_cast<quint32>(color.red() & 0xFF) | (static_cast<quint32>(color.alpha() & 0xFF) << 16));
}

FORCE_INLINE QColor color(QColor a, QColor b, double b_ratio) {
	auto bOpacity = snap(interpolate(0, 255, b_ratio), 0, 255) + 1;
	auto aOpacity = (256 - bOpacity);
	auto components = (non_premultiplied(a) * aOpacity + non_premultiplied(b) * bOpacity);
	return {
	    static_cast<int>((components.high >> 8) & 0xFF),
	    static_cast<int>((components.low >> 24) & 0xFF),
	    static_cast<int>((components.low >> 8) & 0xFF),
	    static_cast<int>((components.high >> 24) & 0xFF),
	};
}

#else // SHIFTED_USE_32BIT

using ShiftedMultiplier = quint64;

struct Shifted {
	Shifted() = default;
	Shifted(quint32 value)
	    : value(value) {}
	Shifted(quint64 value)
	    : value(value) {}
	quint64 value = 0;
};

FORCE_INLINE Shifted operator+(Shifted a, Shifted b) {
	return Shifted(a.value + b.value);
}

FORCE_INLINE Shifted operator*(Shifted shifted, ShiftedMultiplier multiplier) {
	return Shifted(shifted.value * multiplier);
}

FORCE_INLINE Shifted operator*(ShiftedMultiplier multiplier, Shifted shifted) {
	return Shifted(shifted.value * multiplier);
}

FORCE_INLINE Shifted shifted(quint32 components) {
	auto wide = static_cast<quint64>(components);
	return (wide & Q_UINT64_C(0x00000000000000FF)) | ((wide & Q_UINT64_C(0x000000000000FF00)) << 8) |
	       ((wide & Q_UINT64_C(0x0000000000FF0000)) << 16) | ((wide & Q_UINT64_C(0x00000000FF000000)) << 24);
}

FORCE_INLINE quint32 unshifted(Shifted components) {
	return static_cast<quint32>((components.value & Q_UINT64_C(0x000000000000FF00)) >> 8) |
	       static_cast<quint32>((components.value & Q_UINT64_C(0x00000000FF000000)) >> 16) |
	       static_cast<quint32>((components.value & Q_UINT64_C(0x0000FF0000000000)) >> 24) |
	       static_cast<quint32>((components.value & Q_UINT64_C(0xFF00000000000000)) >> 32);
}

FORCE_INLINE Shifted reshifted(Shifted components) {
	return (components.value >> 8) & Q_UINT64_C(0x00FF00FF00FF00FF);
}

FORCE_INLINE Shifted shifted(QColor color) {
	// Make it premultiplied.
	auto alpha = static_cast<quint64>((color.alpha() & 0xFF) + 1);
	auto components = static_cast<quint64>(color.blue() & 0xFF) | (static_cast<quint64>(color.green() & 0xFF) << 16) |
	                  (static_cast<quint64>(color.red() & 0xFF) << 32) | (static_cast<quint64>(255) << 48);
	return reshifted(components * alpha);
}

FORCE_INLINE quint32 getPremultiplied(QColor color) {
	// Make it premultiplied.
	auto alpha = static_cast<quint64>((color.alpha() & 0xFF) + 1);
	auto components = static_cast<quint64>(color.blue() & 0xFF) | (static_cast<quint64>(color.green() & 0xFF) << 16) |
	                  (static_cast<quint64>(color.red() & 0xFF) << 32) | (static_cast<quint64>(255) << 48);
	return unshifted(components * alpha);
}

FORCE_INLINE quint32 getAlpha(Shifted components) {
	return (components.value & 0x00FF000000000000ULL) >> 48;
}

FORCE_INLINE Shifted non_premultiplied(QColor color) {
	return static_cast<quint64>(color.blue() & 0xFF) | (static_cast<quint64>(color.green() & 0xFF) << 16) |
	       (static_cast<quint64>(color.red() & 0xFF) << 32) | (static_cast<quint64>(color.alpha() & 0xFF) << 48);
}

FORCE_INLINE QColor color(QColor a, QColor b, double b_ratio) {
	auto bOpacity = snap(interpolate(0, 255, b_ratio), 0, 255) + 1;
	auto aOpacity = (256 - bOpacity);
	auto components = (non_premultiplied(a) * aOpacity + non_premultiplied(b) * bOpacity);
	return {
	    static_cast<int>((components.value >> 40) & 0xFF),
	    static_cast<int>((components.value >> 24) & 0xFF),
	    static_cast<int>((components.value >> 8) & 0xFF),
	    static_cast<int>((components.value >> 56) & 0xFF),
	};
}

#endif // SHIFTED_USE_32BIT

FORCE_INLINE QColor color(style::color a, QColor b, double b_ratio) {
	return color(a->c, b, b_ratio);
}

FORCE_INLINE QColor color(QColor a, style::color b, double b_ratio) {
	return color(a, b->c, b_ratio);
}

FORCE_INLINE QColor color(style::color a, style::color b, double b_ratio) {
	return color(a->c, b->c, b_ratio);
}

FORCE_INLINE QPen pen(QColor a, QColor b, double b_ratio) {
	return color(a, b, b_ratio);
}

FORCE_INLINE QPen pen(style::color a, QColor b, double b_ratio) {
	return (b_ratio > 0) ? pen(a->c, b, b_ratio) : a;
}

FORCE_INLINE QPen pen(QColor a, style::color b, double b_ratio) {
	return (b_ratio < 1) ? pen(a, b->c, b_ratio) : b;
}

FORCE_INLINE QPen pen(style::color a, style::color b, double b_ratio) {
	return (b_ratio > 0) ? ((b_ratio < 1) ? pen(a->c, b->c, b_ratio) : b) : a;
}

FORCE_INLINE QBrush brush(QColor a, QColor b, double b_ratio) {
	return color(a, b, b_ratio);
}

FORCE_INLINE QBrush brush(style::color a, QColor b, double b_ratio) {
	return (b_ratio > 0) ? brush(a->c, b, b_ratio) : a;
}

FORCE_INLINE QBrush brush(QColor a, style::color b, double b_ratio) {
	return (b_ratio < 1) ? brush(a, b->c, b_ratio) : b;
}

FORCE_INLINE QBrush brush(style::color a, style::color b, double b_ratio) {
	return (b_ratio > 0) ? ((b_ratio < 1) ? brush(a->c, b->c, b_ratio) : b) : a;
}

template <int N> QPainterPath interpolate(QPointF (&from)[N], QPointF (&to)[N], double k) {
	static_assert(N > 1, "Wrong points count in path!");

	auto from_coef = 1. - k, to_coef = k;
	QPainterPath result;
	auto x = from[0].x() * from_coef + to[0].x() * to_coef;
	auto y = from[0].y() * from_coef + to[0].y() * to_coef;
	result.moveTo(x, y);
	for (int i = 1; i != N; ++i) {
		result.lineTo(from[i].x() * from_coef + to[i].x() * to_coef, from[i].y() * from_coef + to[i].y() * to_coef);
	}
	result.lineTo(x, y);
	return result;
}

template <int N> QPainterPath path(QPointF (&from)[N]) {
	static_assert(N > 1, "Wrong points count in path!");

	QPainterPath result;
	auto x = from[0].x();
	auto y = from[0].y();
	result.moveTo(x, y);
	for (int i = 1; i != N; ++i) {
		result.lineTo(from[i].x(), from[i].y());
	}
	result.lineTo(x, y);
	return result;
}

bool Disabled();
void SetDisabled(bool disabled);

} // namespace anim

class BasicAnimation;

class AnimationImplementation {
public:
	virtual void start() {}
	virtual void step(BasicAnimation *a, TimeMs ms, bool timer) = 0;
	virtual ~AnimationImplementation() {}
};

class AnimationCallbacks {
public:
	AnimationCallbacks(AnimationImplementation *implementation)
	    : _implementation(implementation) {}
	AnimationCallbacks(const AnimationCallbacks &other) = delete;
	AnimationCallbacks &operator=(const AnimationCallbacks &other) = delete;
	AnimationCallbacks(AnimationCallbacks &&other)
	    : _implementation(other._implementation) {
		other._implementation = nullptr;
	}
	AnimationCallbacks &operator=(AnimationCallbacks &&other) {
		std::swap(_implementation, other._implementation);
		return *this;
	}

	void start() {
		_implementation->start();
	}
	void step(BasicAnimation *a, TimeMs ms, bool timer) {
		_implementation->step(a, ms, timer);
	}
	~AnimationCallbacks() {
		delete base::take(_implementation);
	}

private:
	AnimationImplementation *_implementation;
};

class BasicAnimation {
public:
	BasicAnimation(AnimationCallbacks &&callbacks)
	    : _callbacks(std::move(callbacks))
	    , _animating(false) {}

	void start();
	void stop();

	void step(TimeMs ms, bool timer = false) {
		_callbacks.step(this, ms, timer);
	}

	void step() {
		step(getms(), false);
	}

	bool animating() const {
		return _animating;
	}

	~BasicAnimation() {
		if (_animating) stop();
	}

private:
	AnimationCallbacks _callbacks;
	bool _animating;
};

template <typename Type> class AnimationCallbacksRelative : public AnimationImplementation {
public:
	typedef void (Type::*Method)(double, bool);

	AnimationCallbacksRelative(Type *obj, Method method)
	    : _obj(obj)
	    , _method(method) {}

	void start() {
		_started = getms();
	}

	void step(BasicAnimation *a, TimeMs ms, bool timer) {
		(_obj->*_method)(qMax(ms - _started, TimeMs(0)), timer);
	}

private:
	TimeMs _started = 0;
	Type *_obj = nullptr;
	Method _method = nullptr;
};

template <typename Type>
AnimationCallbacks animation(Type *obj, typename AnimationCallbacksRelative<Type>::Method method) {
	return AnimationCallbacks(new AnimationCallbacksRelative<Type>(obj, method));
}

template <typename Type> class AnimationCallbacksAbsolute : public AnimationImplementation {
public:
	typedef void (Type::*Method)(TimeMs, bool);

	AnimationCallbacksAbsolute(Type *obj, Method method)
	    : _obj(obj)
	    , _method(method) {}

	void step(BasicAnimation *a, TimeMs ms, bool timer) {
		(_obj->*_method)(ms, timer);
	}

private:
	Type *_obj = nullptr;
	Method _method = nullptr;
};

template <typename Type>
AnimationCallbacks animation(Type *obj, typename AnimationCallbacksAbsolute<Type>::Method method) {
	return AnimationCallbacks(new AnimationCallbacksAbsolute<Type>(obj, method));
}

template <typename Type, typename Param> class AnimationCallbacksRelativeWithParam : public AnimationImplementation {
public:
	typedef void (Type::*Method)(Param, double, bool);

	AnimationCallbacksRelativeWithParam(Param param, Type *obj, Method method)
	    : _param(param)
	    , _obj(obj)
	    , _method(method) {}

	void start() {
		_started = getms();
	}

	void step(BasicAnimation *a, TimeMs ms, bool timer) {
		(_obj->*_method)(_param, qMax(ms - _started, TimeMs(0)), timer);
	}

private:
	TimeMs _started = 0;
	Param _param;
	Type *_obj = nullptr;
	Method _method = nullptr;
};

template <typename Type, typename Param>
AnimationCallbacks animation(Param param, Type *obj,
                             typename AnimationCallbacksRelativeWithParam<Type, Param>::Method method) {
	return AnimationCallbacks(new AnimationCallbacksRelativeWithParam<Type, Param>(param, obj, method));
}

template <typename Type, typename Param> class AnimationCallbacksAbsoluteWithParam : public AnimationImplementation {
public:
	typedef void (Type::*Method)(Param, TimeMs, bool);

	AnimationCallbacksAbsoluteWithParam(Param param, Type *obj, Method method)
	    : _param(param)
	    , _obj(obj)
	    , _method(method) {}

	void step(BasicAnimation *a, TimeMs ms, bool timer) {
		(_obj->*_method)(_param, ms, timer);
	}

private:
	Param _param;
	Type *_obj = nullptr;
	Method _method = nullptr;
};

template <typename Type, typename Param>
AnimationCallbacks animation(Param param, Type *obj,
                             typename AnimationCallbacksAbsoluteWithParam<Type, Param>::Method method) {
	return AnimationCallbacks(new AnimationCallbacksAbsoluteWithParam<Type, Param>(param, obj, method));
}

class Animation {
public:
	void step(TimeMs ms) {
		if (_data) {
			_data->a_animation.step(ms);
			if (_data && !_data->a_animation.animating()) {
				_data.reset();
			}
		}
	}

	bool animating() const {
		if (_data) {
			if (_data->a_animation.animating()) {
				return true;
			}
			_data.reset();
		}
		return false;
	}
	bool animating(TimeMs ms) {
		step(ms);
		return animating();
	}

	double current() const {
		Assert(_data != nullptr);
		return _data->value.current();
	}
	double current(double def) const {
		return animating() ? current() : def;
	}
	double current(TimeMs ms, double def) {
		return animating(ms) ? current() : def;
	}

	static constexpr auto kLongAnimationDuration = 1000;

	template <typename Lambda>
	void start(Lambda &&updateCallback, double from, double to, double duration,
	           anim::transition transition = anim::linear) {
		auto isLong = (duration >= kLongAnimationDuration);
		if (_data) {
			if (!isLong) {
				_data->pause.restart();
			}
		} else {
			_data = std::make_unique<Data>(from, std::forward<Lambda>(updateCallback));
		}
		if (isLong) {
			_data->pause.release();
		}
		_data->value.start(to);
		_data->duration = duration;
		_data->transition = transition;
		_data->a_animation.start();
	}

	void finish() {
		if (_data) {
			_data->value.finish();
			_data->a_animation.stop();
			_data.reset();
		}
	}

	template <typename Lambda> void setUpdateCallback(Lambda &&updateCallback) {
		if (_data) {
			_data->updateCallback = std::forward<Lambda>(updateCallback);
		}
	}

private:
	struct Data {
		template <typename Lambda>
		Data(double from, Lambda updateCallback)
		    : value(from, from)
		    , a_animation(animation(this, &Data::step))
		    , updateCallback(std::move(updateCallback)) {}
		void step(double ms, bool timer) {
			auto dt = (ms >= duration || anim::Disabled()) ? 1. : (ms / duration);
			if (dt >= 1) {
				value.finish();
				a_animation.stop();
				pause.release();
			} else {
				value.update(dt, transition);
			}
			updateCallback();
		}

		anim::value value;
		BasicAnimation a_animation;
		Fn<void()> updateCallback;
		double duration = 0.;
		anim::transition transition = anim::linear;
		MTP::PauseHolder pause;
	};
	mutable std::unique_ptr<Data> _data;
};

class AnimationManager : public QObject {
	Q_OBJECT

public:
	AnimationManager();

	void start(BasicAnimation *obj);
	void stop(BasicAnimation *obj);

public slots:
	void timeout();

	void clipCallback(Media::Clip::Reader *reader, qint32 threadIndex, qint32 notification);

private:
	using AnimatingObjects = std::set<BasicAnimation *>;
	AnimatingObjects _objects, _starting, _stopping;
	QTimer _timer;
	bool _iterating;
};
