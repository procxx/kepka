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

#include "base/variant.h"
#include <gsl/gsl_assert> // for Expects

namespace base {

struct none_type {
	bool operator==(none_type other) const {
		return true;
	}
	bool operator!=(none_type other) const {
		return false;
	}
	bool operator<(none_type other) const {
		return false;
	}
	bool operator<=(none_type other) const {
		return true;
	}
	bool operator>(none_type other) const {
		return false;
	}
	bool operator>=(none_type other) const {
		return true;
	}
};

constexpr none_type none = {};

template <typename... Types> class optional_variant {
public:
	optional_variant()
	    : _impl(none) {}
	optional_variant(const optional_variant &other)
	    : _impl(other._impl) {}
	optional_variant(optional_variant &&other)
	    : _impl(std::move(other._impl)) {}
	template <typename T, typename = std::enable_if_t<!std::is_base_of<optional_variant, std::decay_t<T>>::value>>
	optional_variant(T &&value)
	    : _impl(std::forward<T>(value)) {}
	optional_variant &operator=(const optional_variant &other) {
		_impl = other._impl;
		return *this;
	}
	optional_variant &operator=(optional_variant &&other) {
		_impl = std::move(other._impl);
		return *this;
	}
	template <typename T, typename = std::enable_if_t<!std::is_base_of<optional_variant, std::decay_t<T>>::value>>
	optional_variant &operator=(T &&value) {
		_impl = std::forward<T>(value);
		return *this;
	}

	explicit operator bool() const {
		return (get_if<none_type>(&_impl) == nullptr);
	}
	bool operator==(const optional_variant &other) const {
		return _impl == other._impl;
	}
	bool operator!=(const optional_variant &other) const {
		return _impl != other._impl;
	}
	bool operator<(const optional_variant &other) const {
		return _impl < other._impl;
	}
	bool operator<=(const optional_variant &other) const {
		return _impl <= other._impl;
	}
	bool operator>(const optional_variant &other) const {
		return _impl > other._impl;
	}
	bool operator>=(const optional_variant &other) const {
		return _impl >= other._impl;
	}

	template <typename T> decltype(auto) is() const {
		return _impl.template is<T>();
	}
	template <typename T> decltype(auto) get_unchecked() {
		return _impl.template get_unchecked<T>();
	}
	template <typename T> decltype(auto) get_unchecked() const {
		return _impl.template get_unchecked<T>();
	}

private:
	variant<none_type, Types...> _impl;
};

template <typename T, typename... Types> inline T *get_if(optional_variant<Types...> *v) {
	return (v && v->template is<T>()) ? &v->template get_unchecked<T>() : nullptr;
}

template <typename T, typename... Types> inline const T *get_if(const optional_variant<Types...> *v) {
	return (v && v->template is<T>()) ? &v->template get_unchecked<T>() : nullptr;
}

template <typename Type> class optional;

template <typename Type> struct optional_wrap_once { using type = optional<Type>; };

template <typename Type> struct optional_wrap_once<optional<Type>> { using type = optional<Type>; };

template <typename Type> using optional_wrap_once_t = typename optional_wrap_once<std::decay_t<Type>>::type;

template <typename Type> struct optional_chain_result { using type = optional_wrap_once_t<Type>; };

template <> struct optional_chain_result<void> { using type = bool; };

template <typename Type> using optional_chain_result_t = typename optional_chain_result<Type>::type;

template <typename Type> class optional : public optional_variant<Type> {
public:
	using optional_variant<Type>::optional_variant;

	Type &operator*() {
		auto result = get_if<Type>(this);
		Expects(result != nullptr);
		return *result;
	}
	const Type &operator*() const {
		auto result = get_if<Type>(this);
		Expects(result != nullptr);
		return *result;
	}
	Type *operator->() {
		auto result = get_if<Type>(this);
		Expects(result != nullptr);
		return result;
	}
	const Type *operator->() const {
		auto result = get_if<Type>(this);
		Expects(result != nullptr);
		return result;
	}
};

template <typename Type> optional_wrap_once_t<Type> make_optional(Type &&value) {
	return optional_wrap_once_t<Type>{std::forward<Type>(value)};
}

template <typename Type, typename Method>
inline auto optional_chain(const optional<Type> &value, Method &method, std::false_type)
    -> optional_chain_result_t<decltype(method(*value))> {
	return value ? make_optional(method(*value)) : none;
}

template <typename Type, typename Method>
inline auto optional_chain(const optional<Type> &value, Method &method, std::true_type)
    -> optional_chain_result_t<decltype(method(*value))> {
	return value ? (method(*value), true) : false;
}

template <typename Type, typename Method>
inline auto operator|(const optional<Type> &value, Method method) -> optional_chain_result_t<decltype(method(*value))> {
	using is_void_return = std::is_same<decltype(method(*value)), void>;
	return optional_chain(value, method, is_void_return{});
}

} // namespace base
