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

#include "layout.h"
#include "core/click_handler_types.h"
#include "ui/effects/radial_animation.h"
#include "styles/style_overview.h"

namespace Overview {
namespace Layout {

class PaintContext : public PaintContextBase {
public:
	PaintContext(TimeMs ms, bool selecting) : PaintContextBase(ms, selecting), isAfterDate(false) {
	}
	bool isAfterDate;

};

class ItemBase;
class AbstractItem : public LayoutItemBase {
public:
	virtual void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) = 0;

	virtual ItemBase *toMediaItem() {
		return nullptr;
	}
	virtual const ItemBase *toMediaItem() const {
		return nullptr;
	}

	virtual HistoryItem *getItem() const {
		return nullptr;
	}
	virtual DocumentData *getDocument() const {
		return nullptr;
	}
	MsgId msgId() const {
		auto item = getItem();
		return item ? item->id : 0;
	}

	virtual void invalidateCache() {
	}

};

class ItemBase : public AbstractItem {
public:
	ItemBase(HistoryItem *parent) : _parent(parent) {
	}

	ItemBase *toMediaItem() override {
		return this;
	}
	const ItemBase *toMediaItem() const override {
		return this;
	}
	HistoryItem *getItem() const override {
		return _parent;
	}

	void clickHandlerActiveChanged(const ClickHandlerPtr &action, bool active) override;
	void clickHandlerPressedChanged(const ClickHandlerPtr &action, bool pressed) override;

protected:
	HistoryItem *_parent;

};

class RadialProgressItem : public ItemBase {
public:
	RadialProgressItem(HistoryItem *parent) : ItemBase(parent) {
	}
	RadialProgressItem(const RadialProgressItem &other) = delete;

	void clickHandlerActiveChanged(const ClickHandlerPtr &action, bool active) override;

	~RadialProgressItem();

protected:
	ClickHandlerPtr _openl, _savel, _cancell;
	void setLinks(ClickHandlerPtr &&openl, ClickHandlerPtr &&savel, ClickHandlerPtr &&cancell);
	void setDocumentLinks(DocumentData *document) {
		ClickHandlerPtr save;
		if (document->voice()) {
			save.reset(new DocumentOpenClickHandler(document));
		} else {
			save.reset(new DocumentSaveClickHandler(document));
		}
		setLinks(MakeShared<DocumentOpenClickHandler>(document), std::move(save), MakeShared<DocumentCancelClickHandler>(document));
	}

	void step_radial(TimeMs ms, bool timer);

	void ensureRadial();
	void checkRadialFinished();

	bool isRadialAnimation(TimeMs ms) const {
		if (!_radial || !_radial->animating()) return false;

		_radial->step(ms);
		return _radial && _radial->animating();
	}

	virtual double dataProgress() const = 0;
	virtual bool dataFinished() const = 0;
	virtual bool dataLoaded() const = 0;
	virtual bool iconAnimated() const {
		return false;
	}

	std::unique_ptr<Ui::RadialAnimation> _radial;
	Animation _a_iconOver;

};

class StatusText {
public:
	// duration = -1 - no duration, duration = -2 - "GIF" duration
	void update(int newSize, int fullSize, int duration, TimeMs realDuration);
	void setSize(int newSize);

	int size() const {
		return _size;
	}
	QString text() const {
		return _text;
	}

private:
	// >= 0 will contain download / upload string, _size = loaded bytes
	// < 0 will contain played string, _size = -(seconds + 1) played
	// 0x7FFFFFF0 will contain status for not yet downloaded file
	// 0x7FFFFFF1 will contain status for already downloaded file
	// 0x7FFFFFF2 will contain status for failed to download / upload file
	int _size = 0;
	QString _text;

};

struct Info : public RuntimeComponent<Info> {
	int top = 0;
};

class Date : public AbstractItem {
public:
	Date(const QDate &date, bool month);

	void initDimensions() override;
	void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) override;

private:
	QDate _date;
	QString _text;

};

class PhotoVideoCheckbox;

class Photo : public ItemBase {
public:
	Photo(PhotoData *photo, HistoryItem *parent);

	void initDimensions() override;
	qint32 resizeGetHeight(qint32 width) override;
	void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) override;
	void getState(ClickHandlerPtr &link, HistoryCursorState &cursor, QPoint point) const override;

	void clickHandlerActiveChanged(const ClickHandlerPtr &action, bool active) override;
	void clickHandlerPressedChanged(const ClickHandlerPtr &action, bool pressed) override;

	void invalidateCache() override;

private:
	void ensureCheckboxCreated();

	std::unique_ptr<PhotoVideoCheckbox> _check;

	PhotoData *_data;
	ClickHandlerPtr _link;

	QPixmap _pix;
	bool _goodLoaded = false;

};

class Video : public RadialProgressItem {
public:
	Video(DocumentData *video, HistoryItem *parent);

	void initDimensions() override;
	qint32 resizeGetHeight(qint32 width) override;
	void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) override;
	void getState(ClickHandlerPtr &link, HistoryCursorState &cursor, QPoint point) const override;

	void clickHandlerActiveChanged(const ClickHandlerPtr &action, bool active) override;
	void clickHandlerPressedChanged(const ClickHandlerPtr &action, bool pressed) override;

	void invalidateCache() override;

protected:
	double dataProgress() const override {
		return _data->progress();
	}
	bool dataFinished() const override {
		return !_data->loading();
	}
	bool dataLoaded() const override {
		return _data->loaded();
	}
	bool iconAnimated() const override {
		return true;
	}

private:
	void ensureCheckboxCreated();

	std::unique_ptr<PhotoVideoCheckbox> _check;

	DocumentData *_data;
	StatusText _status;

	QString _duration;
	QPixmap _pix;
	bool _thumbLoaded = false;

	void updateStatusText();

};

class Voice : public RadialProgressItem {
public:
	Voice(DocumentData *voice, HistoryItem *parent, const style::OverviewFileLayout &st);

	void initDimensions() override;
	void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) override;
	void getState(ClickHandlerPtr &link, HistoryCursorState &cursor, QPoint point) const override;

protected:
	double dataProgress() const override {
		return _data->progress();
	}
	bool dataFinished() const override {
		return !_data->loading();
	}
	bool dataLoaded() const override {
		return _data->loaded();
	}
	bool iconAnimated() const override {
		return true;
	}

private:
	DocumentData *_data;
	StatusText _status;
	ClickHandlerPtr _namel;

	const style::OverviewFileLayout &_st;

	Text _name, _details;
	int _nameVersion;

	void updateName();
	bool updateStatusText();

};

class Document : public RadialProgressItem {
public:
	Document(DocumentData *document, HistoryItem *parent, const style::OverviewFileLayout &st);

	void initDimensions() override;
	void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) override;
	void getState(ClickHandlerPtr &link, HistoryCursorState &cursor, QPoint point) const override;

	virtual DocumentData *getDocument() const override {
		return _data;
	}

protected:
	double dataProgress() const override {
		return _data->progress();
	}
	bool dataFinished() const override {
		return !_data->loading();
	}
	bool dataLoaded() const override {
		return _data->loaded();
	}
	bool iconAnimated() const override {
		return _data->song() || !_data->loaded() || (_radial && _radial->animating());
	}

private:
	DocumentData *_data;
	StatusText _status;
	ClickHandlerPtr _msgl, _namel;

	const style::OverviewFileLayout &_st;

	bool _thumbForLoaded = false;
	QPixmap _thumb;

	Text _name;
	QString _date, _ext;
	qint32 _datew, _extw;
	qint32 _thumbw, _colorIndex;

	bool withThumb() const {
		return !_data->song() && !_data->thumb->isNull() && _data->thumb->width() && _data->thumb->height() && !documentIsExecutableName(_data->name);
	}
	bool updateStatusText();

};

class Link : public ItemBase {
public:
	Link(HistoryMedia *media, HistoryItem *parent);

	void initDimensions() override;
	qint32 resizeGetHeight(qint32 width) override;
	void paint(Painter &p, const QRect &clip, TextSelection selection, const PaintContext *context) override;
	void getState(ClickHandlerPtr &link, HistoryCursorState &cursor, QPoint point) const override;

private:
	ClickHandlerPtr _photol;

	QString _title, _letter;
	int _titlew = 0;
	WebPageData *_page = nullptr;
	int _pixw = 0;
	int _pixh = 0;
	Text _text = { int(st::msgMinWidth) };

	struct LinkEntry {
		LinkEntry() : width(0) {
		}
		LinkEntry(const QString &url, const QString &text);
		QString text;
		qint32 width;
		TextClickHandlerPtr lnk;
	};
	QVector<LinkEntry> _links;

};

} // namespace Layout
} // namespace Overview
