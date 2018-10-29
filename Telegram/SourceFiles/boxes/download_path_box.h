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

#include "base/observer.h"
#include "boxes/abstract_box.h"

namespace Ui {
template <typename Enum> class RadioenumGroup;
template <typename Enum> class Radioenum;
class LinkButton;
} // namespace Ui

class DownloadPathBox : public BoxContent {
	Q_OBJECT

public:
	DownloadPathBox(QWidget *parent);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;

private slots:
	void onEditPath();

private:
	enum class Directory {
		Downloads,
		Temp,
		Custom,
	};
	void radioChanged(Directory value);
	Directory typeFromPath(const QString &path) {
		if (path.isEmpty()) {
			return Directory::Downloads;
		} else if (path == qsl("tmp")) {
			return Directory::Temp;
		}
		return Directory::Custom;
	}

	void save();
	void updateControlsVisibility();
	void setPathText(const QString &text);

	QString _path;
	QByteArray _pathBookmark;

	std::shared_ptr<Ui::RadioenumGroup<Directory>> _group;
	object_ptr<Ui::Radioenum<Directory>> _default;
	object_ptr<Ui::Radioenum<Directory>> _temp;
	object_ptr<Ui::Radioenum<Directory>> _dir;
	object_ptr<Ui::LinkButton> _pathLink;
};
