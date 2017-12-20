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

#include "base/flags.h"

class FileLoader;
class mtpFileLoader;

enum LoadFromCloudSetting {
	LoadFromCloudOrLocal,
	LoadFromLocalOnly,
};

enum LoadToCacheSetting {
	LoadToFileOnly,
	LoadToCacheAsWell,
};

enum class ImageRoundRadius {
	None,
	Large,
	Small,
	Ellipse,
};
enum class ImageRoundCorner {
	None        = 0x00,
	TopLeft     = 0x01,
	TopRight    = 0x02,
	BottomLeft  = 0x04,
	BottomRight = 0x08,
	All         = 0x0f,
};
using ImageRoundCorners = base::flags<ImageRoundCorner>;
inline constexpr auto is_flag_type(ImageRoundCorner) { return true; };

inline quint32 packInt(qint32 a) {
	return (a < 0) ? quint32(qint64(a) + 0x100000000LL) : quint32(a);
}
inline qint32 unpackInt(quint32 a) {
	return (a > 0x7FFFFFFFU) ? qint32(qint64(a) - 0x100000000LL) : qint32(a);
}
inline quint64 packUIntUInt(quint32 a, quint32 b) {
	return (quint64(a) << 32) | quint64(b);
}
inline quint64 packUIntInt(quint32 a, qint32 b) {
	return packUIntUInt(a, packInt(b));
}
inline quint64 packIntUInt(qint32 a, quint32 b) {
	return packUIntUInt(packInt(a), b);
}
inline quint64 packIntInt(qint32 a, qint32 b) {
	return packUIntUInt(packInt(a), packInt(b));
}
inline quint32 unpackUIntFirst(quint64 v) {
	return quint32(v >> 32);
}
inline qint32 unpackIntFirst(quint64 v) {
	return unpackInt(unpackUIntFirst(v));
}
inline quint32 unpackUIntSecond(quint64 v) {
	return quint32(v & 0xFFFFFFFFULL);
}
inline qint32 unpackIntSecond(quint64 v) {
	return unpackInt(unpackUIntSecond(v));
}

class StorageImageLocation {
public:
	StorageImageLocation() = default;
	StorageImageLocation(qint32 width, qint32 height, qint32 dc, const quint64 &volume, qint32 local, const quint64 &secret) : _widthheight(packIntInt(width, height)), _dclocal(packIntInt(dc, local)), _volume(volume), _secret(secret) {
	}
	StorageImageLocation(qint32 width, qint32 height, const MTPDfileLocation &location) : _widthheight(packIntInt(width, height)), _dclocal(packIntInt(location.vdc_id.v, location.vlocal_id.v)), _volume(location.vvolume_id.v), _secret(location.vsecret.v) {
	}
	bool isNull() const {
		return !_dclocal;
	}
	qint32 width() const {
		return unpackIntFirst(_widthheight);
	}
	qint32 height() const {
		return unpackIntSecond(_widthheight);
	}
	void setSize(qint32 width, qint32 height) {
		_widthheight = packIntInt(width, height);
	}
	qint32 dc() const {
		return unpackIntFirst(_dclocal);
	}
	quint64 volume() const {
		return _volume;
	}
	qint32 local() const {
		return unpackIntSecond(_dclocal);
	}
	quint64 secret() const {
		return _secret;
	}

	static StorageImageLocation Null;

private:
	quint64 _widthheight = 0;
	quint64 _dclocal = 0;
	quint64 _volume = 0;
	quint64 _secret = 0;

	friend inline bool operator==(const StorageImageLocation &a, const StorageImageLocation &b) {
		return (a._dclocal == b._dclocal) && (a._volume == b._volume) && (a._secret == b._secret);
	}

};

inline bool operator!=(const StorageImageLocation &a, const StorageImageLocation &b) {
	return !(a == b);
}

class WebFileImageLocation {
public:
	WebFileImageLocation() = default;
	WebFileImageLocation(qint32 width, qint32 height, qint32 dc, const QByteArray &url, quint64 accessHash) : _widthheight(packIntInt(width, height)), _accessHash(accessHash), _url(url), _dc(dc) {
	}
	bool isNull() const {
		return !_dc;
	}
	qint32 width() const {
		return unpackIntFirst(_widthheight);
	}
	qint32 height() const {
		return unpackIntSecond(_widthheight);
	}
	void setSize(qint32 width, qint32 height) {
		_widthheight = packIntInt(width, height);
	}
	qint32 dc() const {
		return _dc;
	}
	quint64 accessHash() const {
		return _accessHash;
	}
	const QByteArray &url() const {
		return _url;
	}

	static WebFileImageLocation Null;

private:
	quint64 _widthheight = 0;
	quint64 _accessHash = 0;
	QByteArray _url;
	qint32 _dc = 0;

	friend inline bool operator==(const WebFileImageLocation &a, const WebFileImageLocation &b) {
		return (a._dc == b._dc) && (a._accessHash == b._accessHash) && (a._url == b._url);
	}

};

inline bool operator!=(const WebFileImageLocation &a, const WebFileImageLocation &b) {
	return !(a == b);
}

namespace Images {

QImage prepareBlur(QImage image);
void prepareRound(QImage &image, ImageRoundRadius radius, ImageRoundCorners corners = ImageRoundCorner::All);
void prepareRound(QImage &image, QImage *cornerMasks, ImageRoundCorners corners = ImageRoundCorner::All);
void prepareCircle(QImage &image);
QImage prepareColored(style::color add, QImage image);
QImage prepareOpaque(QImage image);

enum class Option {
	None                  = 0,
	Smooth                = (1 << 0),
	Blurred               = (1 << 1),
	Circled               = (1 << 2),
	RoundedLarge          = (1 << 3),
	RoundedSmall          = (1 << 4),
	RoundedTopLeft        = (1 << 5),
	RoundedTopRight       = (1 << 6),
	RoundedBottomLeft     = (1 << 7),
	RoundedBottomRight    = (1 << 8),
	Colored               = (1 << 9),
	TransparentBackground = (1 << 10),
};
using Options = base::flags<Option>;
inline constexpr auto is_flag_type(Option) { return true; };

QImage prepare(QImage img, int w, int h, Options options, int outerw, int outerh, const style::color *colored = nullptr);

inline QPixmap pixmap(QImage img, int w, int h, Options options, int outerw, int outerh, const style::color *colored = nullptr) {
	return QPixmap::fromImage(prepare(img, w, h, options, outerw, outerh, colored), Qt::ColorOnly);
}

} // namespace Images

class DelayedStorageImage;

class HistoryItem;
class Image {
public:
	Image(const QString &file, QByteArray format = QByteArray());
	Image(const QByteArray &filecontent, QByteArray format = QByteArray());
	Image(const QPixmap &pixmap, QByteArray format = QByteArray());
	Image(const QByteArray &filecontent, QByteArray format, const QPixmap &pixmap);

	virtual void automaticLoad(const HistoryItem *item) { // auto load photo
	}
	virtual void automaticLoadSettingsChanged() {
	}

	virtual bool loaded() const {
		return true;
	}
	virtual bool loading() const {
		return false;
	}
	virtual bool displayLoading() const {
		return false;
	}
	virtual void cancel() {
	}
	virtual double progress() const {
		return 1;
	}
	virtual qint32 loadOffset() const {
		return 0;
	}

	const QPixmap &pix(qint32 w = 0, qint32 h = 0) const;
	const QPixmap &pixRounded(qint32 w = 0, qint32 h = 0, ImageRoundRadius radius = ImageRoundRadius::None, ImageRoundCorners corners = ImageRoundCorner::All) const;
	const QPixmap &pixBlurred(qint32 w = 0, qint32 h = 0) const;
	const QPixmap &pixColored(style::color add, qint32 w = 0, qint32 h = 0) const;
	const QPixmap &pixBlurredColored(style::color add, qint32 w = 0, qint32 h = 0) const;
	const QPixmap &pixSingle(qint32 w, qint32 h, qint32 outerw, qint32 outerh, ImageRoundRadius radius, ImageRoundCorners corners = ImageRoundCorner::All, const style::color *colored = nullptr) const;
	const QPixmap &pixBlurredSingle(qint32 w, qint32 h, qint32 outerw, qint32 outerh, ImageRoundRadius radius, ImageRoundCorners corners = ImageRoundCorner::All) const;
	const QPixmap &pixCircled(qint32 w = 0, qint32 h = 0) const;
	const QPixmap &pixBlurredCircled(qint32 w = 0, qint32 h = 0) const;
	QPixmap pixNoCache(int w = 0, int h = 0, Images::Options options = 0, int outerw = -1, int outerh = -1, const style::color *colored = nullptr) const;
	QPixmap pixColoredNoCache(style::color add, qint32 w = 0, qint32 h = 0, bool smooth = false) const;
	QPixmap pixBlurredColoredNoCache(style::color add, qint32 w, qint32 h = 0) const;

	qint32 width() const {
		return qMax(countWidth(), 1);
	}

	qint32 height() const {
		return qMax(countHeight(), 1);
	}

	virtual void load(bool loadFirst = false, bool prior = true) {
	}

	virtual void loadEvenCancelled(bool loadFirst = false, bool prior = true) {
	}

	virtual const StorageImageLocation &location() const {
		return StorageImageLocation::Null;
	}

	bool isNull() const;

	void forget() const;

	QByteArray savedFormat() const {
		return _format;
	}
	QByteArray savedData() const {
		return _saved;
	}

	virtual DelayedStorageImage *toDelayedStorageImage() {
		return 0;
	}
	virtual const DelayedStorageImage *toDelayedStorageImage() const {
		return 0;
	}

	virtual ~Image();

protected:
	Image(QByteArray format = "PNG") : _format(format), _forgot(false) {
	}

	void restore() const;
	virtual void checkload() const {
	}
	void invalidateSizeCache() const;

	virtual qint32 countWidth() const {
		restore();
		return _data.width();
	}

	virtual qint32 countHeight() const {
		restore();
		return _data.height();
	}

	mutable QByteArray _saved, _format;
	mutable bool _forgot;
	mutable QPixmap _data;

private:
	using Sizes = QMap<quint64, QPixmap>;
	mutable Sizes _sizesCache;

};

typedef QPair<quint64, quint64> StorageKey;
inline quint64 storageMix32To64(qint32 a, qint32 b) {
	return (quint64(*reinterpret_cast<quint32*>(&a)) << 32) | quint64(*reinterpret_cast<quint32*>(&b));
}
inline StorageKey storageKey(qint32 dc, const quint64 &volume, qint32 local) {
	return StorageKey(storageMix32To64(dc, local), volume);
}
inline StorageKey storageKey(const MTPDfileLocation &location) {
	return storageKey(location.vdc_id.v, location.vvolume_id.v, location.vlocal_id.v);
}
inline StorageKey storageKey(const StorageImageLocation &location) {
	return storageKey(location.dc(), location.volume(), location.local());
}
inline StorageKey storageKey(const WebFileImageLocation &location) {
	auto url = location.url();
	auto sha = hashSha1(url.data(), url.size());
	return storageKey(location.dc(), *reinterpret_cast<const quint64*>(sha.data()), *reinterpret_cast<const qint32*>(sha.data() + sizeof(quint64)));
}

class RemoteImage : public Image {
public:
	void automaticLoad(const HistoryItem *item); // auto load photo
	void automaticLoadSettingsChanged();

	bool loaded() const;
	bool loading() const {
		return amLoading();
	}
	bool displayLoading() const;
	void cancel();
	double progress() const;
	qint32 loadOffset() const;

	void setData(QByteArray &bytes, const QByteArray &format = QByteArray());

	void load(bool loadFirst = false, bool prior = true);
	void loadEvenCancelled(bool loadFirst = false, bool prior = true);

	~RemoteImage();

protected:
	// If after loading the image we need to shrink it to fit into a
	// specific size, you can return this size here.
	virtual QSize shrinkBox() const {
		return QSize();
	}
	virtual void setInformation(qint32 size, qint32 width, qint32 height) = 0;
	virtual FileLoader *createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) = 0;

	void checkload() const {
		doCheckload();
	}
	void loadLocal();

private:
	mutable FileLoader *_loader = nullptr;
	bool amLoading() const;
	void doCheckload() const;

	void destroyLoaderDelayed(FileLoader *newValue = nullptr) const;

};

class StorageImage : public RemoteImage {
public:
	StorageImage(const StorageImageLocation &location, qint32 size = 0);
	StorageImage(const StorageImageLocation &location, QByteArray &bytes);

	const StorageImageLocation &location() const override {
		return _location;
	}

protected:
	void setInformation(qint32 size, qint32 width, qint32 height) override;
	FileLoader *createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) override;

	StorageImageLocation _location;
	qint32 _size;

	qint32 countWidth() const override;
	qint32 countHeight() const override;

};

class WebFileImage : public RemoteImage {
public:
	WebFileImage(const WebFileImageLocation &location, qint32 size = 0);

protected:
	void setInformation(qint32 size, qint32 width, qint32 height) override;
	FileLoader *createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) override;

	WebFileImageLocation _location;
	qint32 _size;

	qint32 countWidth() const override;
	qint32 countHeight() const override;

};

class DelayedStorageImage : public StorageImage {
public:

	DelayedStorageImage();
	DelayedStorageImage(qint32 w, qint32 h);
	DelayedStorageImage(QByteArray &bytes);

	void setStorageLocation(const StorageImageLocation location);

	virtual DelayedStorageImage *toDelayedStorageImage() {
		return this;
	}
	virtual const DelayedStorageImage *toDelayedStorageImage() const {
		return this;
	}

	void automaticLoad(const HistoryItem *item); // auto load photo
	void automaticLoadSettingsChanged();

	bool loading() const {
		return _location.isNull() ? _loadRequested : StorageImage::loading();
	}
	bool displayLoading() const;
	void cancel();

	void load(bool loadFirst = false, bool prior = true);
	void loadEvenCancelled(bool loadFirst = false, bool prior = true);

private:
	bool _loadRequested, _loadCancelled, _loadFromCloud;

};

class WebImage : public RemoteImage {
public:

	// If !box.isEmpty() then resize the image to fit in this box.
	WebImage(const QString &url, QSize box = QSize());
	WebImage(const QString &url, int width, int height);

	void setSize(int width, int height);

protected:

	QSize shrinkBox() const override {
		return _box;
	}
	void setInformation(qint32 size, qint32 width, qint32 height) override;
	FileLoader *createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) override;

	qint32 countWidth() const override;
	qint32 countHeight() const override;

private:
	QString _url;
	QSize _box;
	qint32 _size, _width, _height;

};

namespace internal {
	Image *getImage(const QString &file, QByteArray format);
	Image *getImage(const QString &url, QSize box);
	Image *getImage(const QString &url, int width, int height);
	Image *getImage(const QByteArray &filecontent, QByteArray format);
	Image *getImage(const QPixmap &pixmap, QByteArray format);
	Image *getImage(const QByteArray &filecontent, QByteArray format, const QPixmap &pixmap);
	Image *getImage(qint32 width, qint32 height);
	StorageImage *getImage(const StorageImageLocation &location, qint32 size = 0);
	StorageImage *getImage(const StorageImageLocation &location, const QByteArray &bytes);
	WebFileImage *getImage(const WebFileImageLocation &location, qint32 size = 0);
} // namespace internal

class ImagePtr : public ManagedPtr<Image> {
public:
	ImagePtr();
	ImagePtr(const QString &file, QByteArray format = QByteArray()) : Parent(internal::getImage(file, format)) {
	}
	ImagePtr(const QString &url, QSize box) : Parent(internal::getImage(url, box)) {
	}
	ImagePtr(const QString &url, int width, int height) : Parent(internal::getImage(url, width, height)) {
	}
	ImagePtr(const QByteArray &filecontent, QByteArray format = QByteArray()) : Parent(internal::getImage(filecontent, format)) {
	}
	ImagePtr(const QByteArray &filecontent, QByteArray format, const QPixmap &pixmap) : Parent(internal::getImage(filecontent, format, pixmap)) {
	}
	ImagePtr(const QPixmap &pixmap, QByteArray format) : Parent(internal::getImage(pixmap, format)) {
	}
	ImagePtr(const StorageImageLocation &location, qint32 size = 0) : Parent(internal::getImage(location, size)) {
	}
	ImagePtr(const StorageImageLocation &location, const QByteArray &bytes) : Parent(internal::getImage(location, bytes)) {
	}
	ImagePtr(const WebFileImageLocation &location, qint32 size = 0) : Parent(internal::getImage(location, size)) {
	}
	ImagePtr(qint32 width, qint32 height, const MTPFileLocation &location, ImagePtr def = ImagePtr());
	ImagePtr(qint32 width, qint32 height) : Parent(internal::getImage(width, height)) {
	}

	explicit operator bool() const {
		return (_data != nullptr) && !_data->isNull();
	}

};

inline QSize shrinkToKeepAspect(qint32 width, qint32 height, qint32 towidth, qint32 toheight) {
	qint32 w = qMax(width, 1), h = qMax(height, 1);
	if (w * toheight > h * towidth) {
		h = qRound(h * towidth / double(w));
		w = towidth;
	} else {
		w = qRound(w * toheight / double(h));
		h = toheight;
	}
	return QSize(qMax(w, 1), qMax(h, 1));
}

void clearStorageImages();
void clearAllImages();
qint64 imageCacheSize();

class PsFileBookmark;
class ReadAccessEnabler {
public:
	ReadAccessEnabler(const PsFileBookmark *bookmark);
	ReadAccessEnabler(const QSharedPointer<PsFileBookmark> &bookmark);
	bool failed() const {
		return _failed;
	}
	~ReadAccessEnabler();

private:
	const PsFileBookmark *_bookmark;
	bool _failed;

};

class FileLocation {
public:
	FileLocation() = default;
	explicit FileLocation(const QString &name);

	bool check() const;
	const QString &name() const;
	void setBookmark(const QByteArray &bookmark);
	QByteArray bookmark() const;
	bool isEmpty() const {
		return name().isEmpty();
	}

	bool accessEnable() const;
	void accessDisable() const;

	QString fname;
	QDateTime modified;
	qint32 size;

private:
	QSharedPointer<PsFileBookmark> _bookmark;

};
inline bool operator==(const FileLocation &a, const FileLocation &b) {
	return (a.name() == b.name()) && (a.modified == b.modified) && (a.size == b.size);
}
inline bool operator!=(const FileLocation &a, const FileLocation &b) {
	return !(a == b);
}
