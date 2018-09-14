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
#include <windows.h>
#include <windowsx.h>
#include <ShlGuid.h>

#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>

#include "platform/win/file_utilities_win.h"

#include "app.h"
#include "lang/lang_keys.h"
#include "mainwindow.h"
#include "messenger.h"
#include "platform/win/windows_dlls.h"
#include "storage/localstorage.h"
#include <Shlwapi.h>
#include <winerror.h>


HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &, int hbitmapFormat);

namespace Platform {
namespace File {
namespace {

class OpenWithApp {
public:
	OpenWithApp(const QString &name, IAssocHandler *handler, HBITMAP icon = nullptr)
	    : _name(name)
	    , _handler(handler)
	    , _icon(icon) {}
	OpenWithApp(OpenWithApp &&other)
	    : _name(base::take(other._name))
	    , _handler(base::take(other._handler))
	    , _icon(base::take(other._icon)) {}
	OpenWithApp &operator=(OpenWithApp &&other) {
		_name = base::take(other._name);
		_icon = base::take(other._icon);
		_handler = base::take(other._handler);
		return (*this);
	}

	OpenWithApp(const OpenWithApp &other) = delete;
	OpenWithApp &operator=(const OpenWithApp &other) = delete;

	~OpenWithApp() {
		if (_icon) {
			DeleteBitmap(_icon);
		}
		if (_handler) {
			_handler->Release();
		}
	}

	const QString &name() const {
		return _name;
	}
	HBITMAP icon() const {
		return _icon;
	}
	IAssocHandler *handler() const {
		return _handler;
	}

private:
	QString _name;
	IAssocHandler *_handler = nullptr;
	HBITMAP _icon = nullptr;
};

HBITMAP IconToBitmap(LPWSTR icon, int iconindex) {
	if (!icon) return 0;
	WCHAR tmpIcon[4096];
	if (icon[0] == L'@' && SUCCEEDED(SHLoadIndirectString(icon, tmpIcon, 4096, 0))) {
		icon = tmpIcon;
	}
	qint32 w = GetSystemMetrics(SM_CXSMICON), h = GetSystemMetrics(SM_CYSMICON);

	HICON ico = ExtractIcon(0, icon, iconindex);
	if (!ico) {
		if (!iconindex) { // try to read image
			QImage img(QString::fromWCharArray(icon));
			if (!img.isNull()) {
				return qt_pixmapToWinHBITMAP(
				    App::pixmapFromImageInPlace(img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)),
				    /* HBitmapAlpha */ 2);
			}
		}
		return 0;
	}

	HDC screenDC = GetDC(0), hdc = CreateCompatibleDC(screenDC);
	HBITMAP result = CreateCompatibleBitmap(screenDC, w, h);
	HGDIOBJ was = SelectObject(hdc, result);
	DrawIconEx(hdc, 0, 0, ico, w, h, 0, NULL, DI_NORMAL);
	SelectObject(hdc, was);
	DeleteDC(hdc);
	ReleaseDC(0, screenDC);

	DestroyIcon(ico);

	return (HBITMAP)CopyImage(result, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION);
}

} // namespace

void UnsafeOpenEmailLink(const QString &email) {
	auto url = QUrl(qstr("mailto:") + email);
	if (!QDesktopServices::openUrl(url)) {
		auto wstringUrl = url.toString(QUrl::FullyEncoded).toStdWString();
		if (Dlls::SHOpenWithDialog) {
			OPENASINFO info;
			info.oaifInFlags =
			    OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT | OAIF_EXEC | OAIF_FILE_IS_URI | OAIF_URL_PROTOCOL;
			info.pcszClass = NULL;
			info.pcszFile = wstringUrl.c_str();
			Dlls::SHOpenWithDialog(0, &info);
		} else if (Dlls::OpenAs_RunDLL) {
			Dlls::OpenAs_RunDLL(0, 0, wstringUrl.c_str(), SW_SHOWNORMAL);
		} else {
			ShellExecute(0, L"open", wstringUrl.c_str(), 0, 0, SW_SHOWNORMAL);
		}
	}
}

bool UnsafeShowOpenWithDropdown(const QString &filepath, QPoint menuPosition) {
	if (!Dlls::SHAssocEnumHandlers || !Dlls::SHCreateItemFromParsingName) {
		return false;
	}

	auto window = App::wnd();
	if (!window) {
		return false;
	}

	auto parentHWND = window->psHwnd();
	auto wstringPath = QDir::toNativeSeparators(filepath).toStdWString();

	auto result = false;
	std::vector<OpenWithApp> handlers;
	IShellItem *pItem = nullptr;
	if (SUCCEEDED(Dlls::SHCreateItemFromParsingName(wstringPath.c_str(), nullptr, IID_PPV_ARGS(&pItem)))) {
		IEnumAssocHandlers *assocHandlers = nullptr;
		if (SUCCEEDED(pItem->BindToHandler(nullptr, BHID_EnumAssocHandlers, IID_PPV_ARGS(&assocHandlers)))) {
			HRESULT hr = S_FALSE;
			do {
				IAssocHandler *handler = nullptr;
				ULONG ulFetched = 0;
				hr = assocHandlers->Next(1, &handler, &ulFetched);
				if (FAILED(hr) || hr == S_FALSE || !ulFetched) break;

				LPWSTR name = 0;
				if (SUCCEEDED(handler->GetUIName(&name))) {
					LPWSTR icon = 0;
					int iconindex = 0;
					if (SUCCEEDED(handler->GetIconLocation(&icon, &iconindex)) && icon) {
						handlers.push_back(
						    OpenWithApp(QString::fromWCharArray(name), handler, IconToBitmap(icon, iconindex)));
						CoTaskMemFree(icon);
					} else {
						handlers.push_back(OpenWithApp(QString::fromWCharArray(name), handler));
					}
					CoTaskMemFree(name);
				} else {
					handler->Release();
				}
			} while (hr != S_FALSE);
			assocHandlers->Release();
		}

		if (!handlers.empty()) {
			HMENU menu = CreatePopupMenu();
			std::sort(handlers.begin(), handlers.end(),
			          [](const OpenWithApp &a, const OpenWithApp &b) { return a.name() < b.name(); });
			for (qint32 i = 0, l = handlers.size(); i < l; ++i) {
				MENUITEMINFO menuInfo = {0};
				menuInfo.cbSize = sizeof(menuInfo);
				menuInfo.fMask = MIIM_STRING | MIIM_DATA | MIIM_ID;
				menuInfo.fType = MFT_STRING;
				menuInfo.wID = i + 1;
				if (auto icon = handlers[i].icon()) {
					menuInfo.fMask |= MIIM_BITMAP;
					menuInfo.hbmpItem = icon;
				}

				auto name = handlers[i].name();
				if (name.size() > 512) name = name.mid(0, 512);
				WCHAR nameArr[1024];
				name.toWCharArray(nameArr);
				nameArr[name.size()] = 0;
				menuInfo.dwTypeData = nameArr;
				InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &menuInfo);
			}
			MENUITEMINFO sepInfo = {0};
			sepInfo.cbSize = sizeof(sepInfo);
			sepInfo.fMask = MIIM_STRING | MIIM_DATA;
			sepInfo.fType = MFT_SEPARATOR;
			InsertMenuItem(menu, GetMenuItemCount(menu), true, &sepInfo);

			MENUITEMINFO menuInfo = {0};
			menuInfo.cbSize = sizeof(menuInfo);
			menuInfo.fMask = MIIM_STRING | MIIM_DATA | MIIM_ID;
			menuInfo.fType = MFT_STRING;
			menuInfo.wID = handlers.size() + 1;

			QString name = lang(lng_wnd_choose_program_menu);
			if (name.size() > 512) name = name.mid(0, 512);
			WCHAR nameArr[1024];
			name.toWCharArray(nameArr);
			nameArr[name.size()] = 0;
			menuInfo.dwTypeData = nameArr;
			InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &menuInfo);

			int sel = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
			                         menuPosition.x(), menuPosition.y(), 0, parentHWND, 0);
			DestroyMenu(menu);

			if (sel > 0) {
				if (sel <= handlers.size()) {
					IDataObject *dataObj = 0;
					if (SUCCEEDED(pItem->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&dataObj))) && dataObj) {
						handlers[sel - 1].handler()->Invoke(dataObj);
						dataObj->Release();
						result = true;
					}
				}
			} else {
				result = true;
			}
		}

		pItem->Release();
	}
	return result;
}

bool UnsafeShowOpenWith(const QString &filepath) {
	auto wstringPath = QDir::toNativeSeparators(filepath).toStdWString();
	if (Dlls::SHOpenWithDialog) {
		OPENASINFO info;
		info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT | OAIF_EXEC;
		info.pcszClass = NULL;
		info.pcszFile = wstringPath.c_str();
		Dlls::SHOpenWithDialog(0, &info);
		return true;
	} else if (Dlls::OpenAs_RunDLL) {
		Dlls::OpenAs_RunDLL(0, 0, wstringPath.c_str(), SW_SHOWNORMAL);
		return true;
	}
	return false;
}

void UnsafeLaunch(const QString &filepath) {
	auto wstringPath = QDir::toNativeSeparators(filepath).toStdWString();
	ShellExecute(0, L"open", wstringPath.c_str(), 0, 0, SW_SHOWNORMAL);
}

void UnsafeShowInFolder(const QString &filepath) {
	auto nativePath = QDir::toNativeSeparators(filepath);
	auto wstringPath = nativePath.toStdWString();
	if (auto pidl = ILCreateFromPathW(wstringPath.c_str())) {
		SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
		ILFree(pidl);
	} else {
		auto pathEscaped = nativePath.replace('"', qsl("\"\""));
		auto wstringParam = (qstr("/select,") + pathEscaped).toStdWString();
		ShellExecute(0, 0, L"explorer", wstringParam.c_str(), 0, SW_SHOWNORMAL);
	}
}

void PostprocessDownloaded(const QString &filepath) {
	auto wstringZoneFile = QDir::toNativeSeparators(filepath).toStdWString() + L":Zone.Identifier";
	auto f = CreateFile(wstringZoneFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                    0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (f == INVALID_HANDLE_VALUE) { // :(
		return;
	}

	const char data[] = "[ZoneTransfer]\r\nZoneId=3\r\n";

	DWORD written = 0;
	BOOL result = WriteFile(f, data, sizeof(data), &written, NULL);
	CloseHandle(f);

	if (!result || written != sizeof(data)) { // :(
		return;
	}
}

} // namespace File

namespace FileDialog {
namespace {

using Type = ::FileDialog::internal::Type;

} // namespace

void InitLastPath() {
	// hack to restore previous dir without hurting performance
	QSettings settings(QSettings::UserScope, qstr("QtProject"));
	settings.beginGroup(qstr("Qt"));
	QByteArray sd = settings.value(qstr("filedialog")).toByteArray();
	QDataStream stream(&sd, QIODevice::ReadOnly);
	if (!stream.atEnd()) {
		int version = 3, _QFileDialogMagic = 190;
		QByteArray splitterState;
		QByteArray headerData;
		QList<QUrl> bookmarks;
		QStringList history;
		QString currentDirectory;
		qint32 marker;
		qint32 v;
		qint32 viewMode;
		stream >> marker;
		stream >> v;
		if (marker == _QFileDialogMagic && v == version) {
			stream >> splitterState >> bookmarks >> history >> currentDirectory >> headerData >> viewMode;
			cSetDialogLastPath(currentDirectory);
		}
	}

	if (cDialogHelperPath().isEmpty()) {
		QDir temppath(cWorkingDir() + "tdata/tdummy/");
		if (!temppath.exists()) {
			temppath.mkpath(temppath.absolutePath());
		}
		if (temppath.exists()) {
			cSetDialogHelperPath(temppath.absolutePath());
		}
	}
}

bool Get(QStringList &files, QByteArray &remoteContent, const QString &caption, const QString &filter,
         ::FileDialog::internal::Type type, QString startFile) {
	if (cDialogLastPath().isEmpty()) {
		Platform::FileDialog::InitLastPath();
	}

	// A hack for fast dialog create. There was some huge performance problem
	// if we open a file dialog in some folder with a large amount of files.
	// Some internal Qt watcher iterated over all of them, querying some information
	// that forced file icon and maybe other properties being resolved and this was
	// a blocking operation.
	auto helperPath = cDialogHelperPathFinal();
	auto parent = Messenger::Instance().getFileDialogParent();
	QFileDialog dialog(parent, caption, helperPath, filter);

	dialog.setModal(true);
	if (type == Type::ReadFile || type == Type::ReadFiles) {
		dialog.setFileMode((type == Type::ReadFiles) ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
		dialog.setAcceptMode(QFileDialog::AcceptOpen);
	} else if (type == Type::ReadFolder) { // save dir
		dialog.setAcceptMode(QFileDialog::AcceptOpen);

		// We use "obsolete" value ::DirectoryOnly instead of ::Directory + ::ShowDirsOnly
		// because in Windows XP native dialog this one works, while the "preferred" one
		// shows a native file choose dialog where you can't choose a directory, only open one.
		dialog.setFileMode(QFileDialog::DirectoryOnly);
		dialog.setOption(QFileDialog::ShowDirsOnly);
	} else { // save file
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setAcceptMode(QFileDialog::AcceptSave);
	}
	dialog.show();

	auto realLastPath = ([startFile] {
		// If we're given some non empty path containing a folder - use it.
		if (!startFile.isEmpty() && (startFile.indexOf('/') >= 0 || startFile.indexOf('\\') >= 0)) {
			return QFileInfo(startFile).dir().absolutePath();
		}
		return cDialogLastPath();
	})();
	if (realLastPath.isEmpty() || realLastPath.endsWith(qstr("/tdummy"))) {
		realLastPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
	}
	dialog.setDirectory(realLastPath);

	if (type == Type::WriteFile) {
		auto toSelect = startFile;
		auto lastSlash = toSelect.lastIndexOf('/');
		if (lastSlash >= 0) {
			toSelect = toSelect.mid(lastSlash + 1);
		}
		auto lastBackSlash = toSelect.lastIndexOf('\\');
		if (lastBackSlash >= 0) {
			toSelect = toSelect.mid(lastBackSlash + 1);
		}
		dialog.selectFile(toSelect);
	}

	int res = dialog.exec();

	if (type != Type::ReadFolder) {
		// Save last used directory for all queries except directory choosing.
		auto path = dialog.directory().absolutePath();
		if (path != cDialogLastPath()) {
			cSetDialogLastPath(path);
			Local::writeUserSettings();
		}
	}

	if (res == QDialog::Accepted) {
		if (type == Type::ReadFiles) {
			files = dialog.selectedFiles();
		} else {
			files = dialog.selectedFiles().mid(0, 1);
		}
		//		if (type == Type::ReadFile || type == Type::ReadFiles) {
		//			remoteContent = dialog.selectedRemoteContent();
		//		}
		return true;
	}

	files = QStringList();
	remoteContent = QByteArray();
	return false;
}

} // namespace FileDialog
} // namespace Platform
