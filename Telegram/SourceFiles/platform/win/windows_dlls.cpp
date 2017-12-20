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
#include "stdafx.h"
#include "platform/win/windows_dlls.h"

namespace Platform {
namespace Dlls {

f_SetDllDirectory SetDllDirectory;

HINSTANCE LibKernel32;

void init() {
	static bool inited = false;
	if (inited) return;
	inited = true;

	LibKernel32 = LoadLibrary(L"KERNEL32.DLL");
	load(LibKernel32, "SetDllDirectoryW", SetDllDirectory);
	if (SetDllDirectory) {
		// Remove the current directory from the DLL search order.
		SetDllDirectory(L"");
	}
}

f_SetWindowTheme SetWindowTheme;
f_OpenAs_RunDLL OpenAs_RunDLL;
f_SHOpenWithDialog SHOpenWithDialog;
f_SHAssocEnumHandlers SHAssocEnumHandlers;
f_SHCreateItemFromParsingName SHCreateItemFromParsingName;
f_WTSRegisterSessionNotification WTSRegisterSessionNotification;
f_WTSUnRegisterSessionNotification WTSUnRegisterSessionNotification;
f_SHQueryUserNotificationState SHQueryUserNotificationState;
f_SHChangeNotify SHChangeNotify;
f_SetCurrentProcessExplicitAppUserModelID SetCurrentProcessExplicitAppUserModelID;
f_RoGetActivationFactory RoGetActivationFactory;
f_WindowsCreateStringReference WindowsCreateStringReference;
f_WindowsDeleteString WindowsDeleteString;
f_PropVariantToString PropVariantToString;
f_PSStringFromPropertyKey PSStringFromPropertyKey;

HINSTANCE LibUxTheme;
HINSTANCE LibShell32;
HINSTANCE LibWtsApi32;
HINSTANCE LibPropSys;
HINSTANCE LibComBase;

void start() {
	init();

	LibShell32 = LoadLibrary(L"SHELL32.DLL");
	load(LibShell32, "SHAssocEnumHandlers", SHAssocEnumHandlers);
	load(LibShell32, "SHCreateItemFromParsingName", SHCreateItemFromParsingName);
	load(LibShell32, "SHOpenWithDialog", SHOpenWithDialog);
	load(LibShell32, "OpenAs_RunDLLW", OpenAs_RunDLL);
	load(LibShell32, "SHQueryUserNotificationState", SHQueryUserNotificationState);
	load(LibShell32, "SHChangeNotify", SHChangeNotify);
	load(LibShell32, "SetCurrentProcessExplicitAppUserModelID", SetCurrentProcessExplicitAppUserModelID);

	if (cBetaVersion() == 10020001 && SHChangeNotify) { // Temp - app icon was changed
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	}

	LibUxTheme = LoadLibrary(L"UXTHEME.DLL");
	load(LibUxTheme, "SetWindowTheme", SetWindowTheme);

	auto version = QSysInfo::windowsVersion();
	if (version >= QSysInfo::WV_VISTA) {
		LibWtsApi32 = LoadLibrary(L"WTSAPI32.DLL");
		load(LibWtsApi32, "WTSRegisterSessionNotification", WTSRegisterSessionNotification);
		load(LibWtsApi32, "WTSUnRegisterSessionNotification", WTSUnRegisterSessionNotification);

		LibPropSys = LoadLibrary(L"PROPSYS.DLL");
		load(LibPropSys, "PropVariantToString", PropVariantToString);
		load(LibPropSys, "PSStringFromPropertyKey", PSStringFromPropertyKey);

		if (version >= QSysInfo::WV_WINDOWS8) {
			LibComBase = LoadLibrary(L"COMBASE.DLL");
			load(LibComBase, "RoGetActivationFactory", RoGetActivationFactory);
			load(LibComBase, "WindowsCreateStringReference", WindowsCreateStringReference);
			load(LibComBase, "WindowsDeleteString", WindowsDeleteString);
		}
	}
}

} // namespace Dlls
} // namespace Platform
