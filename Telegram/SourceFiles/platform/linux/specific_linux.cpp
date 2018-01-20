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

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#include "platform/linux/specific_linux.h"

#include "platform/linux/linux_libs.h"
#include "lang/lang_keys.h"
#include "application.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "platform/linux/file_utilities_linux.h"
#include "platform/platform_notifications_manager.h"
#include "storage/localstorage.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>

#include <iostream>

#include <qpa/qplatformnativeinterface.h>

using namespace Platform;
using Platform::File::internal::EscapeShell;

namespace Platform {

QString CurrentExecutablePath(int argc, char *argv[]) {
	constexpr auto kMaxPath = 1024;
	char result[kMaxPath] = { 0 };
	auto count = readlink("/proc/self/exe", result, kMaxPath);
	if (count > 0) {
		auto filename = QFile::decodeName(result);
		auto deletedPostfix = qstr(" (deleted)");
		if (filename.endsWith(deletedPostfix) && !QFileInfo(filename).exists()) {
			filename.chop(deletedPostfix.size());
		}
		return filename;
	}

	// Fallback to the first command line argument.
	return argc ? QFile::decodeName(argv[0]) : QString();
}

} // namespace Platform

namespace {

class _PsEventFilter : public QAbstractNativeEventFilter {
public:
	bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
		//auto wnd = App::wnd();
		//if (!wnd) return false;

		return false;
	}
};
_PsEventFilter *_psEventFilter = nullptr;

QRect _monitorRect;
auto _monitorLastGot = 0LL;

} // namespace

QRect psDesktopRect() {
	auto tnow = getms();
	if (tnow > _monitorLastGot + 1000LL || tnow < _monitorLastGot) {
		_monitorLastGot = tnow;
		_monitorRect = QApplication::desktop()->availableGeometry(App::wnd());
	}
	return _monitorRect;
}

void psShowOverAll(QWidget *w, bool canFocus) {
	w->show();
}

void psBringToBack(QWidget *w) {
	w->hide();
}

QAbstractNativeEventFilter *psNativeEventFilter() {
	delete _psEventFilter;
	_psEventFilter = new _PsEventFilter();
	return _psEventFilter;
}

void psWriteDump() {
}

QString demanglestr(const QString &mangled) {
	if (mangled.isEmpty()) return mangled;

	QByteArray cmd = ("c++filt -n " + mangled).toUtf8();
	FILE *f = popen(cmd.constData(), "r");
	if (!f) return "BAD_SYMBOL_" + mangled;

	QString result;
	char buffer[4096] = { 0 };
	while (!feof(f)) {
		if (fgets(buffer, 4096, f) != NULL) {
			result += buffer;
		}
	}
	pclose(f);
	return result.trimmed();
}

QStringList addr2linestr(quint64 *addresses, int count) {
	QStringList result;
	if (!count || cExeName().isEmpty()) return result;

	result.reserve(count);
	QByteArray cmd = "addr2line -e " + EscapeShell(QFile::encodeName(cExeDir() + cExeName()));
	for (int i = 0; i < count; ++i) {
		if (addresses[i]) {
			cmd += qsl(" 0x%1").arg(addresses[i], 0, 16).toUtf8();
		}
	}
	FILE *f = popen(cmd.constData(), "r");

	QStringList addr2lineResult;
	if (f) {
		char buffer[4096] = {0};
		while (!feof(f)) {
			if (fgets(buffer, 4096, f) != NULL) {
				addr2lineResult.push_back(QString::fromUtf8(buffer));
			}
		}
		pclose(f);
	}
	for (int i = 0, j = 0; i < count; ++i) {
		if (addresses[i]) {
			if (j < addr2lineResult.size() && !addr2lineResult.at(j).isEmpty() && !addr2lineResult.at(j).startsWith(qstr("0x"))) {
				QString res = addr2lineResult.at(j).trimmed();
				if (int index = res.indexOf(qstr("/Telegram/"))) {
					if (index > 0) {
						res = res.mid(index + qstr("/Telegram/").size());
					}
				}
				result.push_back(res);
			} else {
				result.push_back(QString());
			}
			++j;
		} else {
			result.push_back(QString());
		}
	}
	return result;
}

QString psPrepareCrashDump(const QByteArray &crashdump, QString dumpfile) {
	QString initial = QString::fromUtf8(crashdump), result;
	QStringList lines = initial.split('\n');
	result.reserve(initial.size());
	qint32 i = 0, l = lines.size();

	while (i < l) {
		quint64 addresses[1024] = { 0 };
		for (; i < l; ++i) {
			result.append(lines.at(i)).append('\n');
			QString line = lines.at(i).trimmed();
			if (line == qstr("Backtrace:")) {
				++i;
				break;
			}
		}

		qint32 start = i;
		for (; i < l; ++i) {
			QString line = lines.at(i).trimmed();
			if (line.isEmpty()) break;

			QRegularExpressionMatch m1 = QRegularExpression(qsl("^(.+)\\(([^+]+)\\+([^\\)]+)\\)\\[(.+)\\]$")).match(line);
			QRegularExpressionMatch m2 = QRegularExpression(qsl("^(.+)\\[(.+)\\]$")).match(line);
			QString addrstr = m1.hasMatch() ? m1.captured(4) : (m2.hasMatch() ? m2.captured(2) : QString());
			if (!addrstr.isEmpty()) {
				quint64 addr = addrstr.startsWith(qstr("0x")) ? addrstr.mid(2).toULongLong(0, 16) : addrstr.toULongLong();
				if (addr > 1) {
					addresses[i - start] = addr;
				}
			}
		}

		QStringList addr2line = addr2linestr(addresses, i - start);
		for (i = start; i < l; ++i) {
			QString line = lines.at(i).trimmed();
			if (line.isEmpty()) break;

			result.append(qsl("\n%1. ").arg(i - start));
			if (line.startsWith(qstr("ERROR: "))) {
				result.append(line).append('\n');
				continue;
			}
			if (line == qstr("[0x1]")) {
				result.append(qsl("(0x1 separator)\n"));
				continue;
			}

			QRegularExpressionMatch m1 = QRegularExpression(qsl("^(.+)\\(([^+]*)\\+([^\\)]+)\\)(.+)$")).match(line);
			QRegularExpressionMatch m2 = QRegularExpression(qsl("^(.+)\\[(.+)\\]$")).match(line);
			if (!m1.hasMatch() && !m2.hasMatch()) {
				result.append(qstr("BAD LINE: ")).append(line).append('\n');
				continue;
			}

			if (m1.hasMatch()) {
				result.append(demanglestr(m1.captured(2))).append(qsl(" + ")).append(m1.captured(3)).append(qsl(" [")).append(m1.captured(1)).append(qsl("] "));
				if (!addr2line.at(i - start).isEmpty() && addr2line.at(i - start) != qsl("??:0")) {
					result.append(qsl(" (")).append(addr2line.at(i - start)).append(qsl(")\n"));
				} else {
					result.append(m1.captured(4)).append(qsl(" (demangled)")).append('\n');
				}
			} else {
				result.append('[').append(m2.captured(1)).append(']');
				if (!addr2line.at(i - start).isEmpty() && addr2line.at(i - start) != qsl("??:0")) {
					result.append(qsl(" (")).append(addr2line.at(i - start)).append(qsl(")\n"));
				} else {
					result.append(' ').append(m2.captured(2)).append('\n');
				}
			}
		}
	}
	return result;
}

bool _removeDirectory(const QString &path) { // from http://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
	QByteArray pathRaw = QFile::encodeName(path);
	DIR *d = opendir(pathRaw.constData());
	if (!d) return false;

	while (struct dirent *p = readdir(d)) {
		/* Skip the names "." and ".." as we don't want to recurse on them. */
		if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;

		QString fname = path + '/' + p->d_name;
		QByteArray fnameRaw = QFile::encodeName(fname);
		struct stat statbuf;
		if (!stat(fnameRaw.constData(), &statbuf)) {
			if (S_ISDIR(statbuf.st_mode)) {
				if (!_removeDirectory(fname)) {
					closedir(d);
					return false;
				}
			} else {
				if (unlink(fnameRaw.constData())) {
					closedir(d);
					return false;
				}
			}
		}
	}
	closedir(d);

	return !rmdir(pathRaw.constData());
}

void psDeleteDir(const QString &dir) {
	_removeDirectory(dir);
}

namespace {

auto _lastUserAction = 0LL;

} // namespace

void psUserActionDone() {
	_lastUserAction = getms(true);
}

bool psIdleSupported() {
	return false;
}

TimeMs psIdleTime() {
	return getms(true) - _lastUserAction;
}

void psActivateProcess(quint64 pid) {
//	objc_activateProgram();
}

namespace {

QString getHomeDir() {
	struct passwd *pw = getpwuid(getuid());
	return (pw && pw->pw_dir && strlen(pw->pw_dir)) ? (QFile::decodeName(pw->pw_dir) + '/') : QString();
}

} // namespace

QString psAppDataPath() {
	// Previously we used ~/.TelegramDesktop, so look there first.
	// If we find data there, we should still use it.
	auto home = getHomeDir();
	if (!home.isEmpty()) {
		auto oldPath = home + qsl(".Kepka/");
		auto oldSettingsBase = oldPath + qsl("tdata/settings");
		if (QFile(oldSettingsBase + '0').exists() || QFile(oldSettingsBase + '1').exists()) {
			return oldPath;
		}
	}

	return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + '/';
}

QString psDownloadPath() {
	return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + '/' + str_const_toString(AppName) + '/';
}

void psDoCleanup() {
	try {
		psAutoStart(false, true);
		psSendToMenu(false, true);
	} catch (...) {
	}
}

int psCleanup() {
	psDoCleanup();
	return 0;
}

void psDoFixPrevious() {
}

int psFixPrevious() {
	psDoFixPrevious();
	return 0;
}

namespace Platform {

void start() {
}

void finish() {
	Notifications::Finish();

	delete _psEventFilter;
	_psEventFilter = nullptr;
}

bool TranslucentWindowsSupported(QPoint globalPosition) {
	if (auto app = static_cast<QGuiApplication*>(QCoreApplication::instance())) {
		if (auto native = app->platformNativeInterface()) {
			if (auto desktop = QApplication::desktop()) {
				auto index = desktop->screenNumber(globalPosition);
				auto screens = QGuiApplication::screens();
				if (auto screen = (index >= 0 && index < screens.size()) ? screens[index] : QGuiApplication::primaryScreen()) {
					if (native->nativeResourceForScreen(QByteArray("compositingEnabled"), screen)) {
						return true;
					}

					static OrderedSet<int> WarnedAbout;
					if (!WarnedAbout.contains(index)) {
						WarnedAbout.insert(index);
						LOG(("WARNING: Compositing is disabled for screen index %1 (for position %2,%3)").arg(index).arg(globalPosition.x()).arg(globalPosition.y()));
					}
				} else {
					LOG(("WARNING: Could not get screen for index %1 (for position %2,%3)").arg(index).arg(globalPosition.x()).arg(globalPosition.y()));
				}
			}
		}
	}
	return false;
}

QString SystemCountry() {
	return QString();
}

QString SystemLanguage() {
	return QString();
}

namespace ThirdParty {

void start() {
	Libs::start();
	MainWindow::LibsLoaded();
}

void finish() {
}

} // namespace ThirdParty

} // namespace Platform

namespace {

bool _psRunCommand(const QByteArray &command) {
	auto result = system(command.constData());
	if (result) {
		DEBUG_LOG(("App Error: command failed, code: %1, command (in utf8): %2").arg(result).arg(command.constData()));
		return false;
	}
	DEBUG_LOG(("App Info: command succeeded, command (in utf8): %1").arg(command.constData()));
	return true;
}

} // namespace

void psRegisterCustomScheme() {
#ifndef TDESKTOP_DISABLE_REGISTER_CUSTOM_SCHEME
	auto home = getHomeDir();
	if (home.isEmpty() || cBetaVersion() || cExeName().isEmpty()) return; // don't update desktop file for beta version

#ifndef TDESKTOP_DISABLE_DESKTOP_FILE_GENERATION
	DEBUG_LOG(("App Info: placing .desktop file"));
	if (QDir(home + qsl(".local/")).exists()) {
		QString apps = home + qsl(".local/share/applications/");
		QString icons = home + qsl(".local/share/icons/");
		if (!QDir(apps).exists()) QDir().mkpath(apps);
		if (!QDir(icons).exists()) QDir().mkpath(icons);

		QString path = cWorkingDir() + qsl("tdata/"), file = path + qsl("telegramdesktop.desktop");
		QDir().mkpath(path);
		QFile f(file);
		if (f.open(QIODevice::WriteOnly)) {
			QString icon = icons + qsl("telegram.png");
			auto iconExists = QFile(icon).exists();
			if (Local::oldSettingsVersion() < 10021 && iconExists) {
				// Icon was changed.
				if (QFile(icon).remove()) {
					iconExists = false;
				}
			}
			if (!iconExists) {
				if (QFile(qsl(":/gui/art/logo_256.png")).copy(icon)) {
					DEBUG_LOG(("App Info: Icon copied to 'tdata'"));
				}
			}

			QTextStream s(&f);
			s.setCodec("UTF-8");
			s << "[Desktop Entry]\n";
			s << "Version=1.0\n";
			s << "Name=" << str_const_toString(AppName) << '\n';
			s << "Comment=Unofficial desktop version of Telegram messaging app\n";
			s << "TryExec=" << EscapeShell(QFile::encodeName(cExeDir() + cExeName())) << "\n";
			s << "Exec=" << EscapeShell(QFile::encodeName(cExeDir() + cExeName())) << " -- %u\n";
			s << "Icon=telegram\n";
			s << "Terminal=false\n";
			s << "StartupWMClass=Kepka\n";
			s << "StartupWMClass=" << str_const_toString(AppName) << '\n';
			s << "Type=Application\n";
			s << "Categories=Network;InstantMessaging;Qt;\n";
			s << "MimeType=x-scheme-handler/tg;\n";
			f.close();

			if (_psRunCommand("desktop-file-install --dir=" + EscapeShell(QFile::encodeName(home + qsl(".local/share/applications"))) + " --delete-original " + EscapeShell(QFile::encodeName(file)))) {
				DEBUG_LOG(("App Info: removing old .desktop file"));
				QFile(qsl("%1.local/share/applications/telegram.desktop").arg(home)).remove();

				_psRunCommand("update-desktop-database " + EscapeShell(QFile::encodeName(home + qsl(".local/share/applications"))));
				_psRunCommand("xdg-mime default telegramdesktop.desktop x-scheme-handler/tg");
			}
		} else {
			LOG(("App Error: Could not open '%1' for write").arg(file));
		}
	}
#endif // !TDESKTOP_DISABLE_DESKTOP_FILE_GENERATION

	DEBUG_LOG(("App Info: registerting for Gnome"));
	if (_psRunCommand("gconftool-2 -t string -s /desktop/gnome/url-handlers/tg/command " + EscapeShell(EscapeShell(QFile::encodeName(cExeDir() + cExeName())) + " -- %s"))) {
		_psRunCommand("gconftool-2 -t bool -s /desktop/gnome/url-handlers/tg/needs_terminal false");
		_psRunCommand("gconftool-2 -t bool -s /desktop/gnome/url-handlers/tg/enabled true");
	}

	DEBUG_LOG(("App Info: placing .protocol file"));
	QString services;
	if (QDir(home + qsl(".kde4/")).exists()) {
		services = home + qsl(".kde4/share/kde4/services/");
	} else if (QDir(home + qsl(".kde/")).exists()) {
		services = home + qsl(".kde/share/kde4/services/");
	}
	if (!services.isEmpty()) {
		if (!QDir(services).exists()) QDir().mkpath(services);

		QString path = services, file = path + qsl("tg.protocol");
		QFile f(file);
		if (f.open(QIODevice::WriteOnly)) {
			QTextStream s(&f);
			s.setCodec("UTF-8");
			s << "[Protocol]\n";
			s << "exec=" << QFile::decodeName(EscapeShell(QFile::encodeName(cExeDir() + cExeName()))) << " -- %u\n";
			s << "protocol=tg\n";
			s << "input=none\n";
			s << "output=none\n";
			s << "helper=true\n";
			s << "listing=false\n";
			s << "reading=false\n";
			s << "writing=false\n";
			s << "makedir=false\n";
			s << "deleting=false\n";
			f.close();
		} else {
			LOG(("App Error: Could not open '%1' for write").arg(file));
		}
	}
#endif // !TDESKTOP_DISABLE_REGISTER_CUSTOM_SCHEME
}

void psNewVersion() {
	psRegisterCustomScheme();
}

bool _execUpdater(bool update = true, const QString &crashreport = QString()) {
	if (cExeName().isEmpty()) {
		return false;
	}
	static const int MaxLen = 65536, MaxArgsCount = 128;

	char path[MaxLen] = {0};
	QByteArray data(QFile::encodeName(cExeDir() + (update ? "Updater" : cExeName())));
	memcpy(path, data.constData(), data.size());

	char *args[MaxArgsCount] = { 0 };
	char p_noupdate[] = "-noupdate";
	char p_autostart[] = "-autostart";
	char p_debug[] = "-debug";
	char p_tosettings[] = "-tosettings";
	char p_key[] = "-key";
	char p_datafile[MaxLen] = { 0 };
	char p_path[] = "-workpath";
	char p_pathbuf[MaxLen] = { 0 };
	char p_startintray[] = "-startintray";
	char p_testmode[] = "-testmode";
	char p_crashreport[] = "-crashreport";
	char p_crashreportbuf[MaxLen] = { 0 };
	char p_exe[] = "-exename";
	char p_exebuf[MaxLen] = { 0 };
	char p_exepath[] = "-exepath";
	char p_exepathbuf[MaxLen] = { 0 };
	int argIndex = 0;
	args[argIndex++] = path;
	if (!update) {
		args[argIndex++] = p_noupdate;
		args[argIndex++] = p_tosettings;
	}
	if (cLaunchMode() == LaunchModeAutoStart) args[argIndex++] = p_autostart;
	if (cDebug()) args[argIndex++] = p_debug;
	if (cStartInTray()) args[argIndex++] = p_startintray;
	if (cTestMode()) args[argIndex++] = p_testmode;
	if (cDataFile() != qsl("data")) {
		QByteArray dataf = QFile::encodeName(cDataFile());
		if (dataf.size() < MaxLen) {
			memcpy(p_datafile, dataf.constData(), dataf.size());
			args[argIndex++] = p_key;
			args[argIndex++] = p_datafile;
		}
	}
	QByteArray pathf = QFile::encodeName(cWorkingDir());
	if (pathf.size() < MaxLen) {
		memcpy(p_pathbuf, pathf.constData(), pathf.size());
		args[argIndex++] = p_path;
		args[argIndex++] = p_pathbuf;
	}
	if (!crashreport.isEmpty()) {
		QByteArray crashreportf = QFile::encodeName(crashreport);
		if (crashreportf.size() < MaxLen) {
			memcpy(p_crashreportbuf, crashreportf.constData(), crashreportf.size());
			args[argIndex++] = p_crashreport;
			args[argIndex++] = p_crashreportbuf;
		}
	}
	QByteArray exef = QFile::encodeName(cExeName());
	if (exef.size() > 0 && exef.size() < MaxLen) {
		memcpy(p_exebuf, exef.constData(), exef.size());
		args[argIndex++] = p_exe;
		args[argIndex++] = p_exebuf;
	}
	QByteArray exepathf = QFile::encodeName(cExeDir());
	if (exepathf.size() > 0 && exepathf.size() < MaxLen) {
		memcpy(p_exepathbuf, exepathf.constData(), exepathf.size());
		args[argIndex++] = p_exepath;
		args[argIndex++] = p_exepathbuf;
	}

	Logs::closeMain();
	SignalHandlers::finish();
	pid_t pid = fork();
	switch (pid) {
	case -1: return false;
	case 0: execv(path, args); return false;
	}
	return true;
}

void psExecUpdater() {
	if (!_execUpdater()) {
		psDeleteDir(cWorkingDir() + qsl("tupdates/temp"));
	}
}

void psExecTelegram(const QString &crashreport) {
	_execUpdater(false, crashreport);
}

bool psShowOpenWithMenu(int x, int y, const QString &file) {
	return false;
}

void psAutoStart(bool start, bool silent) {
}

void psSendToMenu(bool send, bool silent) {
}

void psUpdateOverlayed(QWidget *widget) {
}

bool linuxMoveFile(const char *from, const char *to) {
	FILE *ffrom = fopen(from, "rb"), *fto = fopen(to, "wb");
	if (!ffrom) {
		if (fto) fclose(fto);
		return false;
	}
	if (!fto) {
		fclose(ffrom);
		return false;
	}
	static const int BufSize = 65536;
	char buf[BufSize];
	while (size_t size = fread(buf, 1, BufSize, ffrom)) {
		fwrite(buf, 1, size, fto);
	}

	struct stat fst; // from http://stackoverflow.com/questions/5486774/keeping-fileowner-and-permissions-after-copying-file-in-c
	//let's say this wont fail since you already worked OK on that fp
	if (fstat(fileno(ffrom), &fst) != 0) {
		fclose(ffrom);
		fclose(fto);
		return false;
	}
	//update to the same uid/gid
	if (fchown(fileno(fto), fst.st_uid, fst.st_gid) != 0) {
		fclose(ffrom);
		fclose(fto);
		return false;
	}
	//update the permissions
	if (fchmod(fileno(fto), fst.st_mode) != 0) {
		fclose(ffrom);
		fclose(fto);
		return false;
	}

	fclose(ffrom);
	fclose(fto);

	if (unlink(from)) {
		return false;
	}

	return true;
}

bool psLaunchMaps(const LocationCoords &coords) {
	return false;
}
