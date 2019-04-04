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
#include <qglobal.h> // qint64, quint64
#include "core/click_handler.h" // LeftButtonClickHandler
#include "ui/images.h" // ImagePtr

using PhotoId = quint64;

class PhotoData {
public:
	PhotoData(const PhotoId &id, const quint64 &access = 0, qint32 date = 0, const ImagePtr &thumb = ImagePtr(),
	          const ImagePtr &medium = ImagePtr(), const ImagePtr &full = ImagePtr());

	void automaticLoad(const HistoryItem *item);
	void automaticLoadSettingsChanged();

	void download();
	bool loaded() const;
	bool loading() const;
	bool displayLoading() const;
	void cancel();
	double progress() const;
	qint32 loadOffset() const;
	bool uploading() const;

	void forget();
	ImagePtr makeReplyPreview();

	PhotoId id;
	quint64 access;
	qint32 date;
	ImagePtr thumb, replyPreview;
	ImagePtr medium;
	ImagePtr full;

	PeerData *peer = nullptr; // for chat and channel photos connection
	// geo, caption

	struct UploadingData {
		UploadingData(int size)
		    : size(size) {}
		int offset = 0;
		int size = 0;
	};
	std::unique_ptr<UploadingData> uploadingData;

private:
	void notifyLayoutChanged() const;
};

class PhotoClickHandler : public LeftButtonClickHandler {
public:
	PhotoClickHandler(not_null<PhotoData *> photo, PeerData *peer = nullptr)
	    : _photo(photo)
	    , _peer(peer) {}
	not_null<PhotoData *> photo() const {
		return _photo;
	}
	PeerData *peer() const {
		return _peer;
	}

private:
	not_null<PhotoData *> _photo;
	PeerData *_peer;
};

class PhotoOpenClickHandler : public PhotoClickHandler {
public:
	using PhotoClickHandler::PhotoClickHandler;

protected:
	void onClickImpl() const override;
};

class PhotoSaveClickHandler : public PhotoClickHandler {
public:
	using PhotoClickHandler::PhotoClickHandler;

protected:
	void onClickImpl() const override;
};

class PhotoCancelClickHandler : public PhotoClickHandler {
public:
	using PhotoClickHandler::PhotoClickHandler;

protected:
	void onClickImpl() const override;
};
