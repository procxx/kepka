/* Extracted from https://github.com/facebook/folly/blob/master/folly/functional/Invoke.h */

/*
 * Copyright 2017-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <functional>
#include <type_traits>

#if __cpp_lib_is_invocable >= 201703 || (_MSC_VER >= 1911 && _MSVC_LANG > 201402)

namespace backports {

/* using override */ using std::invoke_result;
/* using override */ using std::invoke_result_t;
/* using override */ using std::is_invocable;
/* using override */ using std::is_invocable_r;
/* using override */ using std::is_invocable_r_v;
/* using override */ using std::is_invocable_v;
/* using override */ using std::is_nothrow_invocable;
/* using override */ using std::is_nothrow_invocable_r;
/* using override */ using std::is_nothrow_invocable_r_v;
/* using override */ using std::is_nothrow_invocable_v;

} // namespace backports

#else

namespace backports {

namespace invoke_detail {

template <bool... Bs> struct Bools {
	using valid_type = bool;
	static constexpr std::size_t size() {
		return sizeof...(Bs);
	}
};

template <typename T> struct Negation : std::bool_constant<!T::value> {};

// Lighter-weight than Conjunction, but evaluates all sub-conditions eagerly.
template <class... Ts> struct StrictConjunction : std::is_same<Bools<Ts::value...>, Bools<(Ts::value || true)...>> {};

template <class... Ts>
struct StrictDisjunction : Negation<std::is_same<Bools<Ts::value...>, Bools<(Ts::value && false)...>>> {};

template <typename F, typename... Args>
using invoke_result_ = decltype(std::invoke(std::declval<F>(), std::declval<Args>()...));

template <typename F, typename... Args>
struct invoke_nothrow_ : std::bool_constant<noexcept(std::invoke(std::declval<F>(), std::declval<Args>()...))> {};

//  from: http://en.cppreference.com/w/cpp/types/result_of, CC-BY-SA

template <typename Void, typename F, typename... Args> struct invoke_result {};

template <typename F, typename... Args> struct invoke_result<std::void_t<invoke_result_<F, Args...>>, F, Args...> {
	using type = invoke_result_<F, Args...>;
};

template <typename Void, typename F, typename... Args> struct is_invocable : std::false_type {};

template <typename F, typename... Args>
struct is_invocable<std::void_t<invoke_result_<F, Args...>>, F, Args...> : std::true_type {};

template <typename Void, typename R, typename F, typename... Args> struct is_invocable_r : std::false_type {};

template <typename R, typename F, typename... Args>
struct is_invocable_r<std::void_t<invoke_result_<F, Args...>>, R, F, Args...>
    : std::is_convertible<invoke_result_<F, Args...>, R> {};

template <typename Void, typename F, typename... Args> struct is_nothrow_invocable : std::false_type {};

template <typename F, typename... Args>
struct is_nothrow_invocable<std::void_t<invoke_result_<F, Args...>>, F, Args...> : invoke_nothrow_<F, Args...> {};

template <typename Void, typename R, typename F, typename... Args> struct is_nothrow_invocable_r : std::false_type {};

template <typename R, typename F, typename... Args>
struct is_nothrow_invocable_r<std::void_t<invoke_result_<F, Args...>>, R, F, Args...>
    : StrictConjunction<std::is_convertible<invoke_result_<F, Args...>, R>, invoke_nothrow_<F, Args...>> {};

} // namespace invoke_detail

//  mimic: std::invoke_result, C++17
template <typename F, typename... Args> struct invoke_result : invoke_detail::invoke_result<void, F, Args...> {};

//  mimic: std::invoke_result_t, C++17
template <typename F, typename... Args> using invoke_result_t = typename invoke_result<F, Args...>::type;

//  mimic: std::is_invocable, C++17
template <typename F, typename... Args> struct is_invocable : invoke_detail::is_invocable<void, F, Args...> {};

//  mimic: std::is_invocable_r, C++17
template <typename R, typename F, typename... Args>
struct is_invocable_r : invoke_detail::is_invocable_r<void, R, F, Args...> {};

//  mimic: std::is_nothrow_invocable, C++17
template <typename F, typename... Args>
struct is_nothrow_invocable : invoke_detail::is_nothrow_invocable<void, F, Args...> {};

//  mimic: std::is_nothrow_invocable_r, C++17
template <typename R, typename F, typename... Args>
struct is_nothrow_invocable_r : invoke_detail::is_nothrow_invocable_r<void, R, F, Args...> {};

template <class Fn, class... ArgTypes> inline constexpr bool is_invocable_v = is_invocable<Fn, ArgTypes...>::value;

template <class R, class Fn, class... ArgTypes>
inline constexpr bool is_invocable_r_v = is_invocable_r<R, Fn, ArgTypes...>::value;

template <class Fn, class... ArgTypes>
inline constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<Fn, ArgTypes...>::value;

template <class R, class Fn, class... ArgTypes>
inline constexpr bool is_nothrow_invocable_r_v = is_nothrow_invocable_r<R, Fn, ArgTypes...>::value;

} // namespace backports

#endif
