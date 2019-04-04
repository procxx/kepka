#include "data/data_photo.h"
#include "messenger.h" // Messenger::
#include "app.h" // App::
#include "facades.h" // Notify::historyItemLayoutChanged
#include "history/history_media.h" // HistoryMedia definition
#include "history/history_media_types.h" // HistoryPhoto
#include "mainwidget.h" // MainWidget

PhotoData::PhotoData(const PhotoId &id, const quint64 &access, qint32 date, const ImagePtr &thumb,
                     const ImagePtr &medium, const ImagePtr &full)
    : id(id)
    , access(access)
    , date(date)
    , thumb(thumb)
    , medium(medium)
    , full(full) {}

void PhotoData::automaticLoad(const HistoryItem *item) {
	full->automaticLoad(item);
}

void PhotoData::automaticLoadSettingsChanged() {
	full->automaticLoadSettingsChanged();
}

void PhotoData::download() {
	full->loadEvenCancelled();
	notifyLayoutChanged();
}

bool PhotoData::loaded() const {
	bool wasLoading = loading();
	if (full->loaded()) {
		if (wasLoading) {
			notifyLayoutChanged();
		}
		return true;
	}
	return false;
}

bool PhotoData::loading() const {
	return full->loading();
}

bool PhotoData::displayLoading() const {
	return full->loading() ? full->displayLoading() : uploading();
}

void PhotoData::cancel() {
	full->cancel();
	notifyLayoutChanged();
}

void PhotoData::notifyLayoutChanged() const {
	auto &items = App::photoItems();
	auto i = items.constFind(const_cast<PhotoData *>(this));
	if (i != items.cend()) {
		for_const (auto item, i.value()) { Notify::historyItemLayoutChanged(item); }
	}
}

double PhotoData::progress() const {
	if (uploading()) {
		if (uploadingData->size > 0) {
			return double(uploadingData->offset) / uploadingData->size;
		}
		return 0;
	}
	return full->progress();
}

qint32 PhotoData::loadOffset() const {
	return full->loadOffset();
}

bool PhotoData::uploading() const {
	return !!uploadingData;
}

void PhotoData::forget() {
	thumb->forget();
	replyPreview->forget();
	medium->forget();
	full->forget();
}

ImagePtr PhotoData::makeReplyPreview() {
	if (replyPreview->isNull() && !thumb->isNull()) {
		if (thumb->loaded()) {
			int w = thumb->width(), h = thumb->height();
			if (w <= 0) w = 1;
			if (h <= 0) h = 1;
			replyPreview =
			    ImagePtr(w > h ? thumb->pix(w * st::msgReplyBarSize.height() / h, st::msgReplyBarSize.height()) :
			                     thumb->pix(st::msgReplyBarSize.height()),
			             "PNG");
		} else {
			thumb->load();
		}
	}
	return replyPreview;
}

void PhotoOpenClickHandler::onClickImpl() const {
	Messenger::Instance().showPhoto(this, App::hoveredLinkItem() ? App::hoveredLinkItem() : App::contextItem());
}

void PhotoSaveClickHandler::onClickImpl() const {
	auto data = photo();
	if (!data->date) return;

	data->download();
}

void PhotoCancelClickHandler::onClickImpl() const {
	auto data = photo();
	if (!data->date) return;

	if (data->uploading()) {
		if (auto item =
		        App::hoveredLinkItem() ? App::hoveredLinkItem() : (App::contextItem() ? App::contextItem() : nullptr)) {
			if (auto media = item->getMedia()) {
				if (media->type() == MediaTypePhoto && static_cast<HistoryPhoto *>(media)->photo() == data) {
					App::contextItem(item);
					App::main()->cancelUploadLayer();
				}
			}
		}
	} else {
		data->cancel();
	}
}
