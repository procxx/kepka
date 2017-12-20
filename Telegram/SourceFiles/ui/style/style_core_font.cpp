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
#include "ui/style/style_core_font.h"

namespace style {
namespace internal {
namespace {

typedef QMap<QString, int> FontFamilyMap;
FontFamilyMap fontFamilyMap;

typedef QVector<QString> FontFamilies;
FontFamilies fontFamilies;

typedef QMap<quint32, FontData*> FontDatas;
FontDatas fontsMap;

quint32 fontKey(int size, quint32 flags, int family) {
	return (((quint32(family) << 10) | quint32(size)) << 3) | flags;
}

} // namespace

void destroyFonts() {
	for (auto fontData : fontsMap) {
		delete fontData;
	}
	fontsMap.clear();
}

int registerFontFamily(const QString &family) {
	auto result = fontFamilyMap.value(family, -1);
	if (result < 0) {
		result = fontFamilies.size();
		fontFamilyMap.insert(family, result);
		fontFamilies.push_back(family);
	}
	return result;
}

FontData::FontData(int size, quint32 flags, int family, Font *other)
: f(Fonts::GetOverride(fontFamilies[family]))
, m(f)
, _size(size)
, _flags(flags)
, _family(family) {
	if (other) {
		memcpy(modified, other, sizeof(modified));
	} else {
		memset(modified, 0, sizeof(modified));
	}
	modified[_flags] = Font(this);

	f.setPixelSize(size);
	f.setBold(_flags & FontBold);
	f.setItalic(_flags & FontItalic);
	f.setUnderline(_flags & FontUnderline);
	f.setStyleStrategy(QFont::PreferQuality);

	m = QFontMetrics(f);
	height = m.height();
	ascent = m.ascent();
	descent = m.descent();
	spacew = width(QLatin1Char(' '));
	elidew = width(qsl("..."));
}

Font FontData::bold(bool set) const {
	return otherFlagsFont(FontBold, set);
}

Font FontData::italic(bool set) const {
	return otherFlagsFont(FontItalic, set);
}

Font FontData::underline(bool set) const {
	return otherFlagsFont(FontUnderline, set);
}

int FontData::size() const {
	return _size;
}

quint32 FontData::flags() const {
	return _flags;
}

int FontData::family() const {
	return _family;
}

Font FontData::otherFlagsFont(quint32 flag, bool set) const {
	qint32 newFlags = set ? (_flags | flag) : (_flags & ~flag);
	if (!modified[newFlags].v()) {
		modified[newFlags] = Font(_size, newFlags, _family, modified);
	}
	return modified[newFlags];
}

Font::Font(int size, quint32 flags, const QString &family) {
	if (fontFamilyMap.isEmpty()) {
		for (quint32 i = 0, s = fontFamilies.size(); i != s; ++i) {
			fontFamilyMap.insert(fontFamilies.at(i), i);
		}
	}

	auto i = fontFamilyMap.constFind(family);
	if (i == fontFamilyMap.cend()) {
		fontFamilies.push_back(family);
		i = fontFamilyMap.insert(family, fontFamilies.size() - 1);
	}
	init(size, flags, i.value(), 0);
}

Font::Font(int size, quint32 flags, int family) {
	init(size, flags, family, 0);
}

Font::Font(int size, quint32 flags, int family, Font *modified) {
	init(size, flags, family, modified);
}

void Font::init(int size, quint32 flags, int family, Font *modified) {
	quint32 key = fontKey(size, flags, family);
	auto i = fontsMap.constFind(key);
	if (i == fontsMap.cend()) {
		i = fontsMap.insert(key, new FontData(size, flags, family, modified));
	}
	ptr = i.value();
}

} // namespace internal
} // namespace style
