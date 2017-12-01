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
#include "stdafx.h"
#include "platform/mac/specific_mac_p.h"

#include "mainwindow.h"
#include "mainwidget.h"
#include "messenger.h"
#include "storage/localstorage.h"
#include "media/player/media_player_instance.h"
#include "media/media_audio.h"
#include "platform/mac/mac_utilities.h"
#include "styles/style_window.h"
#include "lang/lang_keys.h"
#include "base/timer.h"

#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CFURL.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/ev_keymap.h>
#include <SPMediaKeyTap.h>

namespace {

constexpr auto kIgnoreActivationTimeoutMs = 500;

} // namespace

using Platform::Q2NSString;
using Platform::NSlang;
using Platform::NS2QString;

@interface qVisualize : NSObject {
}

+ (id)str:(const QString &)str;
- (id)initWithString:(const QString &)str;

+ (id)bytearr:(const QByteArray &)arr;
- (id)initWithByteArray:(const QByteArray &)arr;

- (id)debugQuickLookObject;

@end // @interface qVisualize

@implementation qVisualize {
	NSString *value;

}

+ (id)bytearr:(const QByteArray &)arr {
	return [[qVisualize alloc] initWithByteArray:arr];
}
- (id)initWithByteArray:(const QByteArray &)arr {
	if (self = [super init]) {
		value = [NSString stringWithUTF8String:arr.constData()];
	}
	return self;
}

+ (id)str:(const QString &)str {
	return [[qVisualize alloc] initWithString:str];
}
- (id)initWithString:(const QString &)str {
	if (self = [super init]) {
		value = [NSString stringWithUTF8String:str.toUtf8().constData()];
	}
	return self;
}

- (id)debugQuickLookObject {
	return value;
}

@end // @implementation qVisualize

@interface ApplicationDelegate : NSObject<NSApplicationDelegate> {
}

- (BOOL) applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)flag;
- (void) applicationDidFinishLaunching:(NSNotification *)aNotification;
- (void) applicationDidBecomeActive:(NSNotification *)aNotification;
- (void) receiveWakeNote:(NSNotification*)note;

- (void) setWatchingMediaKeys:(bool)watching;
- (bool) isWatchingMediaKeys;
- (void) mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;

- (void) ignoreApplicationActivationRightNow;

@end // @interface ApplicationDelegate

ApplicationDelegate *_sharedDelegate = nil;

@implementation ApplicationDelegate {
	SPMediaKeyTap *_keyTap;
	bool _watchingMediaKeys;
	bool _ignoreActivation;
	base::Timer _ignoreActivationStop;
}

- (BOOL) applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)flag {
	if (App::wnd() && App::wnd()->isHidden()) App::wnd()->showFromTray();
	return YES;
}

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification {
	_keyTap = nullptr;
	_watchingMediaKeys = false;
	_ignoreActivation = false;
	_ignoreActivationStop.setCallback([self] {
		_ignoreActivation = false;
	});
#ifndef OS_MAC_STORE
	if ([SPMediaKeyTap usesGlobalMediaKeyTap]) {
		_keyTap = [[SPMediaKeyTap alloc] initWithDelegate:self];
	} else {
		LOG(("Media key monitoring disabled"));
	}
#endif // else for !OS_MAC_STORE
}

- (void) applicationDidBecomeActive:(NSNotification *)aNotification {
	if (auto messenger = Messenger::InstancePointer()) {
		if (!_ignoreActivation) {
			messenger->handleAppActivated();
			if (auto window = App::wnd()) {
				if (window->isHidden()) {
					window->showFromTray();
				}
			}
		}
	}
}

- (void) receiveWakeNote:(NSNotification*)aNotification {
	if (auto messenger = Messenger::InstancePointer()) {
		messenger->checkLocalTime();
	}

	LOG(("Audio Info: -receiveWakeNote: received, scheduling detach from audio device"));
	Media::Audio::ScheduleDetachFromDeviceSafe();
}

- (void) setWatchingMediaKeys:(bool)watching {
	if (_watchingMediaKeys != watching) {
		_watchingMediaKeys = watching;
		if (_keyTap) {
#ifndef OS_MAC_STORE
			if (_watchingMediaKeys) {
				[_keyTap startWatchingMediaKeys];
			} else {
				[_keyTap stopWatchingMediaKeys];
			}
#endif // else for !OS_MAC_STORE
		}
	}
}

- (bool) isWatchingMediaKeys {
	return _watchingMediaKeys;
}

- (void) mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)e {
	if (e && [e type] == NSEventTypeSystemDefined && [e subtype] == SPSystemDefinedEventMediaKeys) {
		objc_handleMediaKeyEvent(e);
	}
}

- (void) ignoreApplicationActivationRightNow {
	_ignoreActivation = true;
	_ignoreActivationStop.callOnce(kIgnoreActivationTimeoutMs);
}

@end // @implementation ApplicationDelegate

namespace Platform {

void SetWatchingMediaKeys(bool watching) {
	if (_sharedDelegate) {
		[_sharedDelegate setWatchingMediaKeys:watching];
	}
}

void InitOnTopPanel(QWidget *panel) {
	Expects(!panel->windowHandle());

	// Force creating windowHandle() without creating the platform window yet.
	panel->setAttribute(Qt::WA_NativeWindow, true);
	panel->windowHandle()->setProperty("_td_macNonactivatingPanelMask", QVariant(true));
	panel->setAttribute(Qt::WA_NativeWindow, false);

	panel->createWinId();

	auto platformWindow = [reinterpret_cast<NSView*>(panel->winId()) window];
	Assert([platformWindow isKindOfClass:[NSPanel class]]);

	auto platformPanel = static_cast<NSPanel*>(platformWindow);
	[platformPanel setLevel:NSPopUpMenuWindowLevel];
	[platformPanel setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorStationary|NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorIgnoresCycle];
	[platformPanel setFloatingPanel:YES];
	[platformPanel setHidesOnDeactivate:NO];

	objc_ignoreApplicationActivationRightNow();
}

void DeInitOnTopPanel(QWidget *panel) {
	auto platformWindow = [reinterpret_cast<NSView*>(panel->winId()) window];
	Assert([platformWindow isKindOfClass:[NSPanel class]]);

	auto platformPanel = static_cast<NSPanel*>(platformWindow);
	auto newBehavior = ([platformPanel collectionBehavior] & (~NSWindowCollectionBehaviorCanJoinAllSpaces)) | NSWindowCollectionBehaviorMoveToActiveSpace;
	[platformPanel setCollectionBehavior:newBehavior];
}

void ReInitOnTopPanel(QWidget *panel) {
	auto platformWindow = [reinterpret_cast<NSView*>(panel->winId()) window];
	Assert([platformWindow isKindOfClass:[NSPanel class]]);

	auto platformPanel = static_cast<NSPanel*>(platformWindow);
	auto newBehavior = ([platformPanel collectionBehavior] & (~NSWindowCollectionBehaviorMoveToActiveSpace)) | NSWindowCollectionBehaviorCanJoinAllSpaces;
	[platformPanel setCollectionBehavior:newBehavior];
}

} // namespace Platform

bool objc_darkMode() {
	bool result = false;
	@autoreleasepool {

	NSDictionary *dict = [[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain];
	id style = [dict objectForKey:Q2NSString(strStyleOfInterface())];
	BOOL darkModeOn = (style && [style isKindOfClass:[NSString class]] && NSOrderedSame == [style caseInsensitiveCompare:@"dark"]);
	result = darkModeOn ? true : false;

	}
	return result;
}

void objc_showOverAll(WId winId, bool canFocus) {
	NSWindow *wnd = [reinterpret_cast<NSView *>(winId) window];
	[wnd setLevel:NSPopUpMenuWindowLevel];
	if (!canFocus) {
		[wnd setStyleMask:NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskNonactivatingPanel];
		[wnd setCollectionBehavior:NSWindowCollectionBehaviorMoveToActiveSpace|NSWindowCollectionBehaviorStationary|NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorIgnoresCycle];
	}
}

void objc_bringToBack(WId winId) {
	NSWindow *wnd = [reinterpret_cast<NSView *>(winId) window];
	[wnd setLevel:NSModalPanelWindowLevel];
}

bool objc_handleMediaKeyEvent(void *ev) {
	auto e = reinterpret_cast<NSEvent*>(ev);

	int keyCode = (([e data1] & 0xFFFF0000) >> 16);
	int keyFlags = ([e data1] & 0x0000FFFF);
	int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
	int keyRepeat = (keyFlags & 0x1);

	if (!_sharedDelegate || ![_sharedDelegate isWatchingMediaKeys]) {
		return false;
	}

	switch (keyCode) {
	case NX_KEYTYPE_PLAY:
		if (keyState == 0) { // Play pressed and released
			Media::Player::instance()->playPause();
			return true;
		}
		break;

	case NX_KEYTYPE_FAST:
		if (keyState == 0) { // Next pressed and released
			Media::Player::instance()->next();
			return true;
		}
		break;

	case NX_KEYTYPE_REWIND:
		if (keyState == 0) { // Previous pressed and released
			Media::Player::instance()->previous();
			return true;
		}
		break;
	}
	return false;
}

void objc_debugShowAlert(const QString &str) {
	@autoreleasepool {

	[[NSAlert alertWithMessageText:@"Debug Message" defaultButton:@"OK" alternateButton:nil otherButton:nil informativeTextWithFormat:@"%@", Q2NSString(str)] runModal];

	}
}

void objc_outputDebugString(const QString &str) {
	@autoreleasepool {

	NSLog(@"%@", Q2NSString(str));

	}
}

bool objc_idleSupported() {
	auto idleTime = 0LL;
	return objc_idleTime(idleTime);
}

bool objc_idleTime(TimeMs &idleTime) { // taken from https://github.com/trueinteractions/tint/issues/53
	CFMutableDictionaryRef properties = 0;
	CFTypeRef obj;
	mach_port_t masterPort;
	io_iterator_t iter;
	io_registry_entry_t curObj;

	IOMasterPort(MACH_PORT_NULL, &masterPort);

	/* Get IOHIDSystem */
	IOServiceGetMatchingServices(masterPort, IOServiceMatching("IOHIDSystem"), &iter);
	if (iter == 0) {
		return false;
	} else {
		curObj = IOIteratorNext(iter);
	}
	if (IORegistryEntryCreateCFProperties(curObj, &properties, kCFAllocatorDefault, 0) == KERN_SUCCESS && properties != NULL) {
		obj = CFDictionaryGetValue(properties, CFSTR("HIDIdleTime"));
		CFRetain(obj);
	} else {
		return false;
	}

	uint64_t err = ~0L, result = err;
	if (obj) {
		CFTypeID type = CFGetTypeID(obj);

		if (type == CFDataGetTypeID()) {
			CFDataGetBytes((CFDataRef) obj, CFRangeMake(0, sizeof(result)), (UInt8*)&result);
		} else if (type == CFNumberGetTypeID()) {
			CFNumberGetValue((CFNumberRef)obj, kCFNumberSInt64Type, &result);
		} else {
			// error
		}

		CFRelease(obj);

		if (result != err) {
			result /= 1000000; // return as ms
		}
	} else {
		// error
	}

	CFRelease((CFTypeRef)properties);
	IOObjectRelease(curObj);
	IOObjectRelease(iter);
	if (result == err) return false;

	idleTime = static_cast<TimeMs>(result);
	return true;
}

void objc_start() {
	_sharedDelegate = [[ApplicationDelegate alloc] init];
	[[NSApplication sharedApplication] setDelegate:_sharedDelegate];
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver: _sharedDelegate
														   selector: @selector(receiveWakeNote:)
															   name: NSWorkspaceDidWakeNotification object: NULL];
}

void objc_ignoreApplicationActivationRightNow() {
	if (_sharedDelegate) {
		[_sharedDelegate ignoreApplicationActivationRightNow];
	}
}

namespace {
	NSURL *_downloadPathUrl = nil;
}

void objc_finish() {
	[_sharedDelegate release];
	_sharedDelegate = nil;
	if (_downloadPathUrl) {
		[_downloadPathUrl stopAccessingSecurityScopedResource];
		_downloadPathUrl = nil;
	}
}

void objc_registerCustomScheme() {
#ifndef TDESKTOP_DISABLE_REGISTER_CUSTOM_SCHEME
	OSStatus result = LSSetDefaultHandlerForURLScheme(CFSTR("tg"), (CFStringRef)[[NSBundle mainBundle] bundleIdentifier]);
	DEBUG_LOG(("App Info: set default handler for 'tg' scheme result: %1").arg(result));
#endif // !TDESKTOP_DISABLE_REGISTER_CUSTOM_SCHEME
}

BOOL _execUpdater(BOOL update = YES, const QString &crashreport = QString()) {
	@autoreleasepool {

	NSString *path = @"", *args = @"";
	@try {
		path = [[NSBundle mainBundle] bundlePath];
		if (!path) {
			LOG(("Could not get bundle path!!"));
			return NO;
		}
		path = [path stringByAppendingString:@"/Contents/Frameworks/Updater"];

		NSMutableArray *args = [[NSMutableArray alloc] initWithObjects:@"-workpath", Q2NSString(cWorkingDir()), @"-procid", nil];
		[args addObject:[NSString stringWithFormat:@"%d", [[NSProcessInfo processInfo] processIdentifier]]];
		if (cRestartingToSettings()) [args addObject:@"-tosettings"];
		if (!update) [args addObject:@"-noupdate"];
		if (cLaunchMode() == LaunchModeAutoStart) [args addObject:@"-autostart"];
		if (cDebug()) [args addObject:@"-debug"];
		if (cStartInTray()) [args addObject:@"-startintray"];
		if (cTestMode()) [args addObject:@"-testmode"];
		if (cDataFile() != qsl("data")) {
			[args addObject:@"-key"];
			[args addObject:Q2NSString(cDataFile())];
		}
		if (!crashreport.isEmpty()) {
			[args addObject:@"-crashreport"];
			[args addObject:Q2NSString(crashreport)];
		}

		DEBUG_LOG(("Application Info: executing %1 %2").arg(NS2QString(path)).arg(NS2QString([args componentsJoinedByString:@" "])));
		Logs::closeMain();
		SignalHandlers::finish();
		if (![NSTask launchedTaskWithLaunchPath:path arguments:args]) {
			DEBUG_LOG(("Task not launched while executing %1 %2").arg(NS2QString(path)).arg(NS2QString([args componentsJoinedByString:@" "])));
			return NO;
		}
	}
	@catch (NSException *exception) {
		LOG(("Exception caught while executing %1 %2").arg(NS2QString(path)).arg(NS2QString(args)));
		return NO;
	}
	@finally {
	}

	}
	return YES;
}

bool objc_execUpdater() {
	return !!_execUpdater();
}

void objc_execTelegram(const QString &crashreport) {
	if (cExeName().isEmpty()) {
		return;
	}
#ifndef OS_MAC_STORE
	_execUpdater(NO, crashreport);
#else // OS_MAC_STORE
	@autoreleasepool {

	NSDictionary *conf = [NSDictionary dictionaryWithObject:[NSArray array] forKey:NSWorkspaceLaunchConfigurationArguments];
	[[NSWorkspace sharedWorkspace] launchApplicationAtURL:[NSURL fileURLWithPath:Q2NSString(cExeDir() + cExeName())] options:NSWorkspaceLaunchAsync | NSWorkspaceLaunchNewInstance configuration:conf error:0];

	}
#endif // OS_MAC_STORE
}

void objc_activateProgram(WId winId) {
	[NSApp activateIgnoringOtherApps:YES];
	if (winId) {
		NSWindow *w = [reinterpret_cast<NSView*>(winId) window];
		[w makeKeyAndOrderFront:NSApp];
	}
}

bool objc_moveFile(const QString &from, const QString &to) {
	@autoreleasepool {

	NSString *f = Q2NSString(from), *t = Q2NSString(to);
	if ([[NSFileManager defaultManager] fileExistsAtPath:t]) {
		NSData *data = [NSData dataWithContentsOfFile:f];
		if (data) {
			if ([data writeToFile:t atomically:YES]) {
				if ([[NSFileManager defaultManager] removeItemAtPath:f error:nil]) {
					return true;
				}
			}
		}
	} else {
		if ([[NSFileManager defaultManager] moveItemAtPath:f toPath:t error:nil]) {
			return true;
		}
	}

	}
	return false;
}

void objc_deleteDir(const QString &dir) {
	@autoreleasepool {

	[[NSFileManager defaultManager] removeItemAtPath:Q2NSString(dir) error:nil];

	}
}

double objc_appkitVersion() {
	return NSAppKitVersionNumber;
}

QString objc_documentsPath() {
	NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil];
	if (url) {
		return QString::fromUtf8([[url path] fileSystemRepresentation]) + '/';
	}
	return QString();
}

QString objc_appDataPath() {
	NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil];
	if (url) {
		return QString::fromUtf8([[url path] fileSystemRepresentation]) + '/' + str_const_toString(AppName) + '/';
	}
	return QString();
}

QString objc_downloadPath() {
	NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSDownloadsDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil];
	if (url) {
		return QString::fromUtf8([[url path] fileSystemRepresentation]) + '/' + str_const_toString(AppName) + '/';
	}
	return QString();
}

QByteArray objc_downloadPathBookmark(const QString &path) {
#ifndef OS_MAC_STORE
	return QByteArray();
#else // OS_MAC_STORE
	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.toUtf8().constData()] isDirectory:YES];
	if (!url) return QByteArray();

	NSError *error = nil;
	NSData *data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
	return data ? QByteArray::fromNSData(data) : QByteArray();
#endif // OS_MAC_STORE
}

QByteArray objc_pathBookmark(const QString &path) {
#ifndef OS_MAC_STORE
	return QByteArray();
#else // OS_MAC_STORE
	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.toUtf8().constData()]];
	if (!url) return QByteArray();

	NSError *error = nil;
	NSData *data = [url bookmarkDataWithOptions:(NSURLBookmarkCreationWithSecurityScope | NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess) includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
	return data ? QByteArray::fromNSData(data) : QByteArray();
#endif // OS_MAC_STORE
}

void objc_downloadPathEnableAccess(const QByteArray &bookmark) {
#ifdef OS_MAC_STORE
	if (bookmark.isEmpty()) return;

	BOOL isStale = NO;
	NSError *error = nil;
	NSURL *url = [NSURL URLByResolvingBookmarkData:bookmark.toNSData() options:NSURLBookmarkResolutionWithSecurityScope relativeToURL:nil bookmarkDataIsStale:&isStale error:&error];
	if (!url) return;

	if ([url startAccessingSecurityScopedResource]) {
		if (_downloadPathUrl) {
			[_downloadPathUrl stopAccessingSecurityScopedResource];
		}
		_downloadPathUrl = url;

		Global::SetDownloadPath(NS2QString([_downloadPathUrl path]) + '/');
		if (isStale) {
			NSData *data = [_downloadPathUrl bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
			if (data) {
				Global::SetDownloadPathBookmark(QByteArray::fromNSData(data));
				Local::writeUserSettings();
			}
		}
	}
#endif // OS_MAC_STORE
}

#ifdef OS_MAC_STORE
namespace {
	QMutex _bookmarksMutex;
}

class objc_FileBookmark::objc_FileBookmarkData {
public:
	~objc_FileBookmarkData() {
		if (url) [url release];
	}
	NSURL *url = nil;
	QString name;
	QByteArray bookmark;
	int counter = 0;
};
#endif // OS_MAC_STORE

objc_FileBookmark::objc_FileBookmark(const QByteArray &bookmark) {
#ifdef OS_MAC_STORE
	if (bookmark.isEmpty()) return;

	BOOL isStale = NO;
	NSError *error = nil;
	NSURL *url = [NSURL URLByResolvingBookmarkData:bookmark.toNSData() options:NSURLBookmarkResolutionWithSecurityScope relativeToURL:nil bookmarkDataIsStale:&isStale error:&error];
	if (!url) return;

	if ([url startAccessingSecurityScopedResource]) {
		data = new objc_FileBookmarkData();
		data->url = [url retain];
		data->name = NS2QString([url path]);
		data->bookmark = bookmark;
		[url stopAccessingSecurityScopedResource];
	}
#endif // OS_MAC_STORE
}

bool objc_FileBookmark::valid() const {
	if (enable()) {
		disable();
		return true;
	}
	return false;
}

bool objc_FileBookmark::enable() const {
#ifndef OS_MAC_STORE
	return true;
#else // OS_MAC_STORE
	if (!data) return false;

	QMutexLocker lock(&_bookmarksMutex);
	if (data->counter > 0 || [data->url startAccessingSecurityScopedResource] == YES) {
		++data->counter;
		return true;
	}
	return false;
#endif // OS_MAC_STORE
}

void objc_FileBookmark::disable() const {
#ifdef OS_MAC_STORE
	if (!data) return;

	QMutexLocker lock(&_bookmarksMutex);
	if (data->counter > 0) {
		--data->counter;
		if (!data->counter) {
			[data->url stopAccessingSecurityScopedResource];
		}
	}
#endif // OS_MAC_STORE
}

const QString &objc_FileBookmark::name(const QString &original) const {
#ifndef OS_MAC_STORE
	return original;
#else // OS_MAC_STORE
	return (data && !data->name.isEmpty()) ? data->name : original;
#endif // OS_MAC_STORE
}

QByteArray objc_FileBookmark::bookmark() const {
#ifndef OS_MAC_STORE
	return QByteArray();
#else // OS_MAC_STORE
	return data ? data->bookmark : QByteArray();
#endif // OS_MAC_STORE
}

objc_FileBookmark::~objc_FileBookmark() {
#ifdef OS_MAC_STORE
	if (data && data->counter > 0) {
		LOG(("Did not disable() bookmark, counter: %1").arg(data->counter));
		[data->url stopAccessingSecurityScopedResource];
	}
#endif // OS_MAC_STORE
}
