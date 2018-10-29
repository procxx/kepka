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

#include "ui/widgets/input_fields.h"

class HistoryWidget;
namespace Window {
class Controller;
} // namespace Window

QString ConvertTagToMimeTag(const QString &tagId);

EntitiesInText ConvertTextTagsToEntities(const TextWithTags::Tags &tags);
TextWithTags::Tags ConvertEntitiesToTextTags(const EntitiesInText &entities);
std::unique_ptr<QMimeData> MimeDataFromTextWithEntities(const TextWithEntities &forClipboard);

class MessageField final : public Ui::FlatTextarea {
	Q_OBJECT

public:
	MessageField(QWidget *parent, not_null<Window::Controller *> controller, const style::FlatTextarea &st,
	             Fn<QString()> placeholderFactory = Fn<QString()>(), const QString &val = QString());

	bool hasSendText() const;

	void setInsertFromMimeDataHook(Fn<bool(const QMimeData *data)> hook) {
		_insertFromMimeDataHook = std::move(hook);
	}

public slots:
	void onEmojiInsert(EmojiPtr emoji);

signals:
	void focused();

protected:
	void focusInEvent(QFocusEvent *e) override;
	void dropEvent(QDropEvent *e) override;
	bool canInsertFromMimeData(const QMimeData *source) const override;
	void insertFromMimeData(const QMimeData *source) override;

private:
	not_null<Window::Controller *> _controller;
	Fn<bool(const QMimeData *data)> _insertFromMimeDataHook;
};
