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

#include "base/runtime_composer.h"

constexpr auto FullSelection = TextSelection { 0xFFFF, 0xFFFF };

extern TextParseOptions _textNameOptions, _textDlgOptions;
extern TextParseOptions _historyTextOptions, _historyBotOptions, _historyTextNoMonoOptions, _historyBotNoMonoOptions;

const TextParseOptions &itemTextOptions(History *h, PeerData *f);
const TextParseOptions &itemTextOptions(const HistoryItem *item);
const TextParseOptions &itemTextNoMonoOptions(History *h, PeerData *f);
const TextParseOptions &itemTextNoMonoOptions(const HistoryItem *item);

enum RoundCorners {
	SmallMaskCorners = 0x00, // for images
	LargeMaskCorners,

	BoxCorners,
	MenuCorners,
	BotKbOverCorners,
	StickerCorners,
	StickerSelectedCorners,
	SelectedOverlaySmallCorners,
	SelectedOverlayLargeCorners,
	DateCorners,
	DateSelectedCorners,
	ForwardCorners,
	MediaviewSaveCorners,
	EmojiHoverCorners,
	StickerHoverCorners,
	BotKeyboardCorners,
	PhotoSelectOverlayCorners,

	Doc1Corners,
	Doc2Corners,
	Doc3Corners,
	Doc4Corners,

	InShadowCorners, // for photos without bg
	InSelectedShadowCorners,

	MessageInCorners, // with shadow
	MessageInSelectedCorners,
	MessageOutCorners,
	MessageOutSelectedCorners,

	RoundCornersCount
};

static const int32_t FileStatusSizeReady = 0x7FFFFFF0;
static const int32_t FileStatusSizeLoaded = 0x7FFFFFF1;
static const int32_t FileStatusSizeFailed = 0x7FFFFFF2;

QString formatSizeText(int64_t size);
QString formatDownloadText(int64_t ready, int64_t total);
QString formatDurationText(int64_t duration);
QString formatDurationWords(int64_t duration);
QString formatDurationAndSizeText(int64_t duration, int64_t size);
QString formatGifAndSizeText(int64_t size);
QString formatPlayedText(int64_t played, int64_t duration);

int32_t documentColorIndex(DocumentData *document, QString &ext);
style::color documentColor(int colorIndex);
style::color documentDarkColor(int colorIndex);
style::color documentOverColor(int colorIndex);
style::color documentSelectedColor(int colorIndex);
RoundCorners documentCorners(int colorIndex);
bool documentIsValidMediaFile(const QString &filepath);
bool documentIsExecutableName(const QString &filename);

class PaintContextBase {
public:
	PaintContextBase(TimeMs ms, bool selecting) : ms(ms), selecting(selecting) {
	}
	TimeMs ms;
	bool selecting;

};

class LayoutItemBase : public RuntimeComposer, public ClickHandlerHost {
public:
	LayoutItemBase() {
	}

	LayoutItemBase(const LayoutItemBase &other) = delete;
	LayoutItemBase &operator=(const LayoutItemBase &other) = delete;

	int maxWidth() const {
		return _maxw;
	}
	int minHeight() const {
		return _minh;
	}
	virtual void initDimensions() = 0;
	virtual int resizeGetHeight(int width) {
		_width = qMin(width, _maxw);
		_height = _minh;
		return _height;
	}

	virtual void getState(ClickHandlerPtr &link, HistoryCursorState &cursor, QPoint point) const {
		link.clear();
		cursor = HistoryDefaultCursorState;
	}
	virtual void getSymbol(uint16_t &symbol, bool &after, bool &upon, QPoint point) const { // from text
		upon = hasPoint(point);
		symbol = upon ? 0xFFFF : 0;
		after = false;
	}

	int width() const {
		return _width;
	}
	int height() const {
		return _height;
	}

	bool hasPoint(QPoint point) const {
		return QRect(0, 0, width(), height()).contains(point);
	}

	virtual ~LayoutItemBase() {
	}

protected:
	int _width = 0;
	int _height = 0;
	int _maxw = 0;
	int _minh = 0;

};
