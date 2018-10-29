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
#include "media/media_clip_implementation.h"
#include "ui/images.h"

namespace Media {
namespace Clip {
namespace internal {

void ReaderImplementation::initDevice() {
	if (_data->isEmpty()) {
		if (_file.isOpen()) _file.close();
		_file.setFileName(_location->name());
		_dataSize = _file.size();
	} else {
		if (_buffer.isOpen()) _buffer.close();
		_buffer.setBuffer(_data);
		_dataSize = _data->size();
	}
	_device = _data->isEmpty() ? static_cast<QIODevice*>(&_file) : static_cast<QIODevice*>(&_buffer);
}

} // namespace internal
} // namespace Clip
} // namespace Media
