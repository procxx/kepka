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
#include "history/history_location_manager.h"

#include "mainwidget.h"
#include "lang/lang_keys.h"
#include "platform/platform_specific.h"

namespace {

constexpr auto kCoordPrecision = 8;
constexpr auto kMaxHttpRedirects = 5;

} // namespace

QString LocationClickHandler::copyToClipboardContextItemText() const {
	return lang(lng_context_copy_link);
}

void LocationClickHandler::onClick(Qt::MouseButton button) const {
	if (!psLaunchMaps(_coords)) {
		QDesktopServices::openUrl(_text);
	}
}

void LocationClickHandler::setup() {
	auto latlon = _coords.latAsString() + ',' + _coords.lonAsString();
	_text = qsl("https://maps.google.com/maps?q=") + latlon + qsl("&ll=") + latlon + qsl("&z=16");
}

namespace {
LocationManager *locationManager = nullptr;
} // namespace

void initLocationManager() {
	if (!locationManager) {
		locationManager = new LocationManager();
		locationManager->init();
	}
}

void reinitLocationManager() {
	if (locationManager) {
		locationManager->reinit();
	}
}

void deinitLocationManager() {
	if (locationManager) {
		locationManager->deinit();
		delete locationManager;
		locationManager = nullptr;
	}
}

void LocationManager::init() {
	if (manager) delete manager;
	manager = new QNetworkAccessManager();
	App::setProxySettings(*manager);

	connect(manager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), this, SLOT(onFailed(QNetworkReply*)));
#ifndef OS_MAC_OLD
	connect(manager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)), this, SLOT(onFailed(QNetworkReply*)));
#endif // OS_MAC_OLD
	connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onFinished(QNetworkReply*)));

	if (notLoadedPlaceholder) {
		delete notLoadedPlaceholder->v();
		delete notLoadedPlaceholder;
	}
	auto data = QImage(cIntRetinaFactor(), cIntRetinaFactor(), QImage::Format_ARGB32_Premultiplied);
	data.fill(st::imageBgTransparent->c);
	data.setDevicePixelRatio(cRetinaFactor());
	notLoadedPlaceholder = new ImagePtr(App::pixmapFromImageInPlace(std::move(data)), "GIF");
}

void LocationManager::reinit() {
	if (manager) App::setProxySettings(*manager);
}

void LocationManager::deinit() {
	if (manager) {
		delete manager;
		manager = nullptr;
	}
	if (notLoadedPlaceholder) {
		delete notLoadedPlaceholder->v();
		delete notLoadedPlaceholder;
		notLoadedPlaceholder = nullptr;
	}
	dataLoadings.clear();
	imageLoadings.clear();
}

void LocationManager::getData(LocationData *data) {
	if (!manager) {
		DEBUG_LOG(("App Error: getting image link data without manager init!"));
		return failed(data);
	}

	int32_t w = st::locationSize.width(), h = st::locationSize.height();
	int32_t zoom = 13, scale = 1;
	if (cScale() == dbisTwo || cRetina()) {
		scale = 2;
	} else {
		w = convertScale(w);
		h = convertScale(h);
	}
	auto coords = data->coords.latAsString() + ',' + data->coords.lonAsString();
	QString url = qsl("https://maps.googleapis.com/maps/api/staticmap?center=") + coords + qsl("&zoom=%1&size=%2x%3&maptype=roadmap&scale=%4&markers=color:red|size:big|").arg(zoom).arg(w).arg(h).arg(scale) + coords + qsl("&sensor=false");
	QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(url)));
	imageLoadings[reply] = data;
}

void LocationManager::onFinished(QNetworkReply *reply) {
	if (!manager) return;
	if (reply->error() != QNetworkReply::NoError) return onFailed(reply);

	QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	if (statusCode.isValid()) {
		int status = statusCode.toInt();
		if (status == 301 || status == 302) {
			QString loc = reply->header(QNetworkRequest::LocationHeader).toString();
			if (!loc.isEmpty()) {
				QMap<QNetworkReply*, LocationData*>::iterator i = dataLoadings.find(reply);
				if (i != dataLoadings.cend()) {
					LocationData *d = i.value();
					if (serverRedirects.constFind(d) == serverRedirects.cend()) {
						serverRedirects.insert(d, 1);
					} else if (++serverRedirects[d] > kMaxHttpRedirects) {
						DEBUG_LOG(("Network Error: Too many HTTP redirects in onFinished() for image link: %1").arg(loc));
						return onFailed(reply);
					}
					dataLoadings.erase(i);
					dataLoadings.insert(manager->get(QNetworkRequest(loc)), d);
					return;
				} else if ((i = imageLoadings.find(reply)) != imageLoadings.cend()) {
					LocationData *d = i.value();
					if (serverRedirects.constFind(d) == serverRedirects.cend()) {
						serverRedirects.insert(d, 1);
					} else if (++serverRedirects[d] > kMaxHttpRedirects) {
						DEBUG_LOG(("Network Error: Too many HTTP redirects in onFinished() for image link: %1").arg(loc));
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

	LocationData *d = 0;
	QMap<QNetworkReply*, LocationData*>::iterator i = dataLoadings.find(reply);
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

		if (App::main()) App::main()->update();
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
				if (format.isEmpty()) format = QByteArray("JPG");
			}
			d->loading = false;
			d->thumb = thumb.isNull() ? (*notLoadedPlaceholder) : ImagePtr(thumb, format);
			serverRedirects.remove(d);
			if (App::main()) App::main()->update();
		}
	}
}

void LocationManager::onFailed(QNetworkReply *reply) {
	if (!manager) return;

	LocationData *d = 0;
	QMap<QNetworkReply*, LocationData*>::iterator i = dataLoadings.find(reply);
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
	DEBUG_LOG(("Network Error: failed to get data for image link %1,%2 error %3").arg(d ? d->coords.latAsString() : QString()).arg(d ? d->coords.lonAsString() : QString()).arg(reply->errorString()));
	if (d) {
		failed(d);
	}
}

void LocationManager::failed(LocationData *data) {
	data->loading = false;
	data->thumb = *notLoadedPlaceholder;
	serverRedirects.remove(data);
}

void LocationData::load() {
	if (!thumb->isNull()) return thumb->load(false, false);
	if (loading) return;

	loading = true;
	if (locationManager) {
		locationManager->getData(this);
	}
}
