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

#include <type_traits>

namespace base {

template <typename T> struct custom_is_fast_copy_type : public std::false_type {};
// To make your own type a fast copy type just write:
// template <> struct base::custom_is_fast_copy_type<MyTinyType> : public std::true_type {};

namespace internal {

template <typename T>
using is_fast_copy_type =
    std::integral_constant<bool, std::is_fundamental_v<T> || std::is_pointer_v<T> || std::is_member_pointer_v<T> ||
                                     custom_is_fast_copy_type<T>::value>;

template <typename T> struct add_const_reference { using type = const T &; };
template <> struct add_const_reference<void> { using type = void; };
template <typename T> using add_const_reference_t = typename add_const_reference<T>::type;

} // namespace internal

template <typename T> struct type_traits {
	using is_fast_copy_type = internal::is_fast_copy_type<T>;

	using parameter_type = std::conditional_t<is_fast_copy_type::value, T, internal::add_const_reference_t<T>>;
};

template <typename T> using parameter_type = typename type_traits<T>::parameter_type;

} // namespace base
