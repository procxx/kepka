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

#include "private/qfontengine_p.h"

enum TextBlockType {
	TextBlockTNewline = 0x01,
	TextBlockTText = 0x02,
	TextBlockTEmoji = 0x03,
	TextBlockTSkip = 0x04,
};

enum TextBlockFlags {
	TextBlockFBold = 0x01,
	TextBlockFItalic = 0x02,
	TextBlockFUnderline = 0x04,
	TextBlockFTilde = 0x08, // tilde fix in OpenSans
	TextBlockFSemibold = 0x10,
	TextBlockFCode = 0x20,
	TextBlockFPre = 0x40,
};

class ITextBlock {
public:
	ITextBlock(const style::font &font, const QString &str, quint16 from, quint16 length, uchar flags, quint16 lnkIndex) : _from(from), _flags((flags & 0xFF) | ((lnkIndex & 0xFFFF) << 12)) {
	}

	quint16 from() const {
		return _from;
	}
	qint32 width() const {
		return _width.toInt();
	}
	qint32 rpadding() const {
		return _rpadding.toInt();
	}
	QFixed f_width() const {
		return _width;
	}
	QFixed f_rpadding() const {
		return _rpadding;
	}

	// Should be virtual, but optimized throught type() call.
	QFixed f_rbearing() const;

	quint16 lnkIndex() const {
		return (_flags >> 12) & 0xFFFF;
	}
	void setLnkIndex(quint16 lnkIndex) {
		_flags = (_flags & ~(0xFFFF << 12)) | (lnkIndex << 12);
	}

	TextBlockType type() const {
		return TextBlockType((_flags >> 8) & 0x0F);
	}
	qint32 flags() const {
		return (_flags & 0xFF);
	}

	virtual std::unique_ptr<ITextBlock> clone() const = 0;
	virtual ~ITextBlock() {
	}

protected:
	quint16 _from = 0;

	quint32 _flags = 0; // 4 bits empty, 16 bits lnkIndex, 4 bits type, 8 bits flags

	QFixed _width = 0;

	// Right padding: spaces after the last content of the block (like a word).
	// This holds spaces after the end of the block, for example a text ending
	// with a space before a link has started. If text block has a leading spaces
	// (for example a text block after a link block) it is prepended with an empty
	// word that holds those spaces as a right padding.
	QFixed _rpadding = 0;

};

class NewlineBlock : public ITextBlock {
public:
	NewlineBlock(const style::font &font, const QString &str, quint16 from, quint16 length, uchar flags, quint16 lnkIndex) : ITextBlock(font, str, from, length, flags, lnkIndex), _nextDir(Qt::LayoutDirectionAuto) {
		_flags |= ((TextBlockTNewline & 0x0F) << 8);
	}

	Qt::LayoutDirection nextDirection() const {
		return _nextDir;
	}

	std::unique_ptr<ITextBlock> clone() const override {
		return std::make_unique<NewlineBlock>(*this);
	}

private:
	Qt::LayoutDirection _nextDir;

	friend class Text;
	friend class TextParser;

	friend class TextPainter;

};

class TextWord {
public:
	TextWord() = default;
	TextWord(quint16 from, QFixed width, QFixed rbearing, QFixed rpadding = 0)
		: _from(from)
		, _width(width)
		, _rpadding(rpadding)
		, _rbearing(rbearing.value() > 0x7FFF ? 0x7FFF : (rbearing.value() < -0x7FFF ? -0x7FFF : rbearing.value())) {
	}
	quint16 from() const {
		return _from;
	}
	QFixed f_rbearing() const {
		return QFixed::fromFixed(_rbearing);
	}
	QFixed f_width() const {
		return _width;
	}
	QFixed f_rpadding() const {
		return _rpadding;
	}
	void add_rpadding(QFixed padding) {
		_rpadding += padding;
	}

private:
	quint16 _from = 0;
	QFixed _width, _rpadding;
	qint16 _rbearing = 0;

};

class TextBlock : public ITextBlock {
public:
	TextBlock(const style::font &font, const QString &str, QFixed minResizeWidth, quint16 from, quint16 length, uchar flags, quint16 lnkIndex);

	std::unique_ptr<ITextBlock> clone() const override {
		return std::make_unique<TextBlock>(*this);
	}

private:
	friend class ITextBlock;
	QFixed real_f_rbearing() const {
		return _words.isEmpty() ? 0 : _words.back().f_rbearing();
	}

	typedef QVector<TextWord> TextWords;
	TextWords _words;

	friend class Text;
	friend class TextParser;

	friend class BlockParser;
	friend class TextPainter;

};

class EmojiBlock : public ITextBlock {
public:
	EmojiBlock(const style::font &font, const QString &str, quint16 from, quint16 length, uchar flags, quint16 lnkIndex, EmojiPtr emoji);

	std::unique_ptr<ITextBlock> clone() const {
		return std::make_unique<EmojiBlock>(*this);
	}

private:
	EmojiPtr emoji = nullptr;

	friend class Text;
	friend class TextParser;

	friend class TextPainter;

};

class SkipBlock : public ITextBlock {
public:
	SkipBlock(const style::font &font, const QString &str, quint16 from, qint32 w, qint32 h, quint16 lnkIndex);

	qint32 height() const {
		return _height;
	}

	std::unique_ptr<ITextBlock> clone() const override {
		return std::make_unique<SkipBlock>(*this);
	}

private:
	qint32 _height;

	friend class Text;
	friend class TextParser;

	friend class TextPainter;

};
