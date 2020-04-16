/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

#include <vector>
#include <string.h>
#include <string>

namespace Ui {
namespace Emoji {

static_assert(sizeof(char16_t) == 2, "Bad UTF-16 character size.");

/// @brief Alias for std::u16string. It was the class in upstream code.
using utf16string = std::u16string;

namespace internal {

using checksum = unsigned int;
checksum countChecksum(const void *data, std::size_t size);

utf16string GetReplacementEmoji(utf16string replacement);

} // namespace internal

class Suggestion {
public:
	Suggestion() = default;
	Suggestion(utf16string emoji, utf16string label, utf16string replacement) : emoji_(emoji), label_(label), replacement_(replacement) {
	}
	Suggestion(const Suggestion &other) = default;
	Suggestion &operator=(const Suggestion &other) = default;

	utf16string emoji() const {
		return emoji_;
	}
	utf16string label() const {
		return label_;
	}
	utf16string replacement() const {
		return replacement_;
	}

private:
	utf16string emoji_;
	utf16string label_;
	utf16string replacement_;

};

std::vector<Suggestion> GetSuggestions(utf16string query);

inline utf16string GetSuggestionEmoji(utf16string replacement) {
	return internal::GetReplacementEmoji(replacement);
}

int GetSuggestionMaxLength();

} // namespace Emoji
} // namespace Ui
