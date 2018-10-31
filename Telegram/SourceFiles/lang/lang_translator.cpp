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
#include "lang/lang_translator.h"

#include "lang/lang_keys.h"

namespace Lang {

QString Translator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const {
	if (qstr("QMenuBar") == context) {
		if (qstr("Services") == sourceText) return lang(lng_mac_menu_services);
		if (qstr("Hide %1") == sourceText) return lng_mac_menu_hide_telegram(lt_telegram, qsl("%1"));
		if (qstr("Hide Others") == sourceText) return lang(lng_mac_menu_hide_others);
		if (qstr("Show All") == sourceText) return lang(lng_mac_menu_show_all);
		if (qstr("Preferences...") == sourceText) return lang(lng_mac_menu_preferences);
		if (qstr("Quit %1") == sourceText) return lng_mac_menu_quit_telegram(lt_telegram, qsl("%1"));
		if (qstr("About %1") == sourceText) return lng_mac_menu_about_telegram(lt_telegram, qsl("%1"));
		return QString();
	}
	if (qstr("QWidgetTextControl") == context || qstr("QLineEdit") == context) {
		if (qstr("&Undo") == sourceText)
			return lang(
			    (cPlatform() == dbipWindows) ?
			        lng_wnd_menu_undo :
			        ((cPlatform() == dbipMac || cPlatform() == dbipMacOld) ? lng_mac_menu_undo : lng_linux_menu_undo));
		if (qstr("&Redo") == sourceText)
			return lang(
			    (cPlatform() == dbipWindows) ?
			        lng_wnd_menu_redo :
			        ((cPlatform() == dbipMac || cPlatform() == dbipMacOld) ? lng_mac_menu_redo : lng_linux_menu_redo));
		if (qstr("Cu&t") == sourceText) return lang(lng_mac_menu_cut);
		if (qstr("&Copy") == sourceText) return lang(lng_mac_menu_copy);
		if (qstr("&Paste") == sourceText) return lang(lng_mac_menu_paste);
		if (qstr("Delete") == sourceText) return lang(lng_mac_menu_delete);
		if (qstr("Select All") == sourceText) return lang(lng_mac_menu_select_all);
		return QString();
	}
	if (qstr("QUnicodeControlCharacterMenu") == context) {
		if (qstr("Insert Unicode control character") == sourceText) return lang(lng_menu_insert_unicode);
		return QString();
	}
	return QString();
}

} // namespace Lang
