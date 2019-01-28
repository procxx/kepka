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
#include "history/history_location_manager.h"
#include <QBuffer>
#include <QDesktopServices>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonParseError>

#include "lang/lang_keys.h"
#include "mainwidget.h"
#include "platform/platform_specific.h"

namespace {

constexpr auto kMaxHttpRedirects = 5;

} // namespace

/// @brief Helper interface for getting location map tile and URL.
///        Used instead of hardcoded URLs.
class ILocationMapTileHelper {
public:
	virtual QString locationUrl(const LocationCoords &loc) = 0;
	/// @brief Returns image tile URL for requested location loc.
	/// @param width  Tile width, in px.
	/// @param height Tile height, in px.
	/// @param zoom   The world map zoom. Usually varies from 1 to 16.
	/// @param scale  The map objects' scale for adopting map objects and
	///               labels for HiDPI / Retina displays.
	virtual QString locationTileImageUrl(const LocationCoords &loc, int width, int height, int zoom, int scale) = 0;
	virtual ~ILocationMapTileHelper();
};

// Note: this dtor has been extracted to avoid the inlining and triggering a
// warning related to Weak vtables. If this dtor will be inside the class
// definition, then compiler will have to place multiple copies of vtables
// which could increase binary size and could make ABI clashes.
ILocationMapTileHelper::~ILocationMapTileHelper() = default;

/// @brief Yandex.Maps tile helper. Allows to use the Yandex.Maps as backend
///        for tile images and location URLs.
class YandexMapsLocationTileHelper : public ILocationMapTileHelper {
public:
	QString locationUrl(const LocationCoords &loc) override;

	/// @param zoom World map zoom (from 0 to 17)
	/// @see   https://tech.yandex.ru/maps/doc/staticapi/1.x/dg/concepts/map_scale_docpage/
	QString locationTileImageUrl(const LocationCoords &loc, int width, int height, int zoom, int scale) override;
};

QString YandexMapsLocationTileHelper::locationUrl(const LocationCoords &loc) {
	// Yandex.Maps accepts ll string in "longitude,latitude" format
	auto latlon = loc.lonAsString() + "%2C" + loc.latAsString();
	return qsl("https://maps.yandex.ru/?ll=") + latlon + qsl("&z=16");
}

QString YandexMapsLocationTileHelper::locationTileImageUrl(const LocationCoords &loc, int width, int height, int zoom,
                                                           int scale) {
	// Map marker and API endpoint constants.
	// See https://tech.yandex.ru/maps/doc/staticapi/1.x/dg/concepts/input_params-docpage/
	// for API parameters reference.
	const char *mapsApiUrl = "https://static-maps.yandex.ru/1.x/?ll=";
	const char *mapsMarkerParams = ",pm2rdl"; // red large marker looking like "9"
	// Tile image parameters format string
	QString mapsApiParams = "&z=%1&size=%2,%3&l=map&scale=%4&pt=";

	// Yandex.Maps accepts ll string in "longitude,latitude" format
	auto coords = loc.lonAsString() + ',' + loc.latAsString();

	QString url =
	    mapsApiUrl + coords + mapsApiParams.arg(zoom).arg(width).arg(height).arg(scale) + coords + mapsMarkerParams;

	return url;
}

/// @brief Uses Google Maps Static API. Adopted from old upstream code.
class GoogleMapsLocationTileHelper : public ILocationMapTileHelper {
public:
	QString locationUrl(const LocationCoords &loc) override;
	QString locationTileImageUrl(const LocationCoords &loc, int width, int height, int zoom, int scale) override;
};

QString GoogleMapsLocationTileHelper::locationUrl(const LocationCoords &loc) {
	auto latlon = loc.latAsString() + ',' + loc.lonAsString();
	return qsl("https://maps.google.com/maps?q=") + latlon + qsl("&ll=") + latlon + qsl("&z=16");
}

QString GoogleMapsLocationTileHelper::locationTileImageUrl(const LocationCoords &loc, int width, int height, int zoom,
                                                           int scale) {
	// Map marker, API options and endpoint constants.
	const char *mapsApiUrl = "https://maps.googleapis.com/maps/api/staticmap?center=";
	// additional marker params
	const char *mapsMarkerParams = "&sensor=false";
	// API format string with basic marker params (red and big)
	QString mapsApiParams = "&zoom=%1&size=%2,%3&maptype=roadmap&scale=%4&markers=color:red|size:big|";

	// Google uses lat,lon in query URLs
	auto coords = loc.latAsString() + ',' + loc.lonAsString();

	QString url =
	    mapsApiUrl + coords + mapsApiParams.arg(zoom).arg(width).arg(height).arg(scale) + coords + mapsMarkerParams;
	return url;
}

// This option could be enabled in core CMakeLists.txt
#ifdef KEPKA_USE_YANDEX_MAPS
using LocationMapTileHelper = YandexMapsLocationTileHelper;
#else
using LocationMapTileHelper = GoogleMapsLocationTileHelper;
#endif
//
// Static variables
//

namespace {
LocationManager *locationManager = nullptr;
ILocationMapTileHelper *locationMapTileHelper = nullptr;
} // namespace

//
// LocationClickHandler routines
//

QString LocationClickHandler::copyToClipboardContextItemText() const {
	return lang(lng_context_copy_link);
}

void LocationClickHandler::onClick([[maybe_unused]] Qt::MouseButton button) const {
	if (!psLaunchMaps(_coords)) {
		QDesktopServices::openUrl(_text);
	}
}

void LocationClickHandler::setup() {
	_text = locationMapTileHelper->locationUrl(_coords);
}

void initLocationManager() {
	if (locationManager == nullptr) {
		locationManager = new LocationManager();
		locationManager->init();
	}
	if (locationMapTileHelper == nullptr) {
		locationMapTileHelper = new LocationMapTileHelper();
	}
}

void reinitLocationManager() {
	if (locationManager != nullptr) {
		locationManager->reinit();
	}
}

void deinitLocationManager() {
	if (locationManager != nullptr) {
		locationManager->deinit();
		delete locationManager;
		locationManager = nullptr;
	}
	// if (ptr) is useless, because delete nullptr is valid.
	delete locationMapTileHelper;
	locationMapTileHelper = nullptr;
}

void LocationManager::init() {
	delete manager;
	manager = new QNetworkAccessManager();
	App::setProxySettings(*manager);

	connect(manager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)), this,
	        SLOT(onFailed(QNetworkReply *)));
#ifndef OS_MAC_OLD
	connect(manager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)), this,
	        SLOT(onFailed(QNetworkReply *)));
#endif // OS_MAC_OLD
	connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(onFinished(QNetworkReply *)));

	if (notLoadedPlaceholder != nullptr) {
		delete notLoadedPlaceholder->v();
		delete notLoadedPlaceholder;
	}
	auto data = QImage(cIntRetinaFactor(), cIntRetinaFactor(), QImage::Format_ARGB32_Premultiplied);
	data.fill(st::imageBgTransparent->c);
	data.setDevicePixelRatio(cRetinaFactor());
	notLoadedPlaceholder = new ImagePtr(App::pixmapFromImageInPlace(std::move(data)), "GIF");
}

void LocationManager::reinit() {
	if (manager != nullptr) {
		App::setProxySettings(*manager);
	}
}

void LocationManager::deinit() {
	if (manager != nullptr) {
		delete manager;
		manager = nullptr;
	}
	if (notLoadedPlaceholder != nullptr) {
		delete notLoadedPlaceholder->v();
		delete notLoadedPlaceholder;
		notLoadedPlaceholder = nullptr;
	}
	dataLoadings.clear();
	imageLoadings.clear();
}

void LocationManager::getData(LocationData *data) {
	if (manager == nullptr) {
		DEBUG_LOG(("App Error: getting image link data without manager init!"));
		return failed(data);
	}

	qint32 w = st::locationSize.width(), h = st::locationSize.height();
	qint32 zoom = 13, scale = 1;
	if (cScale() == dbisTwo || cRetina()) {
		scale = 2;
	} else {
		w = convertScale(w);
		h = convertScale(h);
	}

	QString url = locationMapTileHelper->locationTileImageUrl(data->coords, w, h, zoom, scale);
	QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(url)));
	imageLoadings[reply] = data;
}

void LocationManager::onFinished(QNetworkReply *reply) {
	if (manager == nullptr) {
		return;
	}
	if (reply->error() != QNetworkReply::NoError) {
		return onFailed(reply);
	}

	QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	if (statusCode.isValid()) {
		int status = statusCode.toInt();
		if (status == 301 || status == 302) {
			QString loc = reply->header(QNetworkRequest::LocationHeader).toString();
			if (!loc.isEmpty()) {
				QMap<QNetworkReply *, LocationData *>::iterator i = dataLoadings.find(reply);
				if (i != dataLoadings.cend()) {
					LocationData *d = i.value();
					if (serverRedirects.constFind(d) == serverRedirects.cend()) {
						serverRedirects.insert(d, 1);
					} else if (++serverRedirects[d] > kMaxHttpRedirects) {
						DEBUG_LOG(
						    ("Network Error: Too many HTTP redirects in onFinished() for image link: %1").arg(loc));
						return onFailed(reply);
					}
					dataLoadings.erase(i);
					dataLoadings.insert(manager->get(QNetworkRequest(loc)), d);
					return;
				}
				if ((i = imageLoadings.find(reply)) != imageLoadings.cend()) {
					LocationData *d = i.value();
					if (serverRedirects.constFind(d) == serverRedirects.cend()) {
						serverRedirects.insert(d, 1);
					} else if (++serverRedirects[d] > kMaxHttpRedirects) {
						DEBUG_LOG(
						    ("Network Error: Too many HTTP redirects in onFinished() for image link: %1").arg(loc));
						return onFailed(reply);
					}
					imageLoadings.erase(i);
					imageLoadings.insert(manager->get(QNetworkRequest(loc)), d);
					return;
				}
			}
		}
		if (status != 200) {
			DEBUG_LOG(("Network Error: Bad HTTP status received in onFinished() for image link: %1").arg(status));
			return onFailed(reply);
		}
	}

	LocationData *d = nullptr;
	QMap<QNetworkReply *, LocationData *>::iterator i = dataLoadings.find(reply);
	if (i != dataLoadings.cend()) {
		d = i.value();
		dataLoadings.erase(i);

		QJsonParseError e;
		QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &e);
		if (e.error != QJsonParseError::NoError) {
			DEBUG_LOG(("JSON Error: Bad json received in onFinished() for image link"));
			return onFailed(reply);
		}
		failed(d);

		if (App::main() != nullptr) {
			App::main()->update();
		}
	} else {
		i = imageLoadings.find(reply);
		if (i != imageLoadings.cend()) {
			d = i.value();
			imageLoadings.erase(i);

			QPixmap thumb;
			QByteArray format;
			QByteArray data(reply->readAll());
			{
				QBuffer buffer(&data);
				QImageReader reader(&buffer);
#ifndef OS_MAC_OLD
				reader.setAutoTransform(true);
#endif // OS_MAC_OLD
				thumb = QPixmap::fromImageReader(&reader, Qt::ColorOnly);
				format = reader.format();
				thumb.setDevicePixelRatio(cRetinaFactor());
				if (format.isEmpty()) {
					format = QByteArray("JPG");
				}
			}
			d->loading = false;
			d->thumb = thumb.isNull() ? (*notLoadedPlaceholder) : ImagePtr(thumb, format);
			serverRedirects.remove(d);
			if (App::main() != nullptr) {
				App::main()->update();
			}
		}
	}
}

void LocationManager::onFailed(QNetworkReply *reply) {
	if (manager == nullptr) {
		return;
	}

	LocationData *d = nullptr;
	QMap<QNetworkReply *, LocationData *>::iterator i = dataLoadings.find(reply);
	if (i != dataLoadings.cend()) {
		d = i.value();
		dataLoadings.erase(i);
	} else {
		i = imageLoadings.find(reply);
		if (i != imageLoadings.cend()) {
			d = i.value();
			imageLoadings.erase(i);
		}
	}
	DEBUG_LOG(("Network Error: failed to get data for image link %1,%2 error %3")
	              .arg(d ? d->coords.latAsString() : QString())
	              .arg(d ? d->coords.lonAsString() : QString())
	              .arg(reply->errorString()));
	if (d != nullptr) {
		failed(d);
	}
}

void LocationManager::failed(LocationData *data) {
	data->loading = false;
	data->thumb = *notLoadedPlaceholder;
	serverRedirects.remove(data);
}

void LocationData::load() {
	if (!thumb->isNull()) {
		return thumb->load(false, false);
	}
	if (loading) {
		return;
	}

	loading = true;
	if (locationManager != nullptr) {
		locationManager->getData(this);
	}
}
