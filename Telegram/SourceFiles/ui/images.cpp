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
#include "ui/images.h"

#include "auth_session.h"
#include "mainwidget.h"
#include "platform/platform_specific.h"
#include "storage/localstorage.h"

#include <QBuffer>
#include <QImageReader>

namespace Images {
namespace {

FORCE_INLINE quint64 blurGetColors(const uchar *p) {
	return (quint64)p[0] + ((quint64)p[1] << 16) + ((quint64)p[2] << 32) + ((quint64)p[3] << 48);
}

const QPixmap &circleMask(int width, int height) {
	Assert(Global::started());

	quint64 key = quint64(quint32(width)) << 32 | quint64(quint32(height));

	Global::CircleMasksMap &masks(Global::RefCircleMasks());
	auto i = masks.constFind(key);
	if (i == masks.cend()) {
		QImage mask(width, height, QImage::Format_ARGB32_Premultiplied);
		{
			Painter p(&mask);
			PainterHighQualityEnabler hq(p);

			p.setCompositionMode(QPainter::CompositionMode_Source);
			p.fillRect(0, 0, width, height, Qt::transparent);
			p.setBrush(Qt::white);
			p.setPen(Qt::NoPen);
			p.drawEllipse(0, 0, width, height);
		}
		mask.setDevicePixelRatio(cRetinaFactor());
		i = masks.insert(key, App::pixmapFromImageInPlace(std::move(mask)));
	}
	return i.value();
}

} // namespace

QImage prepareBlur(QImage img) {
	auto ratio = img.devicePixelRatio();
	auto fmt = img.format();
	if (fmt != QImage::Format_RGB32 && fmt != QImage::Format_ARGB32_Premultiplied) {
		img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
		img.setDevicePixelRatio(ratio);
		Assert(!img.isNull());
	}

	uchar *pix = img.bits();
	if (pix) {
		int w = img.width(), h = img.height();
		const int radius = 3;
		const int r1 = radius + 1;
		const int div = radius * 2 + 1;
		const int stride = w * 4;
		if (radius < 16 && div < w && div < h && stride <= w * 4) {
			bool withalpha = img.hasAlphaChannel();
			if (withalpha) {
				QImage imgsmall(w, h, img.format());
				{
					Painter p(&imgsmall);
					PainterHighQualityEnabler hq(p);

					p.setCompositionMode(QPainter::CompositionMode_Source);
					p.fillRect(0, 0, w, h, Qt::transparent);
					p.drawImage(QRect(radius, radius, w - 2 * radius, h - 2 * radius), img, QRect(0, 0, w, h));
				}
				imgsmall.setDevicePixelRatio(ratio);
				auto was = img;
				img = std::move(imgsmall);
				imgsmall = QImage();
				Assert(!img.isNull());

				pix = img.bits();
				if (!pix) return was;
			}
			quint64 *rgb = new quint64[w * h];

			int x, y, i;

			int yw = 0;
			const int we = w - r1;
			for (y = 0; y < h; y++) {
				quint64 cur = blurGetColors(&pix[yw]);
				quint64 rgballsum = -radius * cur;
				quint64 rgbsum = cur * ((r1 * (r1 + 1)) >> 1);

				for (i = 1; i <= radius; i++) {
					quint64 cur = blurGetColors(&pix[yw + i * 4]);
					rgbsum += cur * (r1 - i);
					rgballsum += cur;
				}

				x = 0;

#define update(start, middle, end)                                                                                     \
	rgb[y * w + x] = (rgbsum >> 4) & 0x00FF00FF00FF00FFLL;                                                             \
	rgballsum += blurGetColors(&pix[yw + (start)*4]) - 2 * blurGetColors(&pix[yw + (middle)*4]) +                      \
	             blurGetColors(&pix[yw + (end)*4]);                                                                    \
	rgbsum += rgballsum;                                                                                               \
	x++;

				while (x < r1) {
					update(0, x, x + r1);
				}
				while (x < we) {
					update(x - r1, x, x + r1);
				}
				while (x < w) {
					update(x - r1, x, w - 1);
				}

#undef update

				yw += stride;
			}

			const int he = h - r1;
			for (x = 0; x < w; x++) {
				quint64 rgballsum = -radius * rgb[x];
				quint64 rgbsum = rgb[x] * ((r1 * (r1 + 1)) >> 1);
				for (i = 1; i <= radius; i++) {
					rgbsum += rgb[i * w + x] * (r1 - i);
					rgballsum += rgb[i * w + x];
				}

				y = 0;
				int yi = x * 4;

#define update(start, middle, end)                                                                                     \
	quint64 res = rgbsum >> 4;                                                                                         \
	pix[yi] = res & 0xFF;                                                                                              \
	pix[yi + 1] = (res >> 16) & 0xFF;                                                                                  \
	pix[yi + 2] = (res >> 32) & 0xFF;                                                                                  \
	pix[yi + 3] = (res >> 48) & 0xFF;                                                                                  \
	rgballsum += rgb[x + (start)*w] - 2 * rgb[x + (middle)*w] + rgb[x + (end)*w];                                      \
	rgbsum += rgballsum;                                                                                               \
	y++;                                                                                                               \
	yi += stride;

				while (y < r1) {
					update(0, y, y + r1);
				}
				while (y < he) {
					update(y - r1, y, y + r1);
				}
				while (y < h) {
					update(y - r1, y, h - 1);
				}

#undef update
			}

			delete[] rgb;
		}
	}
	return img;
}

void prepareCircle(QImage &img) {
	Assert(!img.isNull());

	img.setDevicePixelRatio(cRetinaFactor());
	img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	Assert(!img.isNull());

	QPixmap mask = circleMask(img.width(), img.height());
	Painter p(&img);
	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	p.drawPixmap(0, 0, mask);
}

void prepareRound(QImage &image, ImageRoundRadius radius, ImageRoundCorners corners) {
	if (!static_cast<int>(corners)) {
		return;
	} else if (radius == ImageRoundRadius::Ellipse) {
		Assert(corners == ImageRoundCorners(ImageRoundCorner::All));
		prepareCircle(image);
	}
	Assert(!image.isNull());

	image.setDevicePixelRatio(cRetinaFactor());
	image = std::move(image).convertToFormat(QImage::Format_ARGB32_Premultiplied);
	Assert(!image.isNull());

	auto masks = App::cornersMask(radius);
	prepareRound(image, masks, corners);
}

void prepareRound(QImage &image, QImage *cornerMasks, ImageRoundCorners corners) {
	auto cornerWidth = cornerMasks[0].width();
	auto cornerHeight = cornerMasks[0].height();
	auto imageWidth = image.width();
	auto imageHeight = image.height();
	if (imageWidth < 2 * cornerWidth || imageHeight < 2 * cornerHeight) {
		return;
	}
	constexpr auto imageIntsPerPixel = 1;
	auto imageIntsPerLine = (image.bytesPerLine() >> 2);
	Assert(image.depth() == static_cast<int>((imageIntsPerPixel * sizeof(quint32)) << 3));
	Assert(image.bytesPerLine() == (imageIntsPerLine << 2));

	auto ints = reinterpret_cast<quint32 *>(image.bits());
	auto intsTopLeft = ints;
	auto intsTopRight = ints + imageWidth - cornerWidth;
	auto intsBottomLeft = ints + (imageHeight - cornerHeight) * imageWidth;
	auto intsBottomRight = ints + (imageHeight - cornerHeight + 1) * imageWidth - cornerWidth;
	auto maskCorner = [imageIntsPerPixel, imageIntsPerLine](quint32 *imageInts, const QImage &mask) {
		auto maskWidth = mask.width();
		auto maskHeight = mask.height();
		auto maskBytesPerPixel = (mask.depth() >> 3);
		auto maskBytesPerLine = mask.bytesPerLine();
		auto maskBytesAdded = maskBytesPerLine - maskWidth * maskBytesPerPixel;
		auto maskBytes = mask.constBits();
		Assert(maskBytesAdded >= 0);
		Assert(mask.depth() == (maskBytesPerPixel << 3));
		auto imageIntsAdded = imageIntsPerLine - maskWidth * imageIntsPerPixel;
		Assert(imageIntsAdded >= 0);
		for (auto y = 0; y != maskHeight; ++y) {
			for (auto x = 0; x != maskWidth; ++x) {
				auto opacity = static_cast<anim::ShiftedMultiplier>(*maskBytes) + 1;
				*imageInts = anim::unshifted(anim::shifted(*imageInts) * opacity);
				maskBytes += maskBytesPerPixel;
				imageInts += imageIntsPerPixel;
			}
			maskBytes += maskBytesAdded;
			imageInts += imageIntsAdded;
		}
	};
	if (corners & ImageRoundCorner::TopLeft) maskCorner(intsTopLeft, cornerMasks[0]);
	if (corners & ImageRoundCorner::TopRight) maskCorner(intsTopRight, cornerMasks[1]);
	if (corners & ImageRoundCorner::BottomLeft) maskCorner(intsBottomLeft, cornerMasks[2]);
	if (corners & ImageRoundCorner::BottomRight) maskCorner(intsBottomRight, cornerMasks[3]);
}

QImage prepareColored(style::color add, QImage image) {
	auto format = image.format();
	if (format != QImage::Format_RGB32 && format != QImage::Format_ARGB32_Premultiplied) {
		image = std::move(image).convertToFormat(QImage::Format_ARGB32_Premultiplied);
	}

	if (auto pix = image.bits()) {
		int ca = int(add->c.alphaF() * 0xFF), cr = int(add->c.redF() * 0xFF), cg = int(add->c.greenF() * 0xFF),
		    cb = int(add->c.blueF() * 0xFF);
		const int w = image.width(), h = image.height(), size = w * h * 4;
		for (qint32 i = 0; i < size; i += 4) {
			int b = pix[i], g = pix[i + 1], r = pix[i + 2], a = pix[i + 3], aca = a * ca;
			pix[i + 0] = uchar(b + ((aca * (cb - b)) >> 16));
			pix[i + 1] = uchar(g + ((aca * (cg - g)) >> 16));
			pix[i + 2] = uchar(r + ((aca * (cr - r)) >> 16));
			pix[i + 3] = uchar(a + ((aca * (0xFF - a)) >> 16));
		}
	}
	return image;
}

QImage prepareOpaque(QImage image) {
	if (image.hasAlphaChannel()) {
		image = std::move(image).convertToFormat(QImage::Format_ARGB32_Premultiplied);
		auto ints = reinterpret_cast<quint32 *>(image.bits());
		auto bg = anim::shifted(st::imageBgTransparent->c);
		auto width = image.width();
		auto height = image.height();
		auto addPerLine = (image.bytesPerLine() / sizeof(quint32)) - width;
		for (auto y = 0; y != height; ++y) {
			for (auto x = 0; x != width; ++x) {
				auto components = anim::shifted(*ints);
				*ints++ = anim::unshifted(components * 256 + bg * (256 - anim::getAlpha(components)));
			}
			ints += addPerLine;
		}
	}
	return image;
}

QImage prepare(QImage img, int w, int h, Images::Options options, int outerw, int outerh, const style::color *colored) {
	Assert(!img.isNull());
	if (options & Images::Option::Blurred) {
		img = prepareBlur(std::move(img));
		Assert(!img.isNull());
	}
	if (w <= 0 || (w == img.width() && (h <= 0 || h == img.height()))) {
	} else if (h <= 0) {
		img = img.scaledToWidth(w,
		                        (options & Images::Option::Smooth) ? Qt::SmoothTransformation : Qt::FastTransformation);
		Assert(!img.isNull());
	} else {
		img = img.scaled(w, h, Qt::IgnoreAspectRatio,
		                 (options & Images::Option::Smooth) ? Qt::SmoothTransformation : Qt::FastTransformation);
		Assert(!img.isNull());
	}
	if (outerw > 0 && outerh > 0) {
		outerw *= cIntRetinaFactor();
		outerh *= cIntRetinaFactor();
		if (outerw != w || outerh != h) {
			img.setDevicePixelRatio(cRetinaFactor());
			auto result = QImage(outerw, outerh, QImage::Format_ARGB32_Premultiplied);
			result.setDevicePixelRatio(cRetinaFactor());
			if (options & Images::Option::TransparentBackground) {
				result.fill(Qt::transparent);
			}
			{
				QPainter p(&result);
				if (w < outerw || h < outerh) {
					p.fillRect(0, 0, result.width(), result.height(), st::imageBg);
				}
				p.drawImage((result.width() - img.width()) / (2 * cIntRetinaFactor()),
				            (result.height() - img.height()) / (2 * cIntRetinaFactor()), img);
			}
			img = result;
			Assert(!img.isNull());
		}
	}
	auto corners = [](Images::Options options) {
		return ((options & Images::Option::RoundedTopLeft) ? ImageRoundCorner::TopLeft : ImageRoundCorner::None) |
		       ((options & Images::Option::RoundedTopRight) ? ImageRoundCorner::TopRight : ImageRoundCorner::None) |
		       ((options & Images::Option::RoundedBottomLeft) ? ImageRoundCorner::BottomLeft : ImageRoundCorner::None) |
		       ((options & Images::Option::RoundedBottomRight) ? ImageRoundCorner::BottomRight :
		                                                         ImageRoundCorner::None);
	};
	if (options & Images::Option::Circled) {
		prepareCircle(img);
		Assert(!img.isNull());
	} else if (options & Images::Option::RoundedLarge) {
		prepareRound(img, ImageRoundRadius::Large, corners(options));
		Assert(!img.isNull());
	} else if (options & Images::Option::RoundedSmall) {
		prepareRound(img, ImageRoundRadius::Small, corners(options));
		Assert(!img.isNull());
	}
	if (options & Images::Option::Colored) {
		Assert(colored != nullptr);
		img = prepareColored(*colored, std::move(img));
	}
	img.setDevicePixelRatio(cRetinaFactor());
	return img;
}

} // namespace Images

namespace {

using LocalImages = QMap<QString, Image *>;
LocalImages localImages;

using WebImages = QMap<QString, WebImage *>;
WebImages webImages;

Image *generateBlankImage() {
	auto data = QImage(cIntRetinaFactor(), cIntRetinaFactor(), QImage::Format_ARGB32_Premultiplied);
	data.fill(Qt::transparent);
	data.setDevicePixelRatio(cRetinaFactor());
	return internal::getImage(App::pixmapFromImageInPlace(std::move(data)), "GIF");
}

Image *blank() {
	static auto blankImage = generateBlankImage();
	return blankImage;
}

using StorageImages = QMap<StorageKey, StorageImage *>;
StorageImages storageImages;

using WebFileImages = QMap<StorageKey, WebFileImage *>;
WebFileImages webFileImages;

qint64 globalAcquiredSize = 0;

quint64 PixKey(int width, int height, Images::Options options) {
	return static_cast<quint64>(width) | (static_cast<quint64>(height) << 24) | (static_cast<quint64>(options) << 48);
}

quint64 SinglePixKey(Images::Options options) {
	return PixKey(0, 0, options);
}

} // namespace

StorageImageLocation StorageImageLocation::Null;
WebFileImageLocation WebFileImageLocation::Null;

bool Image::isNull() const {
	return (this == blank());
}

ImagePtr::ImagePtr()
    : Parent(blank()) {}

ImagePtr::ImagePtr(qint32 width, qint32 height, const MTPFileLocation &location, ImagePtr def)
    : Parent((location.type() == mtpc_fileLocation) ?
                 (Image *)(internal::getImage(StorageImageLocation(width, height, location.c_fileLocation()))) :
                 def.v()) {}

Image::Image(const QString &file, QByteArray fmt)
    : _forgot(false) {
	_data = App::pixmapFromImageInPlace(App::readImage(file, &fmt, false, 0, &_saved));
	_format = fmt;
	if (!_data.isNull()) {
		globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;
	}
}

Image::Image(const QByteArray &filecontent, QByteArray fmt)
    : _forgot(false) {
	_data = App::pixmapFromImageInPlace(App::readImage(filecontent, &fmt, false));
	_format = fmt;
	_saved = filecontent;
	if (!_data.isNull()) {
		globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;
	}
}

Image::Image(const QPixmap &pixmap, QByteArray format)
    : _format(format)
    , _forgot(false)
    , _data(pixmap) {
	if (!_data.isNull()) {
		globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;
	}
}

Image::Image(const QByteArray &filecontent, QByteArray fmt, const QPixmap &pixmap)
    : _saved(filecontent)
    , _format(fmt)
    , _forgot(false)
    , _data(pixmap) {
	_data = pixmap;
	_format = fmt;
	_saved = filecontent;
	if (!_data.isNull()) {
		globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;
	}
}

const QPixmap &Image::pix(qint32 w, qint32 h) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Smooth | Images::Option::None;
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixNoCache(w, h, options);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixRounded(qint32 w, qint32 h, ImageRoundRadius radius, ImageRoundCorners corners) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Smooth | Images::Option::None;
	auto cornerOptions = [](ImageRoundCorners corners) {
		return (corners & ImageRoundCorner::TopLeft ? Images::Option::RoundedTopLeft : Images::Option::None) |
		       (corners & ImageRoundCorner::TopRight ? Images::Option::RoundedTopRight : Images::Option::None) |
		       (corners & ImageRoundCorner::BottomLeft ? Images::Option::RoundedBottomLeft : Images::Option::None) |
		       (corners & ImageRoundCorner::BottomRight ? Images::Option::RoundedBottomRight : Images::Option::None);
	};
	if (radius == ImageRoundRadius::Large) {
		options |= Images::Option::RoundedLarge | cornerOptions(corners);
	} else if (radius == ImageRoundRadius::Small) {
		options |= Images::Option::RoundedSmall | cornerOptions(corners);
	} else if (radius == ImageRoundRadius::Ellipse) {
		options |= Images::Option::Circled | cornerOptions(corners);
	}
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixNoCache(w, h, options);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixCircled(qint32 w, qint32 h) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Smooth | Images::Option::Circled;
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixNoCache(w, h, options);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixBlurredCircled(qint32 w, qint32 h) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Smooth | Images::Option::Circled | Images::Option::Blurred;
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixNoCache(w, h, options);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixBlurred(qint32 w, qint32 h) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width() * cIntRetinaFactor();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Smooth | Images::Option::Blurred;
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixNoCache(w, h, options);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixColored(style::color add, qint32 w, qint32 h) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width() * cIntRetinaFactor();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Smooth | Images::Option::Colored;
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixColoredNoCache(add, w, h, true);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixBlurredColored(style::color add, qint32 w, qint32 h) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width() * cIntRetinaFactor();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}
	auto options = Images::Option::Blurred | Images::Option::Smooth | Images::Option::Colored;
	auto k = PixKey(w, h, options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend()) {
		auto p = pixBlurredColoredNoCache(add, w, h);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixSingle(qint32 w, qint32 h, qint32 outerw, qint32 outerh, ImageRoundRadius radius,
                                ImageRoundCorners corners, const style::color *colored) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width() * cIntRetinaFactor();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}

	auto options = Images::Option::Smooth | Images::Option::None;
	auto cornerOptions = [](ImageRoundCorners corners) {
		return (corners & ImageRoundCorner::TopLeft ? Images::Option::RoundedTopLeft : Images::Option::None) |
		       (corners & ImageRoundCorner::TopRight ? Images::Option::RoundedTopRight : Images::Option::None) |
		       (corners & ImageRoundCorner::BottomLeft ? Images::Option::RoundedBottomLeft : Images::Option::None) |
		       (corners & ImageRoundCorner::BottomRight ? Images::Option::RoundedBottomRight : Images::Option::None);
	};
	if (radius == ImageRoundRadius::Large) {
		options |= Images::Option::RoundedLarge | cornerOptions(corners);
	} else if (radius == ImageRoundRadius::Small) {
		options |= Images::Option::RoundedSmall | cornerOptions(corners);
	} else if (radius == ImageRoundRadius::Ellipse) {
		options |= Images::Option::Circled | cornerOptions(corners);
	}
	if (colored) {
		options |= Images::Option::Colored;
	}

	auto k = SinglePixKey(options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend() || i->width() != (outerw * cIntRetinaFactor()) ||
	    i->height() != (outerh * cIntRetinaFactor())) {
		if (i != _sizesCache.cend()) {
			globalAcquiredSize -= qint64(i->width()) * i->height() * 4;
		}
		auto p = pixNoCache(w, h, options, outerw, outerh, colored);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

const QPixmap &Image::pixBlurredSingle(int w, int h, qint32 outerw, qint32 outerh, ImageRoundRadius radius,
                                       ImageRoundCorners corners) const {
	checkload();

	if (w <= 0 || !width() || !height()) {
		w = width() * cIntRetinaFactor();
	} else if (cRetina()) {
		w *= cIntRetinaFactor();
		h *= cIntRetinaFactor();
	}

	auto options = Images::Option::Smooth | Images::Option::Blurred;
	auto cornerOptions = [](ImageRoundCorners corners) {
		return (corners & ImageRoundCorner::TopLeft ? Images::Option::RoundedTopLeft : Images::Option::None) |
		       (corners & ImageRoundCorner::TopRight ? Images::Option::RoundedTopRight : Images::Option::None) |
		       (corners & ImageRoundCorner::BottomLeft ? Images::Option::RoundedBottomLeft : Images::Option::None) |
		       (corners & ImageRoundCorner::BottomRight ? Images::Option::RoundedBottomRight : Images::Option::None);
	};
	if (radius == ImageRoundRadius::Large) {
		options |= Images::Option::RoundedLarge | cornerOptions(corners);
	} else if (radius == ImageRoundRadius::Small) {
		options |= Images::Option::RoundedSmall | cornerOptions(corners);
	} else if (radius == ImageRoundRadius::Ellipse) {
		options |= Images::Option::Circled | cornerOptions(corners);
	}

	auto k = SinglePixKey(options);
	auto i = _sizesCache.constFind(k);
	if (i == _sizesCache.cend() || i->width() != (outerw * cIntRetinaFactor()) ||
	    i->height() != (outerh * cIntRetinaFactor())) {
		if (i != _sizesCache.cend()) {
			globalAcquiredSize -= qint64(i->width()) * i->height() * 4;
		}
		auto p = pixNoCache(w, h, options, outerw, outerh);
		if (cRetina()) p.setDevicePixelRatio(cRetinaFactor());
		i = _sizesCache.insert(k, p);
		if (!p.isNull()) {
			globalAcquiredSize += qint64(p.width()) * p.height() * 4;
		}
	}
	return i.value();
}

QPixmap Image::pixNoCache(int w, int h, Images::Options options, int outerw, int outerh,
                          const style::color *colored) const {
	if (!loading()) const_cast<Image *>(this)->load();
	restore();

	if (_data.isNull()) {
		if (h <= 0 && height() > 0) {
			h = std::round(width() * w / double(height()));
		}
		return blank()->pixNoCache(w, h, options, outerw, outerh);
	}

	if (isNull() && outerw > 0 && outerh > 0) {
		outerw *= cIntRetinaFactor();
		outerh *= cIntRetinaFactor();

		QImage result(outerw, outerh, QImage::Format_ARGB32_Premultiplied);
		result.setDevicePixelRatio(cRetinaFactor());

		{
			QPainter p(&result);
			if (w < outerw) {
				p.fillRect(0, 0, (outerw - w) / 2, result.height(), st::imageBg);
				p.fillRect(((outerw - w) / 2) + w, 0, result.width() - (((outerw - w) / 2) + w), result.height(),
				           st::imageBg);
			}
			if (h < outerh) {
				p.fillRect(std::max(0, (outerw - w) / 2), 0, std::min(result.width(), w), (outerh - h) / 2,
				           st::imageBg);
				p.fillRect(std::max(0, (outerw - w) / 2), ((outerh - h) / 2) + h, std::min(result.width(), w),
				           result.height() - (((outerh - h) / 2) + h), st::imageBg);
			}
			p.fillRect(std::max(0, (outerw - w) / 2), std::max(0, (outerh - h) / 2), std::min(result.width(), w),
			           std::min(result.height(), h), st::imageBgTransparent);
		}

		auto corners = [](Images::Options options) {
			return ((options & Images::Option::RoundedTopLeft) ? ImageRoundCorner::TopLeft : ImageRoundCorner::None) |
			       ((options & Images::Option::RoundedTopRight) ? ImageRoundCorner::TopRight : ImageRoundCorner::None) |
			       ((options & Images::Option::RoundedBottomLeft) ? ImageRoundCorner::BottomLeft :
			                                                        ImageRoundCorner::None) |
			       ((options & Images::Option::RoundedBottomRight) ? ImageRoundCorner::BottomRight :
			                                                         ImageRoundCorner::None);
		};
		if (options & Images::Option::Circled) {
			Images::prepareCircle(result);
		} else if (options & Images::Option::RoundedLarge) {
			Images::prepareRound(result, ImageRoundRadius::Large, corners(options));
		} else if (options & Images::Option::RoundedSmall) {
			Images::prepareRound(result, ImageRoundRadius::Small, corners(options));
		}
		if (options & Images::Option::Colored) {
			Assert(colored != nullptr);
			result = Images::prepareColored(*colored, std::move(result));
		}
		return App::pixmapFromImageInPlace(std::move(result));
	}

	return Images::pixmap(_data.toImage(), w, h, options, outerw, outerh, colored);
}

QPixmap Image::pixColoredNoCache(style::color add, qint32 w, qint32 h, bool smooth) const {
	const_cast<Image *>(this)->load();
	restore();
	if (_data.isNull()) return blank()->pix();

	auto img = _data.toImage();
	if (w <= 0 || !width() || !height() || (w == width() && (h <= 0 || h == height()))) {
		return App::pixmapFromImageInPlace(Images::prepareColored(add, std::move(img)));
	}
	if (h <= 0) {
		return App::pixmapFromImageInPlace(Images::prepareColored(
		    add, img.scaledToWidth(w, smooth ? Qt::SmoothTransformation : Qt::FastTransformation)));
	}
	return App::pixmapFromImageInPlace(Images::prepareColored(
	    add, img.scaled(w, h, Qt::IgnoreAspectRatio, smooth ? Qt::SmoothTransformation : Qt::FastTransformation)));
}

QPixmap Image::pixBlurredColoredNoCache(style::color add, qint32 w, qint32 h) const {
	const_cast<Image *>(this)->load();
	restore();
	if (_data.isNull()) return blank()->pix();

	auto img = Images::prepareBlur(_data.toImage());
	if (h <= 0) {
		img = img.scaledToWidth(w, Qt::SmoothTransformation);
	} else {
		img = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

	return App::pixmapFromImageInPlace(Images::prepareColored(add, img));
}

void Image::forget() const {
	if (_forgot) return;

	if (_data.isNull()) return;

	invalidateSizeCache();
	if (_saved.isEmpty()) {
		QBuffer buffer(&_saved);
		if (!_data.save(&buffer, _format)) {
			if (_data.save(&buffer, "PNG")) {
				_format = "PNG";
			} else {
				return;
			}
		}
	}
	globalAcquiredSize -= qint64(_data.width()) * _data.height() * 4;
	_data = QPixmap();
	_forgot = true;
}

void Image::restore() const {
	if (!_forgot) return;

	QBuffer buffer(&_saved);
	QImageReader reader(&buffer, _format);
#ifndef OS_MAC_OLD
	reader.setAutoTransform(true);
#endif // OS_MAC_OLD
	_data = QPixmap::fromImageReader(&reader, Qt::ColorOnly);

	if (!_data.isNull()) {
		globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;
	}
	_forgot = false;
}

void Image::invalidateSizeCache() const {
	for (auto &pix : _sizesCache) {
		if (!pix.isNull()) {
			globalAcquiredSize -= qint64(pix.width()) * pix.height() * 4;
		}
	}
	_sizesCache.clear();
}

Image::~Image() {
	invalidateSizeCache();
	if (!_data.isNull()) {
		globalAcquiredSize -= qint64(_data.width()) * _data.height() * 4;
	}
}

void clearStorageImages() {
	for (auto image : base::take(storageImages)) {
		delete image;
	}
	for (auto image : base::take(webImages)) {
		delete image;
	}
	for (auto image : base::take(webFileImages)) {
		delete image;
	}
}

void clearAllImages() {
	for (auto image : base::take(localImages)) {
		delete image;
	}
	clearStorageImages();
}

qint64 imageCacheSize() {
	return globalAcquiredSize;
}

void RemoteImage::doCheckload() const {
	if (!amLoading() || !_loader->finished()) return;

	QPixmap data = _loader->imagePixmap(shrinkBox());
	if (data.isNull()) {
		destroyLoaderDelayed(CancelledFileLoader);
		return;
	}

	if (!_data.isNull()) {
		globalAcquiredSize -= qint64(_data.width()) * _data.height() * 4;
	}

	_format = _loader->imageFormat(shrinkBox());
	_data = data;
	_saved = _loader->bytes();
	const_cast<RemoteImage *>(this)->setInformation(_saved.size(), _data.width(), _data.height());
	globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;

	invalidateSizeCache();

	destroyLoaderDelayed();

	_forgot = false;
}

void RemoteImage::destroyLoaderDelayed(FileLoader *newValue) const {
	_loader->stop();
	auto loader = std::unique_ptr<FileLoader>(std::exchange(_loader, newValue));
	Auth().downloader().delayedDestroyLoader(std::move(loader));
}

void RemoteImage::loadLocal() {
	if (loaded() || amLoading()) return;

	_loader = createLoader(LoadFromLocalOnly, true);
	if (_loader) _loader->start();
}

void RemoteImage::setData(QByteArray &bytes, const QByteArray &bytesFormat) {
	QBuffer buffer(&bytes);

	if (!_data.isNull()) {
		globalAcquiredSize -= qint64(_data.width()) * _data.height() * 4;
	}
	QByteArray fmt(bytesFormat);
	_data = App::pixmapFromImageInPlace(App::readImage(bytes, &fmt, false));
	if (!_data.isNull()) {
		globalAcquiredSize += qint64(_data.width()) * _data.height() * 4;
		setInformation(bytes.size(), _data.width(), _data.height());
	}

	invalidateSizeCache();
	if (amLoading()) {
		destroyLoaderDelayed();
	}
	_saved = bytes;
	_format = fmt;
	_forgot = false;
}

bool RemoteImage::amLoading() const {
	return _loader && _loader != CancelledFileLoader;
}

void RemoteImage::automaticLoad(const HistoryItem *item) {
	if (loaded()) return;

	if (_loader != CancelledFileLoader && item) {
		bool loadFromCloud = false;
		if (item->history()->peer->isUser()) {
			loadFromCloud = !(cAutoDownloadPhoto() & dbiadNoPrivate);
		} else {
			loadFromCloud = !(cAutoDownloadPhoto() & dbiadNoGroups);
		}

		if (_loader) {
			if (loadFromCloud) _loader->permitLoadFromCloud();
		} else {
			_loader = createLoader(loadFromCloud ? LoadFromCloudOrLocal : LoadFromLocalOnly, true);
			if (_loader) _loader->start();
		}
	}
}

void RemoteImage::automaticLoadSettingsChanged() {
	if (loaded() || _loader != CancelledFileLoader) return;
	_loader = 0;
}

void RemoteImage::load(bool loadFirst, bool prior) {
	if (loaded()) return;

	if (!_loader) {
		_loader = createLoader(LoadFromCloudOrLocal, false);
	}
	if (amLoading()) {
		_loader->start(loadFirst, prior);
	}
}

void RemoteImage::loadEvenCancelled(bool loadFirst, bool prior) {
	if (_loader == CancelledFileLoader) _loader = 0;
	return load(loadFirst, prior);
}

RemoteImage::~RemoteImage() {
	if (!_data.isNull()) {
		globalAcquiredSize -= qint64(_data.width()) * _data.height() * 4;
	}
	if (amLoading()) {
		destroyLoaderDelayed();
	}
}

bool RemoteImage::loaded() const {
	doCheckload();
	return (!_data.isNull() || !_saved.isNull());
}

bool RemoteImage::displayLoading() const {
	return amLoading() && (!_loader->loadingLocal() || !_loader->autoLoading());
}

void RemoteImage::cancel() {
	if (!amLoading()) return;

	auto loader = std::exchange(_loader, CancelledFileLoader);
	loader->cancel();
	loader->stop();
	Auth().downloader().delayedDestroyLoader(std::unique_ptr<FileLoader>(loader));
}

double RemoteImage::progress() const {
	return amLoading() ? _loader->currentProgress() : (loaded() ? 1 : 0);
}

qint32 RemoteImage::loadOffset() const {
	return amLoading() ? _loader->currentOffset() : 0;
}

StorageImage::StorageImage(const StorageImageLocation &location, qint32 size)
    : _location(location)
    , _size(size) {}

StorageImage::StorageImage(const StorageImageLocation &location, QByteArray &bytes)
    : _location(location)
    , _size(bytes.size()) {
	setData(bytes);
	if (!_location.isNull()) {
		Local::writeImage(storageKey(_location), StorageImageSaved(bytes));
	}
}

qint32 StorageImage::countWidth() const {
	return _location.width();
}

qint32 StorageImage::countHeight() const {
	return _location.height();
}

void StorageImage::setInformation(qint32 size, qint32 width, qint32 height) {
	_size = size;
	_location.setSize(width, height);
}

FileLoader *StorageImage::createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) {
	if (_location.isNull()) return 0;
	return new mtpFileLoader(&_location, _size, fromCloud, autoLoading);
}

WebFileImage::WebFileImage(const WebFileImageLocation &location, qint32 size)
    : _location(location)
    , _size(size) {}

qint32 WebFileImage::countWidth() const {
	return _location.width();
}

qint32 WebFileImage::countHeight() const {
	return _location.height();
}

void WebFileImage::setInformation(qint32 size, qint32 width, qint32 height) {
	_size = size;
	_location.setSize(width, height);
}

FileLoader *WebFileImage::createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) {
	if (_location.isNull()) return 0;
	return new mtpFileLoader(&_location, _size, fromCloud, autoLoading);
}

DelayedStorageImage::DelayedStorageImage()
    : StorageImage(StorageImageLocation())
    , _loadRequested(false)
    , _loadCancelled(false)
    , _loadFromCloud(false) {}

DelayedStorageImage::DelayedStorageImage(qint32 w, qint32 h)
    : StorageImage(StorageImageLocation(w, h, 0, 0, 0, 0))
    , _loadRequested(false)
    , _loadCancelled(false)
    , _loadFromCloud(false) {}

DelayedStorageImage::DelayedStorageImage(QByteArray &bytes)
    : StorageImage(StorageImageLocation(), bytes)
    , _loadRequested(false)
    , _loadCancelled(false)
    , _loadFromCloud(false) {}

void DelayedStorageImage::setStorageLocation(const StorageImageLocation location) {
	_location = location;
	if (_loadRequested) {
		if (!_loadCancelled) {
			if (_loadFromCloud) {
				load();
			} else {
				loadLocal();
			}
		}
		_loadRequested = false;
	}
}

void DelayedStorageImage::automaticLoad(const HistoryItem *item) {
	if (_location.isNull()) {
		if (!_loadCancelled && item) {
			bool loadFromCloud = false;
			if (item->history()->peer->isUser()) {
				loadFromCloud = !(cAutoDownloadPhoto() & dbiadNoPrivate);
			} else {
				loadFromCloud = !(cAutoDownloadPhoto() & dbiadNoGroups);
			}

			if (_loadRequested) {
				if (loadFromCloud) _loadFromCloud = loadFromCloud;
			} else {
				_loadFromCloud = loadFromCloud;
				_loadRequested = true;
			}
		}
	} else {
		StorageImage::automaticLoad(item);
	}
}

void DelayedStorageImage::automaticLoadSettingsChanged() {
	if (_loadCancelled) _loadCancelled = false;
	StorageImage::automaticLoadSettingsChanged();
}

void DelayedStorageImage::load(bool loadFirst, bool prior) {
	if (_location.isNull()) {
		_loadRequested = _loadFromCloud = true;
	} else {
		StorageImage::load(loadFirst, prior);
	}
}

void DelayedStorageImage::loadEvenCancelled(bool loadFirst, bool prior) {
	_loadCancelled = false;
	StorageImage::loadEvenCancelled(loadFirst, prior);
}

bool DelayedStorageImage::displayLoading() const {
	return _location.isNull() ? true : StorageImage::displayLoading();
}

void DelayedStorageImage::cancel() {
	if (_loadRequested) {
		_loadRequested = false;
	}
	StorageImage::cancel();
}

WebImage::WebImage(const QString &url, QSize box)
    : _url(url)
    , _box(box)
    , _size(0)
    , _width(0)
    , _height(0) {}

WebImage::WebImage(const QString &url, int width, int height)
    : _url(url)
    , _size(0)
    , _width(width)
    , _height(height) {}

void WebImage::setSize(int width, int height) {
	_width = width;
	_height = height;
}

qint32 WebImage::countWidth() const {
	return _width;
}

qint32 WebImage::countHeight() const {
	return _height;
}

void WebImage::setInformation(qint32 size, qint32 width, qint32 height) {
	_size = size;
	setSize(width, height);
}

FileLoader *WebImage::createLoader(LoadFromCloudSetting fromCloud, bool autoLoading) {
	return new webFileLoader(_url, QString(), fromCloud, autoLoading);
}

namespace internal {

Image *getImage(const QString &file, QByteArray format) {
	if (file.startsWith(qstr("http://"), Qt::CaseInsensitive) ||
	    file.startsWith(qstr("https://"), Qt::CaseInsensitive)) {
		QString key = file;
		WebImages::const_iterator i = webImages.constFind(key);
		if (i == webImages.cend()) {
			i = webImages.insert(key, new WebImage(file));
		}
		return i.value();
	} else {
		QFileInfo f(file);
		QString key = qsl("//:%1//:%2//:").arg(f.size()).arg(f.lastModified().toTime_t()) + file;
		LocalImages::const_iterator i = localImages.constFind(key);
		if (i == localImages.cend()) {
			i = localImages.insert(key, new Image(file, format));
		}
		return i.value();
	}
}

Image *getImage(const QString &url, QSize box) {
	QString key = qsl("//:%1//:%2//:").arg(box.width()).arg(box.height()) + url;
	auto i = webImages.constFind(key);
	if (i == webImages.cend()) {
		i = webImages.insert(key, new WebImage(url, box));
	}
	return i.value();
}

Image *getImage(const QString &url, int width, int height) {
	QString key = url;
	auto i = webImages.constFind(key);
	if (i == webImages.cend()) {
		i = webImages.insert(key, new WebImage(url, width, height));
	} else {
		i.value()->setSize(width, height);
	}
	return i.value();
}

Image *getImage(const QByteArray &filecontent, QByteArray format) {
	return new Image(filecontent, format);
}

Image *getImage(const QPixmap &pixmap, QByteArray format) {
	return new Image(pixmap, format);
}

Image *getImage(const QByteArray &filecontent, QByteArray format, const QPixmap &pixmap) {
	return new Image(filecontent, format, pixmap);
}

Image *getImage(qint32 width, qint32 height) {
	return new DelayedStorageImage(width, height);
}

StorageImage *getImage(const StorageImageLocation &location, qint32 size) {
	StorageKey key(storageKey(location));
	StorageImages::const_iterator i = storageImages.constFind(key);
	if (i == storageImages.cend()) {
		i = storageImages.insert(key, new StorageImage(location, size));
	}
	return i.value();
}

StorageImage *getImage(const StorageImageLocation &location, const QByteArray &bytes) {
	StorageKey key(storageKey(location));
	StorageImages::const_iterator i = storageImages.constFind(key);
	if (i == storageImages.cend()) {
		QByteArray bytesArr(bytes);
		i = storageImages.insert(key, new StorageImage(location, bytesArr));
	} else if (!i.value()->loaded()) {
		QByteArray bytesArr(bytes);
		i.value()->setData(bytesArr);
		if (!location.isNull()) {
			Local::writeImage(key, StorageImageSaved(bytes));
		}
	}
	return i.value();
}

WebFileImage *getImage(const WebFileImageLocation &location, qint32 size) {
	auto key = storageKey(location);
	auto i = webFileImages.constFind(key);
	if (i == webFileImages.cend()) {
		i = webFileImages.insert(key, new WebFileImage(location, size));
	}
	return i.value();
}

} // namespace internal

ReadAccessEnabler::ReadAccessEnabler(const PsFileBookmark *bookmark)
    : _bookmark(bookmark)
    , _failed(_bookmark ? !_bookmark->enable() : false) {}

ReadAccessEnabler::ReadAccessEnabler(const QSharedPointer<PsFileBookmark> &bookmark)
    : _bookmark(bookmark.data())
    , _failed(_bookmark ? !_bookmark->enable() : false) {}

ReadAccessEnabler::~ReadAccessEnabler() {
	if (_bookmark && !_failed) _bookmark->disable();
}

FileLocation::FileLocation(const QString &name)
    : fname(name) {
	if (fname.isEmpty()) {
		size = 0;
	} else {
		setBookmark(psPathBookmark(name));

		QFileInfo f(name);
		if (f.exists()) {
			qint64 s = f.size();
			if (s > INT_MAX) {
				fname = QString();
				_bookmark.clear();
				size = 0;
			} else {
				modified = f.lastModified();
				size = qint32(s);
			}
		} else {
			fname = QString();
			_bookmark.clear();
			size = 0;
		}
	}
}

bool FileLocation::check() const {
	if (fname.isEmpty()) return false;

	ReadAccessEnabler enabler(_bookmark);
	if (enabler.failed()) {
		const_cast<FileLocation *>(this)->_bookmark.clear();
	}

	QFileInfo f(name());
	if (!f.isReadable()) return false;

	quint64 s = f.size();
	if (s > INT_MAX) {
		DEBUG_LOG(("File location check: Wrong size %1").arg(s));
		return false;
	}

	if (qint32(s) != size) {
		DEBUG_LOG(("File location check: Wrong size %1 when should be %2").arg(s).arg(size));
		return false;
	}
	auto realModified = f.lastModified();
	if (realModified != modified) {
		DEBUG_LOG(("File location check: Wrong last modified time %1 when should be %2")
		              .arg(realModified.toMSecsSinceEpoch())
		              .arg(modified.toMSecsSinceEpoch()));
		return false;
	}
	return true;
}

const QString &FileLocation::name() const {
	return _bookmark ? _bookmark->name(fname) : fname;
}

QByteArray FileLocation::bookmark() const {
	return _bookmark ? _bookmark->bookmark() : QByteArray();
}

void FileLocation::setBookmark(const QByteArray &bm) {
	_bookmark.reset(bm.isEmpty() ? nullptr : new PsFileBookmark(bm));
}

bool FileLocation::accessEnable() const {
	return isEmpty() ? false : (_bookmark ? _bookmark->enable() : true);
}

void FileLocation::accessDisable() const {
	return _bookmark ? _bookmark->disable() : (void)0;
}
