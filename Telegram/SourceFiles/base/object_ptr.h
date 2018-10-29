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

#include "core/utils.h" // @todo used for base::take
#include <QObject>
#include <QPointer>

// Smart pointer for QObject*, has move semantics, destroys object if it doesn't have a parent.
template <typename Object> class object_ptr {
public:
	object_ptr(std::nullptr_t) {}

	// No default constructor, but constructors with at least
	// one argument are simply make functions.
	template <typename Parent, typename... Args>
	explicit object_ptr(Parent &&parent, Args &&... args)
	    : _object(new Object(std::forward<Parent>(parent), std::forward<Args>(args)...)) {}

	object_ptr(const object_ptr &other) = delete;
	object_ptr &operator=(const object_ptr &other) = delete;
	object_ptr(object_ptr &&other)
	    : _object(base::take(other._object)) {}
	object_ptr &operator=(object_ptr &&other) {
		auto temp = std::move(other);
		destroy();
		std::swap(_object, temp._object);
		return *this;
	}

	template <typename OtherObject, typename = std::enable_if_t<std::is_base_of<Object, OtherObject>::value>>
	object_ptr(object_ptr<OtherObject> &&other)
	    : _object(base::take(other._object)) {}

	template <typename OtherObject, typename = std::enable_if_t<std::is_base_of<Object, OtherObject>::value>>
	object_ptr &operator=(object_ptr<OtherObject> &&other) {
		_object = base::take(other._object);
		return *this;
	}

	object_ptr &operator=(std::nullptr_t) {
		_object = nullptr;
		return *this;
	}

	// So we can pass this pointer to methods like connect().
	Object *data() const {
		return static_cast<Object *>(_object.data());
	}
	operator Object *() const {
		return data();
	}

	explicit operator bool() const {
		return _object != nullptr;
	}

	Object *operator->() const {
		return data();
	}
	Object &operator*() const {
		return *data();
	}

	// Use that instead "= new Object(parent, ...)"
	template <typename Parent, typename... Args> void create(Parent &&parent, Args &&... args) {
		destroy();
		_object = new Object(std::forward<Parent>(parent), std::forward<Args>(args)...);
	}
	void destroy() {
		delete base::take(_object);
	}
	void destroyDelayed() {
		if (_object) {
			if (auto widget = base::up_cast<QWidget *>(data())) {
				widget->hide();
			}
			base::take(_object)->deleteLater();
		}
	}

	~object_ptr() {
		if (auto pointer = _object) {
			if (!pointer->parent()) {
				destroy();
			}
		}
	}

	template <typename ResultType, typename SourceType>
	friend object_ptr<ResultType> static_object_cast(object_ptr<SourceType> source);

private:
	template <typename OtherObject> friend class object_ptr;

	QPointer<QObject> _object;
};

template <typename ResultType, typename SourceType>
inline object_ptr<ResultType> static_object_cast(object_ptr<SourceType> source) {
	auto result = object_ptr<ResultType>(nullptr);
	result._object = static_cast<ResultType *>(base::take(source._object).data());
	return std::move(result);
}
