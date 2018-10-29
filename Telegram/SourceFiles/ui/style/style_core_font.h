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

#include <QFont>
#include <QFontMetrics>
#include <QString>

namespace style {
namespace internal {

void destroyFonts();
int registerFontFamily(const QString &family);

class FontData;
class Font {
public:
	Font(Qt::Initialization = Qt::Uninitialized)
	    : ptr(0) {}
	Font(int size, quint32 flags, const QString &family);
	Font(int size, quint32 flags, int family);

	Font &operator=(const Font &other) {
		ptr = other.ptr;
		return (*this);
	}

	FontData *operator->() const {
		return ptr;
	}
	FontData *v() const {
		return ptr;
	}

	operator bool() const {
		return !!ptr;
	}

	operator const QFont &() const;

private:
	FontData *ptr;

	void init(int size, quint32 flags, int family, Font *modified);
	friend void startManager();

	Font(FontData *p)
	    : ptr(p) {}
	Font(int size, quint32 flags, int family, Font *modified);
	friend class FontData;
};

enum FontFlags {
	FontBold = 0x01,
	FontItalic = 0x02,
	FontUnderline = 0x04,

	FontDifferentFlags = 0x08,
};

class FontData {
public:
	qint32 width(const QString &str) const {
		return m.width(str);
	}
	qint32 width(const QString &str, qint32 from, qint32 to) const {
		return width(str.mid(from, to));
	}
	qint32 width(QChar ch) const {
		return m.width(ch);
	}
	QString elided(const QString &str, qint32 width, Qt::TextElideMode mode = Qt::ElideRight) const {
		return m.elidedText(str, mode, width);
	}

	Font bold(bool set = true) const;
	Font italic(bool set = true) const;
	Font underline(bool set = true) const;

	int size() const;
	quint32 flags() const;
	int family() const;

	QFont f;
	QFontMetrics m;
	qint32 height, ascent, descent, spacew, elidew;

private:
	mutable Font modified[FontDifferentFlags];

	Font otherFlagsFont(quint32 flag, bool set) const;
	FontData(int size, quint32 flags, int family, Font *other);

	friend class Font;
	int _size;
	quint32 _flags;
	int _family;
};

inline bool operator==(const Font &a, const Font &b) {
	return a.v() == b.v();
}
inline bool operator!=(const Font &a, const Font &b) {
	return a.v() != b.v();
}

inline Font::operator const QFont &() const {
	return ptr->f;
}

} // namespace internal
} // namespace style
