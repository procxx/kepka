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
#include "mtproto/type_utils.h"

const MTPReplyMarkup MTPnullMarkup = MTP_replyKeyboardMarkup(MTP_flags(0), MTP_vector<MTPKeyboardButtonRow>(0));
const MTPVector<MTPMessageEntity> MTPnullEntities = MTP_vector<MTPMessageEntity>(0);
const MTPMessageFwdHeader MTPnullFwdHeader =
    MTP_messageFwdHeader(MTP_flags(0), MTPint(), MTPint(), MTPint(), MTPint(), MTPstring());
