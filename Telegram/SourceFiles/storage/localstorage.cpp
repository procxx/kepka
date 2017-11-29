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
#include "storage/localstorage.h"

#include "storage/serialize_document.h"
#include "storage/serialize_common.h"
#include "data/data_drafts.h"
#include "window/themes/window_theme.h"
#include "observer_peer.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "lang/lang_keys.h"
#include "media/media_audio.h"
#include "ui/widgets/input_fields.h"
#include "mtproto/dc_options.h"
#include "messenger.h"
#include "application.h"
#include "apiwrap.h"
#include "auth_session.h"
#include "window/window_controller.h"
#include "base/flags.h"

#include <openssl/evp.h>

namespace Local {
namespace {

constexpr int kThemeFileSizeLimit = 5 * 1024 * 1024;

using FileKey = uint64_t;

constexpr char tdfMagic[] = { 'T', 'D', 'F', '$' };
constexpr int tdfMagicLen = sizeof(tdfMagic);

QString toFilePart(FileKey val) {
	QString result;
	result.reserve(0x10);
	for (int32_t i = 0; i < 0x10; ++i) {
		uchar v = (val & 0x0F);
		result.push_back((v < 0x0A) ? ('0' + v) : ('A' + (v - 0x0A)));
		val >>= 4;
	}
	return result;
}

QString _basePath, _userBasePath;

bool _started = false;
internal::Manager *_manager = nullptr;
TaskQueue *_localLoader = nullptr;

bool _working() {
	return _manager && !_basePath.isEmpty();
}

bool _userWorking() {
	return _manager && !_basePath.isEmpty() && !_userBasePath.isEmpty();
}

enum class FileOption {
	User = (1 << 0),
	Safe = (1 << 1),
};
using FileOptions = base::flags<FileOption>;
inline constexpr auto is_flag_type(FileOption) { return true; };

bool keyAlreadyUsed(QString &name, FileOptions options = FileOption::User | FileOption::Safe) {
	name += '0';
	if (QFileInfo(name).exists()) return true;
	if (options & (FileOption::Safe)) {
		name[name.size() - 1] = '1';
		return QFileInfo(name).exists();
	}
	return false;
}

FileKey genKey(FileOptions options = FileOption::User | FileOption::Safe) {
	if (options & FileOption::User) {
		if (!_userWorking()) return 0;
	} else {
		if (!_working()) return 0;
	}

	FileKey result;
	QString base = (options & FileOption::User) ? _userBasePath : _basePath, path;
	path.reserve(base.size() + 0x11);
	path += base;
	do {
		result = rand_value<FileKey>();
		path.resize(base.size());
		path += toFilePart(result);
	} while (!result || keyAlreadyUsed(path, options));

	return result;
}

void clearKey(const FileKey &key, FileOptions options = FileOption::User | FileOption::Safe) {
	if (options & FileOption::User) {
		if (!_userWorking()) return;
	} else {
		if (!_working()) return;
	}

	QString base = (options & FileOption::User) ? _userBasePath : _basePath, name;
	name.reserve(base.size() + 0x11);
	name.append(base).append(toFilePart(key)).append('0');
	QFile::remove(name);
	if (options & FileOption::Safe) {
		name[name.size() - 1] = '1';
		QFile::remove(name);
	}
}

bool _checkStreamStatus(QDataStream &stream) {
	if (stream.status() != QDataStream::Ok) {
		LOG(("Bad data stream status: %1").arg(stream.status()));
		return false;
	}
	return true;
}

QByteArray _settingsSalt, _passKeySalt, _passKeyEncrypted;

constexpr auto kLocalKeySize = MTP::AuthKey::kSize;

auto OldKey = MTP::AuthKeyPtr();
auto SettingsKey = MTP::AuthKeyPtr();
auto PassKey = MTP::AuthKeyPtr();
auto LocalKey = MTP::AuthKeyPtr();

void createLocalKey(const QByteArray &pass, QByteArray *salt, MTP::AuthKeyPtr *result) {
	auto key = MTP::AuthKey::Data { { gsl::byte{} } };
	auto iterCount = pass.size() ? LocalEncryptIterCount : LocalEncryptNoPwdIterCount; // dont slow down for no password
	auto newSalt = QByteArray();
	if (!salt) {
		newSalt.resize(LocalEncryptSaltSize);
		memset_rand(newSalt.data(), newSalt.size());
		salt = &newSalt;

		cSetLocalSalt(newSalt);
	}

	PKCS5_PBKDF2_HMAC_SHA1(pass.constData(), pass.size(), (uchar*)salt->data(), salt->size(), iterCount, key.size(), (uchar*)key.data());

	*result = std::make_shared<MTP::AuthKey>(key);
}

struct FileReadDescriptor {
	FileReadDescriptor() : version(0) {
	}
	int32_t version;
	QByteArray data;
	QBuffer buffer;
	QDataStream stream;
	~FileReadDescriptor() {
		if (version) {
			stream.setDevice(0);
			if (buffer.isOpen()) buffer.close();
			buffer.setBuffer(0);
		}
	}
};

struct EncryptedDescriptor {
	EncryptedDescriptor() {
	}
	EncryptedDescriptor(uint32_t size) {
		uint32_t fullSize = sizeof(uint32_t) + size;
		if (fullSize & 0x0F) fullSize += 0x10 - (fullSize & 0x0F);
		data.reserve(fullSize);

		data.resize(sizeof(uint32_t));
		buffer.setBuffer(&data);
		buffer.open(QIODevice::WriteOnly);
		buffer.seek(sizeof(uint32_t));
		stream.setDevice(&buffer);
		stream.setVersion(QDataStream::Qt_5_1);
	}
	QByteArray data;
	QBuffer buffer;
	QDataStream stream;
	void finish() {
		if (stream.device()) stream.setDevice(0);
		if (buffer.isOpen()) buffer.close();
		buffer.setBuffer(0);
	}
	~EncryptedDescriptor() {
		finish();
	}
};

struct FileWriteDescriptor {
	FileWriteDescriptor(const FileKey &key, FileOptions options = FileOption::User | FileOption::Safe) {
		init(toFilePart(key), options);
	}
	FileWriteDescriptor(const QString &name, FileOptions options = FileOption::User | FileOption::Safe) {
		init(name, options);
	}
	void init(const QString &name, FileOptions options) {
		if (options & FileOption::User) {
			if (!_userWorking()) return;
		} else {
			if (!_working()) return;
		}

		// detect order of read attempts and file version
		QString toTry[2];
		toTry[0] = ((options & FileOption::User) ? _userBasePath : _basePath) + name + '0';
		if (options & FileOption::Safe) {
			toTry[1] = ((options & FileOption::User) ? _userBasePath : _basePath) + name + '1';
			QFileInfo toTry0(toTry[0]);
			QFileInfo toTry1(toTry[1]);
			if (toTry0.exists()) {
				if (toTry1.exists()) {
					QDateTime mod0 = toTry0.lastModified(), mod1 = toTry1.lastModified();
					if (mod0 > mod1) {
						qSwap(toTry[0], toTry[1]);
					}
				} else {
					qSwap(toTry[0], toTry[1]);
				}
				toDelete = toTry[1];
			} else if (toTry1.exists()) {
				toDelete = toTry[1];
			}
		}

		file.setFileName(toTry[0]);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(tdfMagic, tdfMagicLen);
			int32_t version = AppVersion;
			file.write((const char*)&version, sizeof(version));

			stream.setDevice(&file);
			stream.setVersion(QDataStream::Qt_5_1);
		}
	}
	bool writeData(const QByteArray &data) {
		if (!file.isOpen()) return false;

		stream << data;
		uint32_t len = data.isNull() ? 0xffffffff : data.size();
		if (QSysInfo::ByteOrder != QSysInfo::BigEndian) {
			len = qbswap(len);
		}
		md5.feed(&len, sizeof(len));
		md5.feed(data.constData(), data.size());
		dataSize += sizeof(len) + data.size();

		return true;
	}
	static QByteArray prepareEncrypted(EncryptedDescriptor &data, const MTP::AuthKeyPtr &key = LocalKey) {
		data.finish();
		QByteArray &toEncrypt(data.data);

		// prepare for encryption
		uint32_t size = toEncrypt.size(), fullSize = size;
		if (fullSize & 0x0F) {
			fullSize += 0x10 - (fullSize & 0x0F);
			toEncrypt.resize(fullSize);
			memset_rand(toEncrypt.data() + size, fullSize - size);
		}
		*(uint32_t*)toEncrypt.data() = size;
		QByteArray encrypted(0x10 + fullSize, Qt::Uninitialized); // 128bit of sha1 - key128, sizeof(data), data
		hashSha1(toEncrypt.constData(), toEncrypt.size(), encrypted.data());
		MTP::aesEncryptLocal(toEncrypt.constData(), encrypted.data() + 0x10, fullSize, key, encrypted.constData());

		return encrypted;
	}
	bool writeEncrypted(EncryptedDescriptor &data, const MTP::AuthKeyPtr &key = LocalKey) {
		return writeData(prepareEncrypted(data, key));
	}
	void finish() {
		if (!file.isOpen()) return;

		stream.setDevice(0);

		md5.feed(&dataSize, sizeof(dataSize));
		int32_t version = AppVersion;
		md5.feed(&version, sizeof(version));
		md5.feed(tdfMagic, tdfMagicLen);
		file.write((const char*)md5.result(), 0x10);
		file.close();

		if (!toDelete.isEmpty()) {
			QFile::remove(toDelete);
		}
	}
	QFile file;
	QDataStream stream;

	QString toDelete;

	HashMd5 md5;
	int32_t dataSize = 0;

	~FileWriteDescriptor() {
		finish();
	}
};

bool readFile(FileReadDescriptor &result, const QString &name, FileOptions options = FileOption::User | FileOption::Safe) {
	if (options & FileOption::User) {
		if (!_userWorking()) return false;
	} else {
		if (!_working()) return false;
	}

	// detect order of read attempts
	QString toTry[2];
	toTry[0] = ((options & FileOption::User) ? _userBasePath : _basePath) + name + '0';
	if (options & FileOption::Safe) {
		QFileInfo toTry0(toTry[0]);
		if (toTry0.exists()) {
			toTry[1] = ((options & FileOption::User) ? _userBasePath : _basePath) + name + '1';
			QFileInfo toTry1(toTry[1]);
			if (toTry1.exists()) {
				QDateTime mod0 = toTry0.lastModified(), mod1 = toTry1.lastModified();
				if (mod0 < mod1) {
					qSwap(toTry[0], toTry[1]);
				}
			} else {
				toTry[1] = QString();
			}
		} else {
			toTry[0][toTry[0].size() - 1] = '1';
		}
	}
	for (int32_t i = 0; i < 2; ++i) {
		QString fname(toTry[i]);
		if (fname.isEmpty()) break;

		QFile f(fname);
		if (!f.open(QIODevice::ReadOnly)) {
			DEBUG_LOG(("App Info: failed to open '%1' for reading").arg(name));
			continue;
		}

		// check magic
		char magic[tdfMagicLen];
		if (f.read(magic, tdfMagicLen) != tdfMagicLen) {
			DEBUG_LOG(("App Info: failed to read magic from '%1'").arg(name));
			continue;
		}
		if (memcmp(magic, tdfMagic, tdfMagicLen)) {
			DEBUG_LOG(("App Info: bad magic %1 in '%2'").arg(Logs::mb(magic, tdfMagicLen).str()).arg(name));
			continue;
		}

		// read app version
		int32_t version;
		if (f.read((char*)&version, sizeof(version)) != sizeof(version)) {
			DEBUG_LOG(("App Info: failed to read version from '%1'").arg(name));
			continue;
		}
		if (version > AppVersion) {
			DEBUG_LOG(("App Info: version too big %1 for '%2', my version %3").arg(version).arg(name).arg(AppVersion));
			continue;
		}

		// read data
		QByteArray bytes = f.read(f.size());
		int32_t dataSize = bytes.size() - 16;
		if (dataSize < 0) {
			DEBUG_LOG(("App Info: bad file '%1', could not read sign part").arg(name));
			continue;
		}

		// check signature
		HashMd5 md5;
		md5.feed(bytes.constData(), dataSize);
		md5.feed(&dataSize, sizeof(dataSize));
		md5.feed(&version, sizeof(version));
		md5.feed(magic, tdfMagicLen);
		if (memcmp(md5.result(), bytes.constData() + dataSize, 16)) {
			DEBUG_LOG(("App Info: bad file '%1', signature did not match").arg(name));
			continue;
		}

		bytes.resize(dataSize);
		result.data = bytes;
		bytes = QByteArray();

		result.version = version;
		result.buffer.setBuffer(&result.data);
		result.buffer.open(QIODevice::ReadOnly);
		result.stream.setDevice(&result.buffer);
		result.stream.setVersion(QDataStream::Qt_5_1);

		if ((i == 0 && !toTry[1].isEmpty()) || i == 1) {
			QFile::remove(toTry[1 - i]);
		}

		return true;
	}
	return false;
}

bool decryptLocal(EncryptedDescriptor &result, const QByteArray &encrypted, const MTP::AuthKeyPtr &key = LocalKey) {
	if (encrypted.size() <= 16 || (encrypted.size() & 0x0F)) {
		LOG(("App Error: bad encrypted part size: %1").arg(encrypted.size()));
		return false;
	}
	uint32_t fullLen = encrypted.size() - 16;

	QByteArray decrypted;
	decrypted.resize(fullLen);
	const char *encryptedKey = encrypted.constData(), *encryptedData = encrypted.constData() + 16;
	aesDecryptLocal(encryptedData, decrypted.data(), fullLen, key, encryptedKey);
	uchar sha1Buffer[20];
	if (memcmp(hashSha1(decrypted.constData(), decrypted.size(), sha1Buffer), encryptedKey, 16)) {
		LOG(("App Info: bad decrypt key, data not decrypted - incorrect password?"));
		return false;
	}

	uint32_t dataLen = *(const uint32_t*)decrypted.constData();
	if (dataLen > uint32_t(decrypted.size()) || dataLen <= fullLen - 16 || dataLen < sizeof(uint32_t)) {
		LOG(("App Error: bad decrypted part size: %1, fullLen: %2, decrypted size: %3").arg(dataLen).arg(fullLen).arg(decrypted.size()));
		return false;
	}

	decrypted.resize(dataLen);
	result.data = decrypted;
	decrypted = QByteArray();

	result.buffer.setBuffer(&result.data);
	result.buffer.open(QIODevice::ReadOnly);
	result.buffer.seek(sizeof(uint32_t)); // skip len
	result.stream.setDevice(&result.buffer);
	result.stream.setVersion(QDataStream::Qt_5_1);

	return true;
}

bool readEncryptedFile(FileReadDescriptor &result, const QString &name, FileOptions options = FileOption::User | FileOption::Safe, const MTP::AuthKeyPtr &key = LocalKey) {
	if (!readFile(result, name, options)) {
		return false;
	}
	QByteArray encrypted;
	result.stream >> encrypted;

	EncryptedDescriptor data;
	if (!decryptLocal(data, encrypted, key)) {
		result.stream.setDevice(0);
		if (result.buffer.isOpen()) result.buffer.close();
		result.buffer.setBuffer(0);
		result.data = QByteArray();
		result.version = 0;
		return false;
	}

	result.stream.setDevice(0);
	if (result.buffer.isOpen()) result.buffer.close();
	result.buffer.setBuffer(0);
	result.data = data.data;
	result.buffer.setBuffer(&result.data);
	result.buffer.open(QIODevice::ReadOnly);
	result.buffer.seek(data.buffer.pos());
	result.stream.setDevice(&result.buffer);
	result.stream.setVersion(QDataStream::Qt_5_1);

	return true;
}

bool readEncryptedFile(FileReadDescriptor &result, const FileKey &fkey, FileOptions options = FileOption::User | FileOption::Safe, const MTP::AuthKeyPtr &key = LocalKey) {
	return readEncryptedFile(result, toFilePart(fkey), options, key);
}

FileKey _dataNameKey = 0;

enum { // Local Storage Keys
	lskUserMap = 0x00,
	lskDraft = 0x01, // data: PeerId peer
	lskDraftPosition = 0x02, // data: PeerId peer
	lskImages = 0x03, // data: StorageKey location
	lskLocations = 0x04, // no data
	lskStickerImages = 0x05, // data: StorageKey location
	lskAudios = 0x06, // data: StorageKey location
	lskRecentStickersOld = 0x07, // no data
	lskBackground = 0x08, // no data
	lskUserSettings = 0x09, // no data
	lskRecentHashtagsAndBots = 0x0a, // no data
	lskStickersOld = 0x0b, // no data
	lskSavedPeers = 0x0c, // no data
	lskReportSpamStatuses = 0x0d, // no data
	lskSavedGifsOld = 0x0e, // no data
	lskSavedGifs = 0x0f, // no data
	lskStickersKeys = 0x10, // no data
	lskTrustedBots = 0x11, // no data
	lskFavedStickers = 0x12, // no data
};

enum {
	dbiKey = 0x00,
	dbiUser = 0x01,
	dbiDcOptionOldOld = 0x02,
	dbiChatSizeMax = 0x03,
	dbiMutePeer = 0x04,
	dbiSendKey = 0x05,
	dbiAutoStart = 0x06,
	dbiStartMinimized = 0x07,
	dbiSoundNotify = 0x08,
	dbiWorkMode = 0x09,
	dbiSeenTrayTooltip = 0x0a,
	dbiDesktopNotify = 0x0b,
	dbiAutoUpdate = 0x0c,
	dbiLastUpdateCheck = 0x0d,
	dbiWindowPosition = 0x0e,
	dbiConnectionTypeOld = 0x0f,
	// 0x10 reserved
	dbiDefaultAttach = 0x11,
	dbiCatsAndDogs = 0x12,
	dbiReplaceEmojis = 0x13,
	dbiAskDownloadPath = 0x14,
	dbiDownloadPathOld = 0x15,
	dbiScale = 0x16,
	dbiEmojiTabOld = 0x17,
	dbiRecentEmojiOldOld = 0x18,
	dbiLoggedPhoneNumber = 0x19,
	dbiMutedPeers = 0x1a,
	// 0x1b reserved
	dbiNotifyView = 0x1c,
	dbiSendToMenu = 0x1d,
	dbiCompressPastedImage = 0x1e,
	dbiLangOld = 0x1f,
	dbiLangFileOld = 0x20,
	dbiTileBackground = 0x21,
	dbiAutoLock = 0x22,
	dbiDialogLastPath = 0x23,
	dbiRecentEmojiOld = 0x24,
	dbiEmojiVariantsOld = 0x25,
	dbiRecentStickers = 0x26,
	dbiDcOptionOld = 0x27,
	dbiTryIPv6 = 0x28,
	dbiSongVolume = 0x29,
	dbiWindowsNotificationsOld = 0x30,
	dbiIncludeMuted = 0x31,
	dbiMegagroupSizeMax = 0x32,
	dbiDownloadPath = 0x33,
	dbiAutoDownload = 0x34,
	dbiSavedGifsLimit = 0x35,
	dbiShowingSavedGifsOld = 0x36,
	dbiAutoPlay = 0x37,
	dbiAdaptiveForWide = 0x38,
	dbiHiddenPinnedMessages = 0x39,
	dbiRecentEmoji = 0x3a,
	dbiEmojiVariants = 0x3b,
	dbiDialogsMode = 0x40,
	dbiModerateMode = 0x41,
	dbiVideoVolume = 0x42,
	dbiStickersRecentLimit = 0x43,
	dbiNativeNotifications = 0x44,
	dbiNotificationsCount  = 0x45,
	dbiNotificationsCorner = 0x46,
	dbiThemeKey = 0x47,
	dbiDialogsWidthRatio = 0x48,
	dbiUseExternalVideoPlayer = 0x49,
	dbiDcOptions = 0x4a,
	dbiMtpAuthorization = 0x4b,
	dbiLastSeenWarningSeenOld = 0x4c,
	dbiAuthSessionData = 0x4d,
	dbiLangPackKey = 0x4e,
	dbiConnectionType = 0x4f,
	dbiStickersFavedLimit = 0x50,

	dbiEncryptedWithSalt = 333,
	dbiEncrypted = 444,

	// 500-600 reserved

	dbiVersion = 666,
};


typedef QMap<PeerId, FileKey> DraftsMap;
DraftsMap _draftsMap, _draftCursorsMap;
typedef QMap<PeerId, bool> DraftsNotReadMap;
DraftsNotReadMap _draftsNotReadMap;

typedef QPair<FileKey, int32_t> FileDesc; // file, size

typedef QMultiMap<MediaKey, FileLocation> FileLocations;
FileLocations _fileLocations;
typedef QPair<MediaKey, FileLocation> FileLocationPair;
typedef QMap<QString, FileLocationPair> FileLocationPairs;
FileLocationPairs _fileLocationPairs;
typedef QMap<MediaKey, MediaKey> FileLocationAliases;
FileLocationAliases _fileLocationAliases;
typedef QMap<QString, FileDesc> WebFilesMap;
WebFilesMap _webFilesMap;
uint64_t _storageWebFilesSize = 0;
FileKey _locationsKey = 0, _reportSpamStatusesKey = 0, _trustedBotsKey = 0;

using TrustedBots = OrderedSet<uint64_t>;
TrustedBots _trustedBots;
bool _trustedBotsRead = false;

FileKey _recentStickersKeyOld = 0;
FileKey _installedStickersKey = 0, _featuredStickersKey = 0, _recentStickersKey = 0, _favedStickersKey = 0, _archivedStickersKey = 0;
FileKey _savedGifsKey = 0;

FileKey _backgroundKey = 0;
bool _backgroundWasRead = false;
bool _backgroundCanWrite = true;

FileKey _themeKey = 0;
QString _themeAbsolutePath;
QString _themePaletteAbsolutePath;

bool _readingUserSettings = false;
FileKey _userSettingsKey = 0;
FileKey _recentHashtagsAndBotsKey = 0;
bool _recentHashtagsAndBotsWereRead = false;

FileKey _savedPeersKey = 0;
FileKey _langPackKey = 0;

typedef QMap<StorageKey, FileDesc> StorageMap;
StorageMap _imagesMap, _stickerImagesMap, _audiosMap;
int32_t _storageImagesSize = 0, _storageStickersSize = 0, _storageAudiosSize = 0;

bool _mapChanged = false;
int32_t _oldMapVersion = 0, _oldSettingsVersion = 0;

enum class WriteMapWhen {
	Now,
	Fast,
	Soon,
};

std::unique_ptr<StoredAuthSession> StoredAuthSessionCache;
StoredAuthSession &GetStoredAuthSessionCache() {
	if (!StoredAuthSessionCache) {
		StoredAuthSessionCache = std::make_unique<StoredAuthSession>();
	}
	return *StoredAuthSessionCache;
}

void _writeMap(WriteMapWhen when = WriteMapWhen::Soon);

void _writeLocations(WriteMapWhen when = WriteMapWhen::Soon) {
	if (when != WriteMapWhen::Now) {
		_manager->writeLocations(when == WriteMapWhen::Fast);
		return;
	}
	if (!_working()) return;

	_manager->writingLocations();
	if (_fileLocations.isEmpty() && _webFilesMap.isEmpty()) {
		if (_locationsKey) {
			clearKey(_locationsKey);
			_locationsKey = 0;
			_mapChanged = true;
			_writeMap();
		}
	} else {
		if (!_locationsKey) {
			_locationsKey = genKey();
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}
		uint32_t size = 0;
		for (FileLocations::const_iterator i = _fileLocations.cbegin(), e = _fileLocations.cend(); i != e; ++i) {
			// location + type + namelen + name
			size += sizeof(uint64_t) * 2 + sizeof(uint32_t) + Serialize::stringSize(i.value().name());
			if (AppVersion > 9013) {
				// bookmark
				size += Serialize::bytearraySize(i.value().bookmark());
			}
			// date + size
			size += Serialize::dateTimeSize() + sizeof(uint32_t);
		}

		//end mark
		size += sizeof(uint64_t) * 2 + sizeof(uint32_t) + Serialize::stringSize(QString());
		if (AppVersion > 9013) {
			size += Serialize::bytearraySize(QByteArray());
		}
		size += Serialize::dateTimeSize() + sizeof(uint32_t);

		size += sizeof(uint32_t); // aliases count
		for (FileLocationAliases::const_iterator i = _fileLocationAliases.cbegin(), e = _fileLocationAliases.cend(); i != e; ++i) {
			// alias + location
			size += sizeof(uint64_t) * 2 + sizeof(uint64_t) * 2;
		}

		size += sizeof(uint32_t); // web files count
		for (WebFilesMap::const_iterator i = _webFilesMap.cbegin(), e = _webFilesMap.cend(); i != e; ++i) {
			// url + filekey + size
			size += Serialize::stringSize(i.key()) + sizeof(uint64_t) + sizeof(int32_t);
		}

		EncryptedDescriptor data(size);
		auto legacyTypeField = 0;
		for (FileLocations::const_iterator i = _fileLocations.cbegin(); i != _fileLocations.cend(); ++i) {
			data.stream << uint64_t(i.key().first) << uint64_t(i.key().second) << uint32_t(legacyTypeField) << i.value().name();
			if (AppVersion > 9013) {
				data.stream << i.value().bookmark();
			}
			data.stream << i.value().modified << uint32_t(i.value().size);
		}

		data.stream << uint64_t(0) << uint64_t(0) << uint32_t(0) << QString();
		if (AppVersion > 9013) {
			data.stream << QByteArray();
		}
		data.stream << QDateTime::currentDateTime() << uint32_t(0);

		data.stream << uint32_t(_fileLocationAliases.size());
		for (FileLocationAliases::const_iterator i = _fileLocationAliases.cbegin(), e = _fileLocationAliases.cend(); i != e; ++i) {
			data.stream << uint64_t(i.key().first) << uint64_t(i.key().second) << uint64_t(i.value().first) << uint64_t(i.value().second);
		}

		data.stream << uint32_t(_webFilesMap.size());
		for (WebFilesMap::const_iterator i = _webFilesMap.cbegin(), e = _webFilesMap.cend(); i != e; ++i) {
			data.stream << i.key() << uint64_t(i.value().first) << int32_t(i.value().second);
		}

		FileWriteDescriptor file(_locationsKey);
		file.writeEncrypted(data);
	}
}

void _readLocations() {
	FileReadDescriptor locations;
	if (!readEncryptedFile(locations, _locationsKey)) {
		clearKey(_locationsKey);
		_locationsKey = 0;
		_writeMap();
		return;
	}

	bool endMarkFound = false;
	while (!locations.stream.atEnd()) {
		uint64_t first, second;
		QByteArray bookmark;
		FileLocation loc;
		uint32_t legacyTypeField = 0;
		locations.stream >> first >> second >> legacyTypeField >> loc.fname;
		if (locations.version > 9013) {
			locations.stream >> bookmark;
		}
		locations.stream >> loc.modified >> loc.size;
		loc.setBookmark(bookmark);

		if (!first && !second && !legacyTypeField && loc.fname.isEmpty() && !loc.size) { // end mark
			endMarkFound = true;
			break;
		}

		MediaKey key(first, second);

		_fileLocations.insert(key, loc);
		_fileLocationPairs.insert(loc.fname, FileLocationPair(key, loc));
	}

	if (endMarkFound) {
		uint32_t cnt;
		locations.stream >> cnt;
		for (uint32_t i = 0; i < cnt; ++i) {
			uint64_t kfirst, ksecond, vfirst, vsecond;
			locations.stream >> kfirst >> ksecond >> vfirst >> vsecond;
			_fileLocationAliases.insert(MediaKey(kfirst, ksecond), MediaKey(vfirst, vsecond));
		}

		if (!locations.stream.atEnd()) {
			_storageWebFilesSize = 0;
			_webFilesMap.clear();

			uint32_t webLocationsCount;
			locations.stream >> webLocationsCount;
			for (uint32_t i = 0; i < webLocationsCount; ++i) {
				QString url;
				uint64_t key;
				int32_t size;
				locations.stream >> url >> key >> size;
				_webFilesMap.insert(url, FileDesc(key, size));
				_storageWebFilesSize += size;
			}
		}
	}
}

void _writeReportSpamStatuses() {
	if (!_working()) return;

	if (cReportSpamStatuses().isEmpty()) {
		if (_reportSpamStatusesKey) {
			clearKey(_reportSpamStatusesKey);
			_reportSpamStatusesKey = 0;
			_mapChanged = true;
			_writeMap();
		}
	} else {
		if (!_reportSpamStatusesKey) {
			_reportSpamStatusesKey = genKey();
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}
		const ReportSpamStatuses &statuses(cReportSpamStatuses());

		uint32_t size = sizeof(int32_t);
		for (ReportSpamStatuses::const_iterator i = statuses.cbegin(), e = statuses.cend(); i != e; ++i) {
			// peer + status
			size += sizeof(uint64_t) + sizeof(int32_t);
		}

		EncryptedDescriptor data(size);
		data.stream << int32_t(statuses.size());
		for (ReportSpamStatuses::const_iterator i = statuses.cbegin(), e = statuses.cend(); i != e; ++i) {
			data.stream << uint64_t(i.key()) << int32_t(i.value());
		}

		FileWriteDescriptor file(_reportSpamStatusesKey);
		file.writeEncrypted(data);
	}
}

void _readReportSpamStatuses() {
	FileReadDescriptor statuses;
	if (!readEncryptedFile(statuses, _reportSpamStatusesKey)) {
		clearKey(_reportSpamStatusesKey);
		_reportSpamStatusesKey = 0;
		_writeMap();
		return;
	}

	ReportSpamStatuses &map(cRefReportSpamStatuses());
	map.clear();

	int32_t size = 0;
	statuses.stream >> size;
	for (int32_t i = 0; i < size; ++i) {
		uint64_t peer = 0;
		int32_t status = 0;
		statuses.stream >> peer >> status;
		map.insert(peer, DBIPeerReportSpamStatus(status));
	}
}

struct ReadSettingsContext {
	int legacyLanguageId = Lang::kLegacyLanguageNone;
	QString legacyLanguageFile;
	MTP::DcOptions dcOptions;
};

void applyReadContext(ReadSettingsContext &&context) {
	Messenger::Instance().dcOptions()->addFromOther(std::move(context.dcOptions));
	if (context.legacyLanguageId != Lang::kLegacyLanguageNone) {
		Lang::Current().fillFromLegacy(context.legacyLanguageId, context.legacyLanguageFile);
		writeLangPack();
	}
}

bool _readSetting(uint32_t blockId, QDataStream &stream, int version, ReadSettingsContext &context) {
	switch (blockId) {
	case dbiDcOptionOldOld: {
		uint32_t dcId, port;
		QString host, ip;
		stream >> dcId >> host >> ip >> port;
		if (!_checkStreamStatus(stream)) return false;

		context.dcOptions.constructAddOne(dcId, 0, ip.toStdString(), port);
	} break;

	case dbiDcOptionOld: {
		uint32_t dcIdWithShift, port;
		int32_t flags;
		QString ip;
		stream >> dcIdWithShift >> flags >> ip >> port;
		if (!_checkStreamStatus(stream)) return false;

		context.dcOptions.constructAddOne(dcIdWithShift, MTPDdcOption::Flags(flags), ip.toStdString(), port);
	} break;

	case dbiDcOptions: {
		auto serialized = QByteArray();
		stream >> serialized;
		if (!_checkStreamStatus(stream)) return false;

		context.dcOptions.constructFromSerialized(serialized);
	} break;

	case dbiChatSizeMax: {
		int32_t maxSize;
		stream >> maxSize;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetChatSizeMax(maxSize);
	} break;

	case dbiSavedGifsLimit: {
		int32_t limit;
		stream >> limit;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetSavedGifsLimit(limit);
	} break;

	case dbiStickersRecentLimit: {
		int32_t limit;
		stream >> limit;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetStickersRecentLimit(limit);
	} break;

	case dbiStickersFavedLimit: {
		int32_t limit;
		stream >> limit;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetStickersFavedLimit(limit);
	} break;

	case dbiMegagroupSizeMax: {
		int32_t maxSize;
		stream >> maxSize;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetMegagroupSizeMax(maxSize);
	} break;

	case dbiUser: {
		uint32_t dcId;
		int32_t userId;
		stream >> userId >> dcId;
		if (!_checkStreamStatus(stream)) return false;

		DEBUG_LOG(("MTP Info: user found, dc %1, uid %2").arg(dcId).arg(userId));
		Messenger::Instance().setMtpMainDcId(dcId);
		Messenger::Instance().setAuthSessionUserId(userId);
	} break;

	case dbiKey: {
		int32_t dcId;
		stream >> dcId;
		auto key = Serialize::read<MTP::AuthKey::Data>(stream);
		if (!_checkStreamStatus(stream)) return false;

		Messenger::Instance().setMtpKey(dcId, key);
	} break;

	case dbiMtpAuthorization: {
		auto serialized = QByteArray();
		stream >> serialized;
		if (!_checkStreamStatus(stream)) return false;

		Messenger::Instance().setMtpAuthorization(serialized);
	} break;

	case dbiAutoStart: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetAutoStart(v == 1);
	} break;

	case dbiStartMinimized: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetStartMinimized(v == 1);
	} break;

	case dbiSendToMenu: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetSendToMenu(v == 1);
	} break;

	case dbiUseExternalVideoPlayer: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetUseExternalVideoPlayer(v == 1);
	} break;

	case dbiSoundNotify: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetSoundNotify(v == 1);
	} break;

	case dbiAutoDownload: {
		int32_t photo, audio, gif;
		stream >> photo >> audio >> gif;
		if (!_checkStreamStatus(stream)) return false;

		cSetAutoDownloadPhoto(photo);
		cSetAutoDownloadAudio(audio);
		cSetAutoDownloadGif(gif);
	} break;

	case dbiAutoPlay: {
		int32_t gif;
		stream >> gif;
		if (!_checkStreamStatus(stream)) return false;

		cSetAutoPlayGif(gif == 1);
	} break;

	case dbiDialogsMode: {
		int32_t enabled, modeInt;
		stream >> enabled >> modeInt;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetDialogsModeEnabled(enabled == 1);
		auto mode = Dialogs::Mode::All;
		if (enabled) {
			mode = static_cast<Dialogs::Mode>(modeInt);
			if (mode != Dialogs::Mode::All && mode != Dialogs::Mode::Important) {
				mode = Dialogs::Mode::All;
			}
		}
		Global::SetDialogsMode(mode);
	} break;

	case dbiModerateMode: {
		int32_t enabled;
		stream >> enabled;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetModerateModeEnabled(enabled == 1);
	} break;

	case dbiIncludeMuted: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetIncludeMuted(v == 1);
	} break;

	case dbiShowingSavedGifsOld: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;
	} break;

	case dbiDesktopNotify: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetDesktopNotify(v == 1);
		if (App::wnd()) App::wnd()->updateTrayMenu();
	} break;

	case dbiWindowsNotificationsOld: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;
	} break;

	case dbiNativeNotifications: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetNativeNotifications(v == 1);
	} break;

	case dbiNotificationsCount: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetNotificationsCount((v > 0 ? v : 3));
	} break;

	case dbiNotificationsCorner: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetNotificationsCorner(static_cast<Notify::ScreenCorner>((v >= 0 && v < 4) ? v : 2));
	} break;

	case dbiDialogsWidthRatio: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		GetStoredAuthSessionCache().dialogsWidthRatio = v / 1000000.;
	} break;

	case dbiLastSeenWarningSeenOld: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		GetStoredAuthSessionCache().data.setLastSeenWarningSeen(v == 1);
	} break;

	case dbiAuthSessionData: {
		QByteArray v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		GetStoredAuthSessionCache().data.constructFromSerialized(v);
	} break;

	case dbiWorkMode: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		auto newMode = [v] {
			switch (v) {
			case dbiwmTrayOnly: return dbiwmTrayOnly;
			case dbiwmWindowOnly: return dbiwmWindowOnly;
			};
			return dbiwmWindowAndTray;
		};
		Global::RefWorkMode().set(newMode());
	} break;

	case dbiConnectionTypeOld: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		switch (v) {
		case dbictHttpProxy:
		case dbictTcpProxy: {
			ProxyData p;
			int32_t port;
			stream >> p.host >> port >> p.user >> p.password;
			if (!_checkStreamStatus(stream)) return false;

			p.port = uint32_t(port);
			Global::SetConnectionProxy(p);
			Global::SetConnectionType(DBIConnectionType(v));
		} break;
		case dbictHttpAuto:
		default: Global::SetConnectionType(dbictAuto); break;
		};
		Global::SetLastProxyType(Global::ConnectionType());
	} break;

	case dbiConnectionType: {
		ProxyData p;
		int32_t connectionType, lastProxyType, port;
		stream >> connectionType >> lastProxyType >> p.host >> port >> p.user >> p.password;
		if (!_checkStreamStatus(stream)) return false;

		p.port = port;
		switch (connectionType) {
		case dbictHttpProxy:
		case dbictTcpProxy: {
			Global::SetConnectionType(DBIConnectionType(lastProxyType));
		} break;
		case dbictHttpAuto:
		default: Global::SetConnectionType(dbictAuto); break;
		};
		switch (lastProxyType) {
		case dbictHttpProxy:
		case dbictTcpProxy: {
			Global::SetLastProxyType(DBIConnectionType(lastProxyType));
			Global::SetConnectionProxy(p);
		} break;
		case dbictHttpAuto:
		default: {
			Global::SetLastProxyType(dbictAuto);
			Global::SetConnectionProxy(ProxyData());
		} break;
		}
	} break;

	case dbiThemeKey: {
		uint64_t themeKey = 0;
		stream >> themeKey;
		if (!_checkStreamStatus(stream)) return false;

		_themeKey = themeKey;
	} break;

	case dbiLangPackKey: {
		uint64_t langPackKey = 0;
		stream >> langPackKey;
		if (!_checkStreamStatus(stream)) return false;

		_langPackKey = langPackKey;
	} break;

	case dbiTryIPv6: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetTryIPv6(v == 1);
	} break;

	case dbiSeenTrayTooltip: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetSeenTrayTooltip(v == 1);
	} break;

	case dbiAutoUpdate: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetAutoUpdate(v == 1);
#ifndef TDESKTOP_DISABLE_AUTOUPDATE
		if (!cAutoUpdate()) {
			Sandbox::stopUpdate();
		}
#endif // !TDESKTOP_DISABLE_AUTOUPDATE
	} break;

	case dbiLastUpdateCheck: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetLastUpdateCheck(v);
	} break;

	case dbiScale: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		DBIScale s = cRealScale();
		switch (v) {
		case dbisAuto: s = dbisAuto; break;
		case dbisOne: s = dbisOne; break;
		case dbisOneAndQuarter: s = dbisOneAndQuarter; break;
		case dbisOneAndHalf: s = dbisOneAndHalf; break;
		case dbisTwo: s = dbisTwo; break;
		}
		if (cRetina()) s = dbisOne;
		cSetConfigScale(s);
		cSetRealScale(s);
	} break;

	case dbiLangOld: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		context.legacyLanguageId = v;
	} break;

	case dbiLangFileOld: {
		QString v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		context.legacyLanguageFile = v;
	} break;

	case dbiWindowPosition: {
		auto position = TWindowPos();
		stream >> position.x >> position.y >> position.w >> position.h;
		stream >> position.moncrc >> position.maximized;
		if (!_checkStreamStatus(stream)) return false;

		DEBUG_LOG(("Window Pos: Read from storage %1, %2, %3, %4 (maximized %5)").arg(position.x).arg(position.y).arg(position.w).arg(position.h).arg(Logs::b(position.maximized)));
		cSetWindowPos(position);
	} break;

	case dbiLoggedPhoneNumber: {
		QString v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetLoggedPhoneNumber(v);
	} break;

	case dbiMutePeer: { // deprecated
		uint64_t peerId;
		stream >> peerId;
		if (!_checkStreamStatus(stream)) return false;
	} break;

	case dbiMutedPeers: { // deprecated
		uint32_t count;
		stream >> count;
		if (!_checkStreamStatus(stream)) return false;

		for (uint32_t i = 0; i < count; ++i) {
			uint64_t peerId;
			stream >> peerId;
		}
		if (!_checkStreamStatus(stream)) return false;
	} break;

	case dbiSendKey: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetCtrlEnter(v == dbiskCtrlEnter);
		if (App::main()) App::main()->ctrlEnterSubmitUpdated();
	} break;

	case dbiCatsAndDogs: { // deprecated
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;
	} break;

	case dbiTileBackground: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		bool tile = (version < 8005 && !_backgroundKey) ? false : (v == 1);
		Window::Theme::Background()->setTile(tile);
	} break;

	case dbiAdaptiveForWide: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetAdaptiveForWide(v == 1);
	} break;

	case dbiAutoLock: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetAutoLock(v);
		Global::RefLocalPasscodeChanged().notify();
	} break;

	case dbiReplaceEmojis: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetReplaceEmojis(v == 1);
	} break;

	case dbiDefaultAttach: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;
	} break;

	case dbiNotifyView: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		switch (v) {
		case dbinvShowNothing: Global::SetNotifyView(dbinvShowNothing); break;
		case dbinvShowName: Global::SetNotifyView(dbinvShowName); break;
		default: Global::SetNotifyView(dbinvShowPreview); break;
		}
	} break;

	case dbiAskDownloadPath: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetAskDownloadPath(v == 1);
	} break;

	case dbiDownloadPathOld: {
		QString v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;
#ifndef OS_WIN_STORE
		if (!v.isEmpty() && v != qstr("tmp") && !v.endsWith('/')) v += '/';
		Global::SetDownloadPath(v);
		Global::SetDownloadPathBookmark(QByteArray());
		Global::RefDownloadPathChanged().notify();
#endif // OS_WIN_STORE
	} break;

	case dbiDownloadPath: {
		QString v;
		QByteArray bookmark;
		stream >> v >> bookmark;
		if (!_checkStreamStatus(stream)) return false;
#ifndef OS_WIN_STORE
		if (!v.isEmpty() && v != qstr("tmp") && !v.endsWith('/')) v += '/';
		Global::SetDownloadPath(v);
		Global::SetDownloadPathBookmark(bookmark);
		psDownloadPathEnableAccess();
		Global::RefDownloadPathChanged().notify();
#endif // OS_WIN_STORE
	} break;

	case dbiCompressPastedImage: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetCompressPastedImage(v == 1);
	} break;

	case dbiEmojiTabOld: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		// deprecated
	} break;

	case dbiRecentEmojiOldOld: {
		RecentEmojiPreloadOldOld v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		if (!v.isEmpty()) {
			RecentEmojiPreload p;
			p.reserve(v.size());
			for (auto &item : v) {
				auto oldKey = uint64_t(item.first);
				switch (oldKey) {
				case 0xD83CDDEFLLU: oldKey = 0xD83CDDEFD83CDDF5LLU; break;
				case 0xD83CDDF0LLU: oldKey = 0xD83CDDF0D83CDDF7LLU; break;
				case 0xD83CDDE9LLU: oldKey = 0xD83CDDE9D83CDDEALLU; break;
				case 0xD83CDDE8LLU: oldKey = 0xD83CDDE8D83CDDF3LLU; break;
				case 0xD83CDDFALLU: oldKey = 0xD83CDDFAD83CDDF8LLU; break;
				case 0xD83CDDEBLLU: oldKey = 0xD83CDDEBD83CDDF7LLU; break;
				case 0xD83CDDEALLU: oldKey = 0xD83CDDEAD83CDDF8LLU; break;
				case 0xD83CDDEELLU: oldKey = 0xD83CDDEED83CDDF9LLU; break;
				case 0xD83CDDF7LLU: oldKey = 0xD83CDDF7D83CDDFALLU; break;
				case 0xD83CDDECLLU: oldKey = 0xD83CDDECD83CDDE7LLU; break;
				}
				auto id = Ui::Emoji::IdFromOldKey(oldKey);
				if (!id.isEmpty()) {
					p.push_back(qMakePair(id, item.second));
				}
			}
			cSetRecentEmojiPreload(p);
		}
	} break;

	case dbiRecentEmojiOld: {
		RecentEmojiPreloadOld v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		if (!v.isEmpty()) {
			RecentEmojiPreload p;
			p.reserve(v.size());
			for (auto &item : v) {
				auto id = Ui::Emoji::IdFromOldKey(item.first);
				if (!id.isEmpty()) {
					p.push_back(qMakePair(id, item.second));
				}
			}
			cSetRecentEmojiPreload(p);
		}
	} break;

	case dbiRecentEmoji: {
		RecentEmojiPreload v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetRecentEmojiPreload(v);
	} break;

	case dbiRecentStickers: {
		RecentStickerPreload v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetRecentStickersPreload(v);
	} break;

	case dbiEmojiVariantsOld: {
		EmojiColorVariantsOld v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		EmojiColorVariants variants;
		for (auto i = v.cbegin(), e = v.cend(); i != e; ++i) {
			auto id = Ui::Emoji::IdFromOldKey(static_cast<uint64_t>(i.key()));
			if (!id.isEmpty()) {
				auto index = Ui::Emoji::ColorIndexFromOldKey(i.value());
				if (index >= 0) {
					variants.insert(id, index);
				}
			}
		}
		cSetEmojiVariants(variants);
	} break;

	case dbiEmojiVariants: {
		EmojiColorVariants v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		cSetEmojiVariants(v);
	} break;

	case dbiHiddenPinnedMessages: {
		Global::HiddenPinnedMessagesMap v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetHiddenPinnedMessages(v);
	} break;

	case dbiDialogLastPath: {
		QString path;
		stream >> path;
		if (!_checkStreamStatus(stream)) return false;

		cSetDialogLastPath(path);
	} break;

	case dbiSongVolume: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetSongVolume(snap(v / 1e6, 0., 1.));
	} break;

	case dbiVideoVolume: {
		int32_t v;
		stream >> v;
		if (!_checkStreamStatus(stream)) return false;

		Global::SetVideoVolume(snap(v / 1e6, 0., 1.));
	} break;

	default:
	LOG(("App Error: unknown blockId in _readSetting: %1").arg(blockId));
	return false;
	}

	return true;
}

bool _readOldSettings(bool remove, ReadSettingsContext &context) {
	bool result = false;
	QFile file(cWorkingDir() + qsl("tdata/config"));
	if (file.open(QIODevice::ReadOnly)) {
		LOG(("App Info: reading old config..."));
		QDataStream stream(&file);
		stream.setVersion(QDataStream::Qt_5_1);

		int32_t version = 0;
		while (!stream.atEnd()) {
			uint32_t blockId;
			stream >> blockId;
			if (!_checkStreamStatus(stream)) break;

			if (blockId == dbiVersion) {
				stream >> version;
				if (!_checkStreamStatus(stream)) break;

				if (version > AppVersion) break;
			} else if (!_readSetting(blockId, stream, version, context)) {
				break;
			}
		}
		file.close();
		result = true;
	}
	if (remove) file.remove();
	return result;
}

void _readOldUserSettingsFields(QIODevice *device, int32_t &version, ReadSettingsContext &context) {
	QDataStream stream(device);
	stream.setVersion(QDataStream::Qt_5_1);

	while (!stream.atEnd()) {
		uint32_t blockId;
		stream >> blockId;
		if (!_checkStreamStatus(stream)) {
			break;
		}

		if (blockId == dbiVersion) {
			stream >> version;
			if (!_checkStreamStatus(stream)) {
				break;
			}

			if (version > AppVersion) return;
		} else if (blockId == dbiEncryptedWithSalt) {
			QByteArray salt, data, decrypted;
			stream >> salt >> data;
			if (!_checkStreamStatus(stream)) {
				break;
			}

			if (salt.size() != 32) {
				LOG(("App Error: bad salt in old user config encrypted part, size: %1").arg(salt.size()));
				continue;
			}

			createLocalKey(QByteArray(), &salt, &OldKey);

			if (data.size() <= 16 || (data.size() & 0x0F)) {
				LOG(("App Error: bad encrypted part size in old user config: %1").arg(data.size()));
				continue;
			}
			uint32_t fullDataLen = data.size() - 16;
			decrypted.resize(fullDataLen);
			const char *dataKey = data.constData(), *encrypted = data.constData() + 16;
			aesDecryptLocal(encrypted, decrypted.data(), fullDataLen, OldKey, dataKey);
			uchar sha1Buffer[20];
			if (memcmp(hashSha1(decrypted.constData(), decrypted.size(), sha1Buffer), dataKey, 16)) {
				LOG(("App Error: bad decrypt key, data from old user config not decrypted"));
				continue;
			}
			uint32_t dataLen = *(const uint32_t*)decrypted.constData();
			if (dataLen > uint32_t(decrypted.size()) || dataLen <= fullDataLen - 16 || dataLen < 4) {
				LOG(("App Error: bad decrypted part size in old user config: %1, fullDataLen: %2, decrypted size: %3").arg(dataLen).arg(fullDataLen).arg(decrypted.size()));
				continue;
			}
			decrypted.resize(dataLen);
			QBuffer decryptedStream(&decrypted);
			decryptedStream.open(QIODevice::ReadOnly);
			decryptedStream.seek(4); // skip size
			LOG(("App Info: reading encrypted old user config..."));

			_readOldUserSettingsFields(&decryptedStream, version, context);
		} else if (!_readSetting(blockId, stream, version, context)) {
			return;
		}
	}
}

bool _readOldUserSettings(bool remove, ReadSettingsContext &context) {
	bool result = false;
	QFile file(cWorkingDir() + cDataFile() + (cTestMode() ? qsl("_test") : QString()) + qsl("_config"));
	if (file.open(QIODevice::ReadOnly)) {
		LOG(("App Info: reading old user config..."));
		int32_t version = 0;
		_readOldUserSettingsFields(&file, version, context);
		file.close();
		result = true;
	}
	if (remove) file.remove();
	return result;
}

void _readOldMtpDataFields(QIODevice *device, int32_t &version, ReadSettingsContext &context) {
	QDataStream stream(device);
	stream.setVersion(QDataStream::Qt_5_1);

	while (!stream.atEnd()) {
		uint32_t blockId;
		stream >> blockId;
		if (!_checkStreamStatus(stream)) {
			break;
		}

		if (blockId == dbiVersion) {
			stream >> version;
			if (!_checkStreamStatus(stream)) {
				break;
			}

			if (version > AppVersion) return;
		} else if (blockId == dbiEncrypted) {
			QByteArray data, decrypted;
			stream >> data;
			if (!_checkStreamStatus(stream)) {
				break;
			}

			if (!OldKey) {
				LOG(("MTP Error: reading old encrypted keys without old key!"));
				continue;
			}

			if (data.size() <= 16 || (data.size() & 0x0F)) {
				LOG(("MTP Error: bad encrypted part size in old keys: %1").arg(data.size()));
				continue;
			}
			uint32_t fullDataLen = data.size() - 16;
			decrypted.resize(fullDataLen);
			const char *dataKey = data.constData(), *encrypted = data.constData() + 16;
			aesDecryptLocal(encrypted, decrypted.data(), fullDataLen, OldKey, dataKey);
			uchar sha1Buffer[20];
			if (memcmp(hashSha1(decrypted.constData(), decrypted.size(), sha1Buffer), dataKey, 16)) {
				LOG(("MTP Error: bad decrypt key, data from old keys not decrypted"));
				continue;
			}
			uint32_t dataLen = *(const uint32_t*)decrypted.constData();
			if (dataLen > uint32_t(decrypted.size()) || dataLen <= fullDataLen - 16 || dataLen < 4) {
				LOG(("MTP Error: bad decrypted part size in old keys: %1, fullDataLen: %2, decrypted size: %3").arg(dataLen).arg(fullDataLen).arg(decrypted.size()));
				continue;
			}
			decrypted.resize(dataLen);
			QBuffer decryptedStream(&decrypted);
			decryptedStream.open(QIODevice::ReadOnly);
			decryptedStream.seek(4); // skip size
			LOG(("App Info: reading encrypted old keys..."));

			_readOldMtpDataFields(&decryptedStream, version, context);
		} else if (!_readSetting(blockId, stream, version, context)) {
			return;
		}
	}
}

bool _readOldMtpData(bool remove, ReadSettingsContext &context) {
	bool result = false;
	QFile file(cWorkingDir() + cDataFile() + (cTestMode() ? qsl("_test") : QString()));
	if (file.open(QIODevice::ReadOnly)) {
		LOG(("App Info: reading old keys..."));
		int32_t version = 0;
		_readOldMtpDataFields(&file, version, context);
		file.close();
		result = true;
	}
	if (remove) file.remove();
	return result;
}

void _writeUserSettings() {
	if (_readingUserSettings) {
		LOG(("App Error: attempt to write settings while reading them!"));
		return;
	}
	LOG(("App Info: writing encrypted user settings..."));

	if (!_userSettingsKey) {
		_userSettingsKey = genKey();
		_mapChanged = true;
		_writeMap(WriteMapWhen::Fast);
	}

	auto recentEmojiPreloadData = cRecentEmojiPreload();
	if (recentEmojiPreloadData.isEmpty()) {
		recentEmojiPreloadData.reserve(Ui::Emoji::GetRecent().size());
		for (auto &item : Ui::Emoji::GetRecent()) {
			recentEmojiPreloadData.push_back(qMakePair(item.first->id(), item.second));
		}
	}
	auto userDataInstance = StoredAuthSessionCache ? &StoredAuthSessionCache->data : Messenger::Instance().getAuthSessionData();
	auto userData = userDataInstance ? userDataInstance->serialize() : QByteArray();
	auto dialogsWidthRatio = [] {
		if (StoredAuthSessionCache) {
			return StoredAuthSessionCache->dialogsWidthRatio;
		} else if (auto window = App::wnd()) {
			if (auto controller = window->controller()) {
				return controller->dialogsWidthRatio().value();
			}
		}
		return Window::Controller::kDefaultDialogsWidthRatio;
	};

	uint32_t size = 21 * (sizeof(uint32_t) + sizeof(int32_t));
	size += sizeof(uint32_t) + Serialize::stringSize(Global::AskDownloadPath() ? QString() : Global::DownloadPath()) + Serialize::bytearraySize(Global::AskDownloadPath() ? QByteArray() : Global::DownloadPathBookmark());

	size += sizeof(uint32_t) + sizeof(int32_t);
	for (auto &item : recentEmojiPreloadData) {
		size += Serialize::stringSize(item.first) + sizeof(item.second);
	}

	size += sizeof(uint32_t) + sizeof(int32_t) + cEmojiVariants().size() * (sizeof(uint32_t) + sizeof(uint64_t));
	size += sizeof(uint32_t) + sizeof(int32_t) + (cRecentStickersPreload().isEmpty() ? cGetRecentStickers().size() : cRecentStickersPreload().size()) * (sizeof(uint64_t) + sizeof(ushort));
	size += sizeof(uint32_t) + Serialize::stringSize(cDialogLastPath());
	size += sizeof(uint32_t) + 3 * sizeof(int32_t);
	size += sizeof(uint32_t) + 2 * sizeof(int32_t);
	if (!Global::HiddenPinnedMessages().isEmpty()) {
		size += sizeof(uint32_t) + sizeof(int32_t) + Global::HiddenPinnedMessages().size() * (sizeof(PeerId) + sizeof(MsgId));
	}
	if (!userData.isEmpty()) {
		size += sizeof(uint32_t) + Serialize::bytearraySize(userData);
	}

	EncryptedDescriptor data(size);
	data.stream << uint32_t(dbiSendKey) << int32_t(cCtrlEnter() ? dbiskCtrlEnter : dbiskEnter);
	data.stream << uint32_t(dbiTileBackground) << int32_t(Window::Theme::Background()->tileForSave() ? 1 : 0);
	data.stream << uint32_t(dbiAdaptiveForWide) << int32_t(Global::AdaptiveForWide() ? 1 : 0);
	data.stream << uint32_t(dbiAutoLock) << int32_t(Global::AutoLock());
	data.stream << uint32_t(dbiReplaceEmojis) << int32_t(cReplaceEmojis() ? 1 : 0);
	data.stream << uint32_t(dbiSoundNotify) << int32_t(Global::SoundNotify());
	data.stream << uint32_t(dbiIncludeMuted) << int32_t(Global::IncludeMuted());
	data.stream << uint32_t(dbiDesktopNotify) << int32_t(Global::DesktopNotify());
	data.stream << uint32_t(dbiNotifyView) << int32_t(Global::NotifyView());
	data.stream << uint32_t(dbiNativeNotifications) << int32_t(Global::NativeNotifications());
	data.stream << uint32_t(dbiNotificationsCount) << int32_t(Global::NotificationsCount());
	data.stream << uint32_t(dbiNotificationsCorner) << int32_t(Global::NotificationsCorner());
	data.stream << uint32_t(dbiAskDownloadPath) << int32_t(Global::AskDownloadPath());
	data.stream << uint32_t(dbiDownloadPath) << (Global::AskDownloadPath() ? QString() : Global::DownloadPath()) << (Global::AskDownloadPath() ? QByteArray() : Global::DownloadPathBookmark());
	data.stream << uint32_t(dbiCompressPastedImage) << int32_t(cCompressPastedImage());
	data.stream << uint32_t(dbiDialogLastPath) << cDialogLastPath();
	data.stream << uint32_t(dbiSongVolume) << int32_t(qRound(Global::SongVolume() * 1e6));
	data.stream << uint32_t(dbiVideoVolume) << int32_t(qRound(Global::VideoVolume() * 1e6));
	data.stream << uint32_t(dbiAutoDownload) << int32_t(cAutoDownloadPhoto()) << int32_t(cAutoDownloadAudio()) << int32_t(cAutoDownloadGif());
	data.stream << uint32_t(dbiDialogsMode) << int32_t(Global::DialogsModeEnabled() ? 1 : 0) << static_cast<int32_t>(Global::DialogsMode());
	data.stream << uint32_t(dbiModerateMode) << int32_t(Global::ModerateModeEnabled() ? 1 : 0);
	data.stream << uint32_t(dbiAutoPlay) << int32_t(cAutoPlayGif() ? 1 : 0);
	data.stream << uint32_t(dbiDialogsWidthRatio) << int32_t(snap(qRound(dialogsWidthRatio() * 1000000), 0, 1000000));
	data.stream << uint32_t(dbiUseExternalVideoPlayer) << int32_t(cUseExternalVideoPlayer());
	if (!userData.isEmpty()) {
		data.stream << uint32_t(dbiAuthSessionData) << userData;
	}

	{
		data.stream << uint32_t(dbiRecentEmoji) << recentEmojiPreloadData;
	}
	data.stream << uint32_t(dbiEmojiVariants) << cEmojiVariants();
	{
		RecentStickerPreload v(cRecentStickersPreload());
		if (v.isEmpty()) {
			v.reserve(cGetRecentStickers().size());
			for (RecentStickerPack::const_iterator i = cGetRecentStickers().cbegin(), e = cGetRecentStickers().cend(); i != e; ++i) {
				v.push_back(qMakePair(i->first->id, i->second));
			}
		}
		data.stream << uint32_t(dbiRecentStickers) << v;
	}
	if (!Global::HiddenPinnedMessages().isEmpty()) {
		data.stream << uint32_t(dbiHiddenPinnedMessages) << Global::HiddenPinnedMessages();
	}

	FileWriteDescriptor file(_userSettingsKey);
	file.writeEncrypted(data);
}

void _readUserSettings() {
	ReadSettingsContext context;
	FileReadDescriptor userSettings;
	if (!readEncryptedFile(userSettings, _userSettingsKey)) {
		LOG(("App Info: could not read encrypted user settings..."));

		_readOldUserSettings(true, context);
		applyReadContext(std::move(context));

		return _writeUserSettings();
	}

	LOG(("App Info: reading encrypted user settings..."));
	_readingUserSettings = true;
	while (!userSettings.stream.atEnd()) {
		uint32_t blockId;
		userSettings.stream >> blockId;
		if (!_checkStreamStatus(userSettings.stream)) {
			_readingUserSettings = false;
			return _writeUserSettings();
		}

		if (!_readSetting(blockId, userSettings.stream, userSettings.version, context)) {
			_readingUserSettings = false;
			return _writeUserSettings();
		}
	}
	_readingUserSettings = false;
	LOG(("App Info: encrypted user settings read."));

	applyReadContext(std::move(context));
}

void _writeMtpData() {
	FileWriteDescriptor mtp(toFilePart(_dataNameKey), FileOption::Safe);
	if (!LocalKey) {
		LOG(("App Error: localkey not created in _writeMtpData()"));
		return;
	}

	auto mtpAuthorizationSerialized = Messenger::Instance().serializeMtpAuthorization();

	uint32_t size = sizeof(uint32_t) + Serialize::bytearraySize(mtpAuthorizationSerialized);

	EncryptedDescriptor data(size);
	data.stream << uint32_t(dbiMtpAuthorization) << mtpAuthorizationSerialized;
	mtp.writeEncrypted(data);
}

void _readMtpData() {
	ReadSettingsContext context;
	FileReadDescriptor mtp;
	if (!readEncryptedFile(mtp, toFilePart(_dataNameKey), FileOption::Safe)) {
		if (LocalKey) {
			_readOldMtpData(true, context);
			applyReadContext(std::move(context));

			_writeMtpData();
		}
		return;
	}

	LOG(("App Info: reading encrypted mtp data..."));
	while (!mtp.stream.atEnd()) {
		uint32_t blockId;
		mtp.stream >> blockId;
		if (!_checkStreamStatus(mtp.stream)) {
			return _writeMtpData();
		}

		if (!_readSetting(blockId, mtp.stream, mtp.version, context)) {
			return _writeMtpData();
		}
	}
	applyReadContext(std::move(context));
}

ReadMapState _readMap(const QByteArray &pass) {
	auto ms = getms();
	QByteArray dataNameUtf8 = (cDataFile() + (cTestMode() ? qsl(":/test/") : QString())).toUtf8();
	FileKey dataNameHash[2];
	hashMd5(dataNameUtf8.constData(), dataNameUtf8.size(), dataNameHash);
	_dataNameKey = dataNameHash[0];
	_userBasePath = _basePath + toFilePart(_dataNameKey) + QChar('/');

	FileReadDescriptor mapData;
	if (!readFile(mapData, qsl("map"))) {
		return ReadMapFailed;
	}
	LOG(("App Info: reading map..."));

	QByteArray salt, keyEncrypted, mapEncrypted;
	mapData.stream >> salt >> keyEncrypted >> mapEncrypted;
	if (!_checkStreamStatus(mapData.stream)) {
		return ReadMapFailed;
	}

	if (salt.size() != LocalEncryptSaltSize) {
		LOG(("App Error: bad salt in map file, size: %1").arg(salt.size()));
		return ReadMapFailed;
	}
	createLocalKey(pass, &salt, &PassKey);

	EncryptedDescriptor keyData, map;
	if (!decryptLocal(keyData, keyEncrypted, PassKey)) {
		LOG(("App Info: could not decrypt pass-protected key from map file, maybe bad password..."));
		return ReadMapPassNeeded;
	}
	auto key = Serialize::read<MTP::AuthKey::Data>(keyData.stream);
	if (keyData.stream.status() != QDataStream::Ok || !keyData.stream.atEnd()) {
		LOG(("App Error: could not read pass-protected key from map file"));
		return ReadMapFailed;
	}
	LocalKey = std::make_shared<MTP::AuthKey>(key);

	_passKeyEncrypted = keyEncrypted;
	_passKeySalt = salt;

	if (!decryptLocal(map, mapEncrypted)) {
		LOG(("App Error: could not decrypt map."));
		return ReadMapFailed;
	}
	LOG(("App Info: reading encrypted map..."));

	DraftsMap draftsMap, draftCursorsMap;
	DraftsNotReadMap draftsNotReadMap;
	StorageMap imagesMap, stickerImagesMap, audiosMap;
	int64_t storageImagesSize = 0, storageStickersSize = 0, storageAudiosSize = 0;
	uint64_t locationsKey = 0, reportSpamStatusesKey = 0, trustedBotsKey = 0;
	uint64_t recentStickersKeyOld = 0;
	uint64_t installedStickersKey = 0, featuredStickersKey = 0, recentStickersKey = 0, favedStickersKey = 0, archivedStickersKey = 0;
	uint64_t savedGifsKey = 0;
	uint64_t backgroundKey = 0, userSettingsKey = 0, recentHashtagsAndBotsKey = 0, savedPeersKey = 0;
	while (!map.stream.atEnd()) {
		uint32_t keyType;
		map.stream >> keyType;
		switch (keyType) {
		case lskDraft: {
			uint32_t count = 0;
			map.stream >> count;
			for (uint32_t i = 0; i < count; ++i) {
				FileKey key;
				uint64_t p;
				map.stream >> key >> p;
				draftsMap.insert(p, key);
				draftsNotReadMap.insert(p, true);
			}
		} break;
		case lskDraftPosition: {
			uint32_t count = 0;
			map.stream >> count;
			for (uint32_t i = 0; i < count; ++i) {
				FileKey key;
				uint64_t p;
				map.stream >> key >> p;
				draftCursorsMap.insert(p, key);
			}
		} break;
		case lskImages: {
			uint32_t count = 0;
			map.stream >> count;
			for (uint32_t i = 0; i < count; ++i) {
				FileKey key;
				uint64_t first, second;
				int32_t size;
				map.stream >> key >> first >> second >> size;
				imagesMap.insert(StorageKey(first, second), FileDesc(key, size));
				storageImagesSize += size;
			}
		} break;
		case lskStickerImages: {
			uint32_t count = 0;
			map.stream >> count;
			for (uint32_t i = 0; i < count; ++i) {
				FileKey key;
				uint64_t first, second;
				int32_t size;
				map.stream >> key >> first >> second >> size;
				stickerImagesMap.insert(StorageKey(first, second), FileDesc(key, size));
				storageStickersSize += size;
			}
		} break;
		case lskAudios: {
			uint32_t count = 0;
			map.stream >> count;
			for (uint32_t i = 0; i < count; ++i) {
				FileKey key;
				uint64_t first, second;
				int32_t size;
				map.stream >> key >> first >> second >> size;
				audiosMap.insert(StorageKey(first, second), FileDesc(key, size));
				storageAudiosSize += size;
			}
		} break;
		case lskLocations: {
			map.stream >> locationsKey;
		} break;
		case lskReportSpamStatuses: {
			map.stream >> reportSpamStatusesKey;
		} break;
		case lskTrustedBots: {
			map.stream >> trustedBotsKey;
		} break;
		case lskRecentStickersOld: {
			map.stream >> recentStickersKeyOld;
		} break;
		case lskBackground: {
			map.stream >> backgroundKey;
		} break;
		case lskUserSettings: {
			map.stream >> userSettingsKey;
		} break;
		case lskRecentHashtagsAndBots: {
			map.stream >> recentHashtagsAndBotsKey;
		} break;
		case lskStickersOld: {
			map.stream >> installedStickersKey;
		} break;
		case lskStickersKeys: {
			map.stream >> installedStickersKey >> featuredStickersKey >> recentStickersKey >> archivedStickersKey;
		} break;
		case lskFavedStickers: {
			map.stream >> favedStickersKey;
		} break;
		case lskSavedGifsOld: {
			uint64_t key;
			map.stream >> key;
		} break;
		case lskSavedGifs: {
			map.stream >> savedGifsKey;
		} break;
		case lskSavedPeers: {
			map.stream >> savedPeersKey;
		} break;
		default:
		LOG(("App Error: unknown key type in encrypted map: %1").arg(keyType));
		return ReadMapFailed;
		}
		if (!_checkStreamStatus(map.stream)) {
			return ReadMapFailed;
		}
	}

	_draftsMap = draftsMap;
	_draftCursorsMap = draftCursorsMap;
	_draftsNotReadMap = draftsNotReadMap;

	_imagesMap = imagesMap;
	_storageImagesSize = storageImagesSize;
	_stickerImagesMap = stickerImagesMap;
	_storageStickersSize = storageStickersSize;
	_audiosMap = audiosMap;
	_storageAudiosSize = storageAudiosSize;

	_locationsKey = locationsKey;
	_reportSpamStatusesKey = reportSpamStatusesKey;
	_trustedBotsKey = trustedBotsKey;
	_recentStickersKeyOld = recentStickersKeyOld;
	_installedStickersKey = installedStickersKey;
	_featuredStickersKey = featuredStickersKey;
	_recentStickersKey = recentStickersKey;
	_favedStickersKey = favedStickersKey;
	_archivedStickersKey = archivedStickersKey;
	_savedGifsKey = savedGifsKey;
	_savedPeersKey = savedPeersKey;
	_backgroundKey = backgroundKey;
	_userSettingsKey = userSettingsKey;
	_recentHashtagsAndBotsKey = recentHashtagsAndBotsKey;
	_oldMapVersion = mapData.version;
	if (_oldMapVersion < AppVersion) {
		_mapChanged = true;
		_writeMap();
	} else {
		_mapChanged = false;
	}

	if (_locationsKey) {
		_readLocations();
	}
	if (_reportSpamStatusesKey) {
		_readReportSpamStatuses();
	}

	_readUserSettings();
	_readMtpData();

	Messenger::Instance().setAuthSessionFromStorage(std::move(StoredAuthSessionCache));

	LOG(("Map read time: %1").arg(getms() - ms));
	if (_oldSettingsVersion < AppVersion) {
		writeSettings();
	}
	return ReadMapDone;
}

void _writeMap(WriteMapWhen when) {
	if (when != WriteMapWhen::Now) {
		_manager->writeMap(when == WriteMapWhen::Fast);
		return;
	}
	_manager->writingMap();
	if (!_mapChanged) return;
	if (_userBasePath.isEmpty()) {
		LOG(("App Error: _userBasePath is empty in writeMap()"));
		return;
	}

	if (!QDir().exists(_userBasePath)) QDir().mkpath(_userBasePath);

	FileWriteDescriptor map(qsl("map"));
	if (_passKeySalt.isEmpty() || _passKeyEncrypted.isEmpty()) {
		QByteArray pass(kLocalKeySize, Qt::Uninitialized), salt(LocalEncryptSaltSize, Qt::Uninitialized);
		memset_rand(pass.data(), pass.size());
		memset_rand(salt.data(), salt.size());
		createLocalKey(pass, &salt, &LocalKey);

		_passKeySalt.resize(LocalEncryptSaltSize);
		memset_rand(_passKeySalt.data(), _passKeySalt.size());
		createLocalKey(QByteArray(), &_passKeySalt, &PassKey);

		EncryptedDescriptor passKeyData(kLocalKeySize);
		LocalKey->write(passKeyData.stream);
		_passKeyEncrypted = FileWriteDescriptor::prepareEncrypted(passKeyData, PassKey);
	}
	map.writeData(_passKeySalt);
	map.writeData(_passKeyEncrypted);

	uint32_t mapSize = 0;
	if (!_draftsMap.isEmpty()) mapSize += sizeof(uint32_t) * 2 + _draftsMap.size() * sizeof(uint64_t) * 2;
	if (!_draftCursorsMap.isEmpty()) mapSize += sizeof(uint32_t) * 2 + _draftCursorsMap.size() * sizeof(uint64_t) * 2;
	if (!_imagesMap.isEmpty()) mapSize += sizeof(uint32_t) * 2 + _imagesMap.size() * (sizeof(uint64_t) * 3 + sizeof(int32_t));
	if (!_stickerImagesMap.isEmpty()) mapSize += sizeof(uint32_t) * 2 + _stickerImagesMap.size() * (sizeof(uint64_t) * 3 + sizeof(int32_t));
	if (!_audiosMap.isEmpty()) mapSize += sizeof(uint32_t) * 2 + _audiosMap.size() * (sizeof(uint64_t) * 3 + sizeof(int32_t));
	if (_locationsKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_reportSpamStatusesKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_trustedBotsKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_recentStickersKeyOld) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_installedStickersKey || _featuredStickersKey || _recentStickersKey || _archivedStickersKey) {
		mapSize += sizeof(uint32_t) + 4 * sizeof(uint64_t);
	}
	if (_favedStickersKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_savedGifsKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_savedPeersKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_backgroundKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_userSettingsKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	if (_recentHashtagsAndBotsKey) mapSize += sizeof(uint32_t) + sizeof(uint64_t);
	EncryptedDescriptor mapData(mapSize);
	if (!_draftsMap.isEmpty()) {
		mapData.stream << uint32_t(lskDraft) << uint32_t(_draftsMap.size());
		for (DraftsMap::const_iterator i = _draftsMap.cbegin(), e = _draftsMap.cend(); i != e; ++i) {
			mapData.stream << uint64_t(i.value()) << uint64_t(i.key());
		}
	}
	if (!_draftCursorsMap.isEmpty()) {
		mapData.stream << uint32_t(lskDraftPosition) << uint32_t(_draftCursorsMap.size());
		for (DraftsMap::const_iterator i = _draftCursorsMap.cbegin(), e = _draftCursorsMap.cend(); i != e; ++i) {
			mapData.stream << uint64_t(i.value()) << uint64_t(i.key());
		}
	}
	if (!_imagesMap.isEmpty()) {
		mapData.stream << uint32_t(lskImages) << uint32_t(_imagesMap.size());
		for (StorageMap::const_iterator i = _imagesMap.cbegin(), e = _imagesMap.cend(); i != e; ++i) {
			mapData.stream << uint64_t(i.value().first) << uint64_t(i.key().first) << uint64_t(i.key().second) << int32_t(i.value().second);
		}
	}
	if (!_stickerImagesMap.isEmpty()) {
		mapData.stream << uint32_t(lskStickerImages) << uint32_t(_stickerImagesMap.size());
		for (StorageMap::const_iterator i = _stickerImagesMap.cbegin(), e = _stickerImagesMap.cend(); i != e; ++i) {
			mapData.stream << uint64_t(i.value().first) << uint64_t(i.key().first) << uint64_t(i.key().second) << int32_t(i.value().second);
		}
	}
	if (!_audiosMap.isEmpty()) {
		mapData.stream << uint32_t(lskAudios) << uint32_t(_audiosMap.size());
		for (StorageMap::const_iterator i = _audiosMap.cbegin(), e = _audiosMap.cend(); i != e; ++i) {
			mapData.stream << uint64_t(i.value().first) << uint64_t(i.key().first) << uint64_t(i.key().second) << int32_t(i.value().second);
		}
	}
	if (_locationsKey) {
		mapData.stream << uint32_t(lskLocations) << uint64_t(_locationsKey);
	}
	if (_reportSpamStatusesKey) {
		mapData.stream << uint32_t(lskReportSpamStatuses) << uint64_t(_reportSpamStatusesKey);
	}
	if (_trustedBotsKey) {
		mapData.stream << uint32_t(lskTrustedBots) << uint64_t(_trustedBotsKey);
	}
	if (_recentStickersKeyOld) {
		mapData.stream << uint32_t(lskRecentStickersOld) << uint64_t(_recentStickersKeyOld);
	}
	if (_installedStickersKey || _featuredStickersKey || _recentStickersKey || _archivedStickersKey) {
		mapData.stream << uint32_t(lskStickersKeys);
		mapData.stream << uint64_t(_installedStickersKey) << uint64_t(_featuredStickersKey) << uint64_t(_recentStickersKey) << uint64_t(_archivedStickersKey);
	}
	if (_favedStickersKey) {
		mapData.stream << uint32_t(lskFavedStickers) << uint64_t(_favedStickersKey);
	}
	if (_savedGifsKey) {
		mapData.stream << uint32_t(lskSavedGifs) << uint64_t(_savedGifsKey);
	}
	if (_savedPeersKey) {
		mapData.stream << uint32_t(lskSavedPeers) << uint64_t(_savedPeersKey);
	}
	if (_backgroundKey) {
		mapData.stream << uint32_t(lskBackground) << uint64_t(_backgroundKey);
	}
	if (_userSettingsKey) {
		mapData.stream << uint32_t(lskUserSettings) << uint64_t(_userSettingsKey);
	}
	if (_recentHashtagsAndBotsKey) {
		mapData.stream << uint32_t(lskRecentHashtagsAndBots) << uint64_t(_recentHashtagsAndBotsKey);
	}
	map.writeEncrypted(mapData);

	_mapChanged = false;
}

} // namespace

void finish() {
	if (_manager) {
		_writeMap(WriteMapWhen::Now);
		_manager->finish();
		_manager->deleteLater();
		_manager = 0;
		delete base::take(_localLoader);
	}
}

void readTheme();
void readLangPack();

void start() {
	Expects(!_manager);

	_manager = new internal::Manager();
	_localLoader = new TaskQueue(0, FileLoaderQueueStopTimeout);

	_basePath = cWorkingDir() + qsl("tdata/");
	if (!QDir().exists(_basePath)) QDir().mkpath(_basePath);

	ReadSettingsContext context;
	FileReadDescriptor settingsData;
	if (!readFile(settingsData, cTestMode() ? qsl("settings_test") : qsl("settings"), FileOption::Safe)) {
		_readOldSettings(true, context);
		_readOldUserSettings(false, context); // needed further in _readUserSettings
		_readOldMtpData(false, context); // needed further in _readMtpData
		applyReadContext(std::move(context));

		return writeSettings();
	}
	LOG(("App Info: reading settings..."));

	QByteArray salt, settingsEncrypted;
	settingsData.stream >> salt >> settingsEncrypted;
	if (!_checkStreamStatus(settingsData.stream)) {
		return writeSettings();
	}

	if (salt.size() != LocalEncryptSaltSize) {
		LOG(("App Error: bad salt in settings file, size: %1").arg(salt.size()));
		return writeSettings();
	}
	createLocalKey(QByteArray(), &salt, &SettingsKey);

	EncryptedDescriptor settings;
	if (!decryptLocal(settings, settingsEncrypted, SettingsKey)) {
		LOG(("App Error: could not decrypt settings from settings file, maybe bad passcode..."));
		return writeSettings();
	}

	LOG(("App Info: reading encrypted settings..."));
	while (!settings.stream.atEnd()) {
		uint32_t blockId;
		settings.stream >> blockId;
		if (!_checkStreamStatus(settings.stream)) {
			return writeSettings();
		}

		if (!_readSetting(blockId, settings.stream, settingsData.version, context)) {
			return writeSettings();
		}
	}

	_oldSettingsVersion = settingsData.version;
	_settingsSalt = salt;

	readTheme();
	readLangPack();

	applyReadContext(std::move(context));
}

void writeSettings() {
	if (_basePath.isEmpty()) {
		LOG(("App Error: _basePath is empty in writeSettings()"));
		return;
	}

	if (!QDir().exists(_basePath)) QDir().mkpath(_basePath);

	FileWriteDescriptor settings(cTestMode() ? qsl("settings_test") : qsl("settings"), FileOption::Safe);
	if (_settingsSalt.isEmpty() || !SettingsKey) {
		_settingsSalt.resize(LocalEncryptSaltSize);
		memset_rand(_settingsSalt.data(), _settingsSalt.size());
		createLocalKey(QByteArray(), &_settingsSalt, &SettingsKey);
	}
	settings.writeData(_settingsSalt);

	auto dcOptionsSerialized = Messenger::Instance().dcOptions()->serialize();

	uint32_t size = 12 * (sizeof(uint32_t) + sizeof(int32_t));
	size += sizeof(uint32_t) + Serialize::bytearraySize(dcOptionsSerialized);

	auto &proxy = Global::ConnectionProxy();
	size += sizeof(uint32_t) + sizeof(int32_t) + sizeof(int32_t);
	size += Serialize::stringSize(proxy.host) + sizeof(int32_t) + Serialize::stringSize(proxy.user) + Serialize::stringSize(proxy.password);

	if (_themeKey) {
		size += sizeof(uint32_t) + sizeof(uint64_t);
	}
	if (_langPackKey) {
		size += sizeof(uint32_t) + sizeof(uint64_t);
	}
	size += sizeof(uint32_t) + sizeof(int32_t) * 8;

	EncryptedDescriptor data(size);
	data.stream << uint32_t(dbiChatSizeMax) << int32_t(Global::ChatSizeMax());
	data.stream << uint32_t(dbiMegagroupSizeMax) << int32_t(Global::MegagroupSizeMax());
	data.stream << uint32_t(dbiSavedGifsLimit) << int32_t(Global::SavedGifsLimit());
	data.stream << uint32_t(dbiStickersRecentLimit) << int32_t(Global::StickersRecentLimit());
	data.stream << uint32_t(dbiStickersFavedLimit) << int32_t(Global::StickersFavedLimit());
	data.stream << uint32_t(dbiAutoStart) << int32_t(cAutoStart());
	data.stream << uint32_t(dbiStartMinimized) << int32_t(cStartMinimized());
	data.stream << uint32_t(dbiSendToMenu) << int32_t(cSendToMenu());
	data.stream << uint32_t(dbiWorkMode) << int32_t(Global::WorkMode().value());
	data.stream << uint32_t(dbiSeenTrayTooltip) << int32_t(cSeenTrayTooltip());
	data.stream << uint32_t(dbiAutoUpdate) << int32_t(cAutoUpdate());
	data.stream << uint32_t(dbiLastUpdateCheck) << int32_t(cLastUpdateCheck());
	data.stream << uint32_t(dbiScale) << int32_t(cConfigScale());
	data.stream << uint32_t(dbiDcOptions) << dcOptionsSerialized;

	data.stream << uint32_t(dbiConnectionType) << int32_t(Global::ConnectionType()) << int32_t(Global::LastProxyType());
	data.stream << proxy.host << int32_t(proxy.port) << proxy.user << proxy.password;

	data.stream << uint32_t(dbiTryIPv6) << int32_t(Global::TryIPv6());
	if (_themeKey) {
		data.stream << uint32_t(dbiThemeKey) << uint64_t(_themeKey);
	}
	if (_langPackKey) {
		data.stream << uint32_t(dbiLangPackKey) << uint64_t(_langPackKey);
	}

	auto position = cWindowPos();
	data.stream << uint32_t(dbiWindowPosition) << int32_t(position.x) << int32_t(position.y) << int32_t(position.w) << int32_t(position.h);
	data.stream << int32_t(position.moncrc) << int32_t(position.maximized);

	DEBUG_LOG(("Window Pos: Writing to storage %1, %2, %3, %4 (maximized %5)").arg(position.x).arg(position.y).arg(position.w).arg(position.h).arg(Logs::b(position.maximized)));

	settings.writeEncrypted(data, SettingsKey);
}

void writeUserSettings() {
	_writeUserSettings();
}

void writeMtpData() {
	_writeMtpData();
}

void reset() {
	if (_localLoader) {
		_localLoader->stop();
	}

	_passKeySalt.clear(); // reset passcode, local key
	_draftsMap.clear();
	_draftCursorsMap.clear();
	_fileLocations.clear();
	_fileLocationPairs.clear();
	_fileLocationAliases.clear();
	_imagesMap.clear();
	_draftsNotReadMap.clear();
	_stickerImagesMap.clear();
	_audiosMap.clear();
	_storageImagesSize = _storageStickersSize = _storageAudiosSize = 0;
	_webFilesMap.clear();
	_storageWebFilesSize = 0;
	_locationsKey = _reportSpamStatusesKey = _trustedBotsKey = 0;
	_recentStickersKeyOld = 0;
	_installedStickersKey = _featuredStickersKey = _recentStickersKey = _favedStickersKey = _archivedStickersKey = 0;
	_savedGifsKey = 0;
	_backgroundKey = _userSettingsKey = _recentHashtagsAndBotsKey = _savedPeersKey = 0;
	_oldMapVersion = _oldSettingsVersion = 0;
	StoredAuthSessionCache.reset();
	_mapChanged = true;
	_writeMap(WriteMapWhen::Now);

	_writeMtpData();
}

bool checkPasscode(const QByteArray &passcode) {
	auto checkKey = MTP::AuthKeyPtr();
	createLocalKey(passcode, &_passKeySalt, &checkKey);
	return checkKey->equals(PassKey);
}

void setPasscode(const QByteArray &passcode) {
	createLocalKey(passcode, &_passKeySalt, &PassKey);

	EncryptedDescriptor passKeyData(kLocalKeySize);
	LocalKey->write(passKeyData.stream);
	_passKeyEncrypted = FileWriteDescriptor::prepareEncrypted(passKeyData, PassKey);

	_mapChanged = true;
	_writeMap(WriteMapWhen::Now);

	Global::SetLocalPasscode(!passcode.isEmpty());
	Global::RefLocalPasscodeChanged().notify();
}

ReadMapState readMap(const QByteArray &pass) {
	ReadMapState result = _readMap(pass);
	if (result == ReadMapFailed) {
		_mapChanged = true;
		_writeMap(WriteMapWhen::Now);
	}
	return result;
}

int32_t oldMapVersion() {
	return _oldMapVersion;
}

int32_t oldSettingsVersion() {
	return _oldSettingsVersion;
}

void writeDrafts(const PeerId &peer, const MessageDraft &localDraft, const MessageDraft &editDraft) {
	if (!_working()) return;

	if (localDraft.msgId <= 0 && localDraft.textWithTags.text.isEmpty() && editDraft.msgId <= 0) {
		auto i = _draftsMap.find(peer);
		if (i != _draftsMap.cend()) {
			clearKey(i.value());
			_draftsMap.erase(i);
			_mapChanged = true;
			_writeMap();
		}

		_draftsNotReadMap.remove(peer);
	} else {
		auto i = _draftsMap.constFind(peer);
		if (i == _draftsMap.cend()) {
			i = _draftsMap.insert(peer, genKey());
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}

		auto msgTags = Ui::FlatTextarea::serializeTagsList(localDraft.textWithTags.tags);
		auto editTags = Ui::FlatTextarea::serializeTagsList(editDraft.textWithTags.tags);

		int size = sizeof(uint64_t);
		size += Serialize::stringSize(localDraft.textWithTags.text) + Serialize::bytearraySize(msgTags) + 2 * sizeof(int32_t);
		size += Serialize::stringSize(editDraft.textWithTags.text) + Serialize::bytearraySize(editTags) + 2 * sizeof(int32_t);

		EncryptedDescriptor data(size);
		data.stream << uint64_t(peer);
		data.stream << localDraft.textWithTags.text << msgTags;
		data.stream << int32_t(localDraft.msgId) << int32_t(localDraft.previewCancelled ? 1 : 0);
		data.stream << editDraft.textWithTags.text << editTags;
		data.stream << int32_t(editDraft.msgId) << int32_t(editDraft.previewCancelled ? 1 : 0);

		FileWriteDescriptor file(i.value());
		file.writeEncrypted(data);

		_draftsNotReadMap.remove(peer);
	}
}

void clearDraftCursors(const PeerId &peer) {
	DraftsMap::iterator i = _draftCursorsMap.find(peer);
	if (i != _draftCursorsMap.cend()) {
		clearKey(i.value());
		_draftCursorsMap.erase(i);
		_mapChanged = true;
		_writeMap();
	}
}

void _readDraftCursors(const PeerId &peer, MessageCursor &localCursor, MessageCursor &editCursor) {
	DraftsMap::iterator j = _draftCursorsMap.find(peer);
	if (j == _draftCursorsMap.cend()) {
		return;
	}

	FileReadDescriptor draft;
	if (!readEncryptedFile(draft, j.value())) {
		clearDraftCursors(peer);
		return;
	}
	uint64_t draftPeer;
	int32_t localPosition = 0, localAnchor = 0, localScroll = QFIXED_MAX;
	int32_t editPosition = 0, editAnchor = 0, editScroll = QFIXED_MAX;
	draft.stream >> draftPeer >> localPosition >> localAnchor >> localScroll;
	if (!draft.stream.atEnd()) {
		draft.stream >> editPosition >> editAnchor >> editScroll;
	}

	if (draftPeer != peer) {
		clearDraftCursors(peer);
		return;
	}

	localCursor = MessageCursor(localPosition, localAnchor, localScroll);
	editCursor = MessageCursor(editPosition, editAnchor, editScroll);
}

void readDraftsWithCursors(History *h) {
	PeerId peer = h->peer->id;
	if (!_draftsNotReadMap.remove(peer)) {
		clearDraftCursors(peer);
		return;
	}

	DraftsMap::iterator j = _draftsMap.find(peer);
	if (j == _draftsMap.cend()) {
		clearDraftCursors(peer);
		return;
	}
	FileReadDescriptor draft;
	if (!readEncryptedFile(draft, j.value())) {
		clearKey(j.value());
		_draftsMap.erase(j);
		clearDraftCursors(peer);
		return;
	}

	uint64_t draftPeer = 0;
	TextWithTags msgData, editData;
	QByteArray msgTagsSerialized, editTagsSerialized;
	int32_t msgReplyTo = 0, msgPreviewCancelled = 0, editMsgId = 0, editPreviewCancelled = 0;
	draft.stream >> draftPeer >> msgData.text;
	if (draft.version >= 9048) {
		draft.stream >> msgTagsSerialized;
	}
	if (draft.version >= 7021) {
		draft.stream >> msgReplyTo;
		if (draft.version >= 8001) {
			draft.stream >> msgPreviewCancelled;
			if (!draft.stream.atEnd()) {
				draft.stream >> editData.text;
				if (draft.version >= 9048) {
					draft.stream >> editTagsSerialized;
				}
				draft.stream >> editMsgId >> editPreviewCancelled;
			}
		}
	}
	if (draftPeer != peer) {
		clearKey(j.value());
		_draftsMap.erase(j);
		clearDraftCursors(peer);
		return;
	}

	msgData.tags = Ui::FlatTextarea::deserializeTagsList(msgTagsSerialized, msgData.text.size());
	editData.tags = Ui::FlatTextarea::deserializeTagsList(editTagsSerialized, editData.text.size());

	MessageCursor msgCursor, editCursor;
	_readDraftCursors(peer, msgCursor, editCursor);

	if (!h->localDraft()) {
		if (msgData.text.isEmpty() && !msgReplyTo) {
			h->clearLocalDraft();
		} else {
			h->setLocalDraft(std::make_unique<Data::Draft>(msgData, msgReplyTo, msgCursor, msgPreviewCancelled));
		}
	}
	if (!editMsgId) {
		h->clearEditDraft();
	} else {
		h->setEditDraft(std::make_unique<Data::Draft>(editData, editMsgId, editCursor, editPreviewCancelled));
	}
}

void writeDraftCursors(const PeerId &peer, const MessageCursor &msgCursor, const MessageCursor &editCursor) {
	if (!_working()) return;

	if (msgCursor == MessageCursor() && editCursor == MessageCursor()) {
		clearDraftCursors(peer);
	} else {
		DraftsMap::const_iterator i = _draftCursorsMap.constFind(peer);
		if (i == _draftCursorsMap.cend()) {
			i = _draftCursorsMap.insert(peer, genKey());
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}

		EncryptedDescriptor data(sizeof(uint64_t) + sizeof(int32_t) * 3);
		data.stream << uint64_t(peer) << int32_t(msgCursor.position) << int32_t(msgCursor.anchor) << int32_t(msgCursor.scroll);
		data.stream << int32_t(editCursor.position) << int32_t(editCursor.anchor) << int32_t(editCursor.scroll);

		FileWriteDescriptor file(i.value());
		file.writeEncrypted(data);
	}
}

bool hasDraftCursors(const PeerId &peer) {
	return _draftCursorsMap.contains(peer);
}

bool hasDraft(const PeerId &peer) {
	return _draftsMap.contains(peer);
}

void writeFileLocation(MediaKey location, const FileLocation &local) {
	if (local.fname.isEmpty()) return;

	FileLocationAliases::const_iterator aliasIt = _fileLocationAliases.constFind(location);
	if (aliasIt != _fileLocationAliases.cend()) {
		location = aliasIt.value();
	}

	FileLocationPairs::iterator i = _fileLocationPairs.find(local.fname);
	if (i != _fileLocationPairs.cend()) {
		if (i.value().second == local) {
			if (i.value().first != location) {
				_fileLocationAliases.insert(location, i.value().first);
				_writeLocations(WriteMapWhen::Fast);
			}
			return;
		}
		if (i.value().first != location) {
			for (FileLocations::iterator j = _fileLocations.find(i.value().first), e = _fileLocations.end(); (j != e) && (j.key() == i.value().first);) {
				if (j.value() == i.value().second) {
					_fileLocations.erase(j);
					break;
				}
			}
			_fileLocationPairs.erase(i);
		}
	}
	_fileLocations.insert(location, local);
	_fileLocationPairs.insert(local.fname, FileLocationPair(location, local));
	_writeLocations(WriteMapWhen::Fast);
}

FileLocation readFileLocation(MediaKey location, bool check) {
	FileLocationAliases::const_iterator aliasIt = _fileLocationAliases.constFind(location);
	if (aliasIt != _fileLocationAliases.cend()) {
		location = aliasIt.value();
	}

	FileLocations::iterator i = _fileLocations.find(location);
	for (FileLocations::iterator i = _fileLocations.find(location); (i != _fileLocations.end()) && (i.key() == location);) {
		if (check) {
			if (!i.value().check()) {
				_fileLocationPairs.remove(i.value().fname);
				i = _fileLocations.erase(i);
				_writeLocations();
				continue;
			}
		}
		return i.value();
	}
	return FileLocation();
}

int32_t _storageImageSize(int32_t rawlen) {
	// fulllen + storagekey + type + len + data
	int32_t result = sizeof(uint32_t) + sizeof(uint64_t) * 2 + sizeof(uint32_t) + sizeof(uint32_t) + rawlen;
	if (result & 0x0F) result += 0x10 - (result & 0x0F);
	result += tdfMagicLen + sizeof(int32_t) + sizeof(uint32_t) + 0x10 + 0x10; // magic + version + len of encrypted + part of sha1 + md5
	return result;
}

int32_t _storageStickerSize(int32_t rawlen) {
	// fulllen + storagekey + len + data
	int32_t result = sizeof(uint32_t) + sizeof(uint64_t) * 2 + sizeof(uint32_t) + rawlen;
	if (result & 0x0F) result += 0x10 - (result & 0x0F);
	result += tdfMagicLen + sizeof(int32_t) + sizeof(uint32_t) + 0x10 + 0x10; // magic + version + len of encrypted + part of sha1 + md5
	return result;
}

int32_t _storageAudioSize(int32_t rawlen) {
	// fulllen + storagekey + len + data
	int32_t result = sizeof(uint32_t) + sizeof(uint64_t) * 2 + sizeof(uint32_t) + rawlen;
	if (result & 0x0F) result += 0x10 - (result & 0x0F);
	result += tdfMagicLen + sizeof(int32_t) + sizeof(uint32_t) + 0x10 + 0x10; // magic + version + len of encrypted + part of sha1 + md5
	return result;
}

void writeImage(const StorageKey &location, const ImagePtr &image) {
	if (image->isNull() || !image->loaded()) return;
	if (_imagesMap.constFind(location) != _imagesMap.cend()) return;

	image->forget();
	writeImage(location, StorageImageSaved(image->savedData()), false);
}

void writeImage(const StorageKey &location, const StorageImageSaved &image, bool overwrite) {
	if (!_working()) return;

	int32_t size = _storageImageSize(image.data.size());
	StorageMap::const_iterator i = _imagesMap.constFind(location);
	if (i == _imagesMap.cend()) {
		i = _imagesMap.insert(location, FileDesc(genKey(FileOption::User), size));
		_storageImagesSize += size;
		_mapChanged = true;
		_writeMap();
	} else if (!overwrite) {
		return;
	}

	auto legacyTypeField = 0;

	EncryptedDescriptor data(sizeof(uint64_t) * 2 + sizeof(uint32_t) + sizeof(uint32_t) + image.data.size());
	data.stream << uint64_t(location.first) << uint64_t(location.second) << uint32_t(legacyTypeField) << image.data;

	FileWriteDescriptor file(i.value().first, FileOption::User);
	file.writeEncrypted(data);
	if (i.value().second != size) {
		_storageImagesSize += size;
		_storageImagesSize -= i.value().second;
		_imagesMap[location].second = size;
	}
}

class AbstractCachedLoadTask : public Task {
public:

	AbstractCachedLoadTask(const FileKey &key, const StorageKey &location, bool readImageFlag, mtpFileLoader *loader) :
		_key(key), _location(location), _readImageFlag(readImageFlag), _loader(loader), _result(0) {
	}
	void process() {
		FileReadDescriptor image;
		if (!readEncryptedFile(image, _key, FileOption::User)) {
			return;
		}

		QByteArray imageData;
		uint64_t locFirst, locSecond;
		uint32_t legacyTypeField = 0;
		readFromStream(image.stream, locFirst, locSecond, imageData);

		// we're saving files now before we have actual location
		//if (locFirst != _location.first || locSecond != _location.second) {
		//	return;
		//}

		_result = new Result(imageData, _readImageFlag);
	}
	void finish() {
		if (_result) {
			_loader->localLoaded(_result->image, _result->format, _result->pixmap);
		} else {
			clearInMap();
			_loader->localLoaded(StorageImageSaved());
		}
	}
	virtual void readFromStream(QDataStream &stream, uint64_t &first, uint64_t &second, QByteArray &data) = 0;
	virtual void clearInMap() = 0;
	virtual ~AbstractCachedLoadTask() {
		delete base::take(_result);
	}

protected:
	FileKey _key;
	StorageKey _location;
	bool _readImageFlag;
	struct Result {
		Result(const QByteArray &data, bool readImageFlag) : image(data) {
			if (readImageFlag) {
				auto realFormat = QByteArray();
				pixmap = App::pixmapFromImageInPlace(App::readImage(data, &realFormat, false));
				if (!pixmap.isNull()) {
					format = realFormat;
				}
			}
		}
		StorageImageSaved image;
		QByteArray format;
		QPixmap pixmap;

	};
	mtpFileLoader *_loader;
	Result *_result;

};

class ImageLoadTask : public AbstractCachedLoadTask {
public:
	ImageLoadTask(const FileKey &key, const StorageKey &location, mtpFileLoader *loader) :
	AbstractCachedLoadTask(key, location, true, loader) {
	}
	void readFromStream(QDataStream &stream, uint64_t &first, uint64_t &second, QByteArray &data) override {
		int32_t legacyTypeField = 0;
		stream >> first >> second >> legacyTypeField >> data;
	}
	void clearInMap() override {
		StorageMap::iterator j = _imagesMap.find(_location);
		if (j != _imagesMap.cend() && j->first == _key) {
			clearKey(_key, FileOption::User);
			_storageImagesSize -= j->second;
			_imagesMap.erase(j);
		}
	}
};

TaskId startImageLoad(const StorageKey &location, mtpFileLoader *loader) {
	StorageMap::const_iterator j = _imagesMap.constFind(location);
	if (j == _imagesMap.cend() || !_localLoader) {
		return 0;
	}
	return _localLoader->addTask(MakeShared<ImageLoadTask>(j->first, location, loader));
}

int32_t hasImages() {
	return _imagesMap.size();
}

int64_t storageImagesSize() {
	return _storageImagesSize;
}

void writeStickerImage(const StorageKey &location, const QByteArray &sticker, bool overwrite) {
	if (!_working()) return;

	int32_t size = _storageStickerSize(sticker.size());
	StorageMap::const_iterator i = _stickerImagesMap.constFind(location);
	if (i == _stickerImagesMap.cend()) {
		i = _stickerImagesMap.insert(location, FileDesc(genKey(FileOption::User), size));
		_storageStickersSize += size;
		_mapChanged = true;
		_writeMap();
	} else if (!overwrite) {
		return;
	}
	EncryptedDescriptor data(sizeof(uint64_t) * 2 + sizeof(uint32_t) + sizeof(uint32_t) + sticker.size());
	data.stream << uint64_t(location.first) << uint64_t(location.second) << sticker;
	FileWriteDescriptor file(i.value().first, FileOption::User);
	file.writeEncrypted(data);
	if (i.value().second != size) {
		_storageStickersSize += size;
		_storageStickersSize -= i.value().second;
		_stickerImagesMap[location].second = size;
	}
}

class StickerImageLoadTask : public AbstractCachedLoadTask {
public:
	StickerImageLoadTask(const FileKey &key, const StorageKey &location, mtpFileLoader *loader) :
	AbstractCachedLoadTask(key, location, true, loader) {
	}
	void readFromStream(QDataStream &stream, uint64_t &first, uint64_t &second, QByteArray &data) {
		stream >> first >> second >> data;
	}
	void clearInMap() {
		auto j = _stickerImagesMap.find(_location);
		if (j != _stickerImagesMap.cend() && j->first == _key) {
			clearKey(j.value().first, FileOption::User);
			_storageStickersSize -= j.value().second;
			_stickerImagesMap.erase(j);
		}
	}
};

TaskId startStickerImageLoad(const StorageKey &location, mtpFileLoader *loader) {
	auto j = _stickerImagesMap.constFind(location);
	if (j == _stickerImagesMap.cend() || !_localLoader) {
		return 0;
	}
	return _localLoader->addTask(MakeShared<StickerImageLoadTask>(j->first, location, loader));
}

bool willStickerImageLoad(const StorageKey &location) {
	return _stickerImagesMap.constFind(location) != _stickerImagesMap.cend();
}

bool copyStickerImage(const StorageKey &oldLocation, const StorageKey &newLocation) {
	auto i = _stickerImagesMap.constFind(oldLocation);
	if (i == _stickerImagesMap.cend()) {
		return false;
	}
	_stickerImagesMap.insert(newLocation, i.value());
	_mapChanged = true;
	_writeMap();
	return true;
}

int32_t hasStickers() {
	return _stickerImagesMap.size();
}

int64_t storageStickersSize() {
	return _storageStickersSize;
}

void writeAudio(const StorageKey &location, const QByteArray &audio, bool overwrite) {
	if (!_working()) return;

	int32_t size = _storageAudioSize(audio.size());
	StorageMap::const_iterator i = _audiosMap.constFind(location);
	if (i == _audiosMap.cend()) {
		i = _audiosMap.insert(location, FileDesc(genKey(FileOption::User), size));
		_storageAudiosSize += size;
		_mapChanged = true;
		_writeMap();
	} else if (!overwrite) {
		return;
	}
	EncryptedDescriptor data(sizeof(uint64_t) * 2 + sizeof(uint32_t) + sizeof(uint32_t) + audio.size());
	data.stream << uint64_t(location.first) << uint64_t(location.second) << audio;
	FileWriteDescriptor file(i.value().first, FileOption::User);
	file.writeEncrypted(data);
	if (i.value().second != size) {
		_storageAudiosSize += size;
		_storageAudiosSize -= i.value().second;
		_audiosMap[location].second = size;
	}
}

class AudioLoadTask : public AbstractCachedLoadTask {
public:
	AudioLoadTask(const FileKey &key, const StorageKey &location, mtpFileLoader *loader) :
	AbstractCachedLoadTask(key, location, false, loader) {
	}
	void readFromStream(QDataStream &stream, uint64_t &first, uint64_t &second, QByteArray &data) {
		stream >> first >> second >> data;
	}
	void clearInMap() {
		auto j = _audiosMap.find(_location);
		if (j != _audiosMap.cend() && j->first == _key) {
			clearKey(j.value().first, FileOption::User);
			_storageAudiosSize -= j.value().second;
			_audiosMap.erase(j);
		}
	}
};

TaskId startAudioLoad(const StorageKey &location, mtpFileLoader *loader) {
	auto j = _audiosMap.constFind(location);
	if (j == _audiosMap.cend() || !_localLoader) {
		return 0;
	}
	return _localLoader->addTask(MakeShared<AudioLoadTask>(j->first, location, loader));
}

bool copyAudio(const StorageKey &oldLocation, const StorageKey &newLocation) {
	auto i = _audiosMap.constFind(oldLocation);
	if (i == _audiosMap.cend()) {
		return false;
	}
	_audiosMap.insert(newLocation, i.value());
	_mapChanged = true;
	_writeMap();
	return true;
}

int32_t hasAudios() {
	return _audiosMap.size();
}

int64_t storageAudiosSize() {
	return _storageAudiosSize;
}

int32_t _storageWebFileSize(const QString &url, int32_t rawlen) {
	// fulllen + url + len + data
	int32_t result = sizeof(uint32_t) + Serialize::stringSize(url) + sizeof(uint32_t) + rawlen;
	if (result & 0x0F) result += 0x10 - (result & 0x0F);
	result += tdfMagicLen + sizeof(int32_t) + sizeof(uint32_t) + 0x10 + 0x10; // magic + version + len of encrypted + part of sha1 + md5
	return result;
}

void writeWebFile(const QString &url, const QByteArray &content, bool overwrite) {
	if (!_working()) return;

	int32_t size = _storageWebFileSize(url, content.size());
	WebFilesMap::const_iterator i = _webFilesMap.constFind(url);
	if (i == _webFilesMap.cend()) {
		i = _webFilesMap.insert(url, FileDesc(genKey(FileOption::User), size));
		_storageWebFilesSize += size;
		_writeLocations();
	} else if (!overwrite) {
		return;
	}
	EncryptedDescriptor data(Serialize::stringSize(url) + sizeof(uint32_t) + sizeof(uint32_t) + content.size());
	data.stream << url << content;
	FileWriteDescriptor file(i.value().first, FileOption::User);
	file.writeEncrypted(data);
	if (i.value().second != size) {
		_storageWebFilesSize += size;
		_storageWebFilesSize -= i.value().second;
		_webFilesMap[url].second = size;
	}
}

class WebFileLoadTask : public Task {
public:
	WebFileLoadTask(const FileKey &key, const QString &url, webFileLoader *loader)
		: _key(key)
		, _url(url)
		, _loader(loader)
		, _result(0) {
	}
	void process() {
		FileReadDescriptor image;
		if (!readEncryptedFile(image, _key, FileOption::User)) {
			return;
		}

		QByteArray imageData;
		QString url;
		image.stream >> url >> imageData;

		_result = new Result(imageData);
	}
	void finish() {
		if (_result) {
			_loader->localLoaded(_result->image, _result->format, _result->pixmap);
		} else {
			WebFilesMap::iterator j = _webFilesMap.find(_url);
			if (j != _webFilesMap.cend() && j->first == _key) {
				clearKey(j.value().first, FileOption::User);
				_storageWebFilesSize -= j.value().second;
				_webFilesMap.erase(j);
			}
			_loader->localLoaded(StorageImageSaved());
		}
	}
	virtual ~WebFileLoadTask() {
		delete base::take(_result);
	}

protected:
	FileKey _key;
	QString _url;
	struct Result {
		explicit Result(const QByteArray &data) : image(data) {
			QByteArray guessFormat;
			pixmap = App::pixmapFromImageInPlace(App::readImage(data, &guessFormat, false));
			if (!pixmap.isNull()) {
				format = guessFormat;
			}
		}
		StorageImageSaved image;
		QByteArray format;
		QPixmap pixmap;

	};
	webFileLoader *_loader;
	Result *_result;

};

TaskId startWebFileLoad(const QString &url, webFileLoader *loader) {
	WebFilesMap::const_iterator j = _webFilesMap.constFind(url);
	if (j == _webFilesMap.cend() || !_localLoader) {
		return 0;
	}
	return _localLoader->addTask(MakeShared<WebFileLoadTask>(j->first, url, loader));
}

int32_t hasWebFiles() {
	return _webFilesMap.size();
}

int64_t storageWebFilesSize() {
	return _storageWebFilesSize;
}

class CountWaveformTask : public Task {
public:
	CountWaveformTask(DocumentData *doc)
		: _doc(doc)
		, _loc(doc->location(true))
		, _data(doc->data())
		, _wavemax(0) {
		if (_data.isEmpty() && !_loc.accessEnable()) {
			_doc = 0;
		}
	}
	void process() {
		if (!_doc) return;

		_waveform = audioCountWaveform(_loc, _data);
		uchar wavemax = 0;
		for (int32_t i = 0, l = _waveform.size(); i < l; ++i) {
			uchar waveat = _waveform.at(i);
			if (wavemax < waveat) wavemax = waveat;
		}
		_wavemax = wavemax;
	}
	void finish() {
		if (VoiceData *voice = _doc ? _doc->voice() : 0) {
			if (!_waveform.isEmpty()) {
				voice->waveform = _waveform;
				voice->wavemax = _wavemax;
			}
			if (voice->waveform.isEmpty()) {
				voice->waveform.resize(1);
				voice->waveform[0] = -2;
				voice->wavemax = 0;
			} else if (voice->waveform[0] < 0) {
				voice->waveform[0] = -2;
				voice->wavemax = 0;
			}
			auto &items = App::documentItems();
			auto i = items.constFind(_doc);
			if (i != items.cend()) {
				for_const (auto item, i.value()) {
					Ui::repaintHistoryItem(item);
				}
			}
		}
	}
	virtual ~CountWaveformTask() {
		if (_data.isEmpty() && _doc) {
			_loc.accessDisable();
		}
	}

protected:
	DocumentData *_doc;
	FileLocation _loc;
	QByteArray _data;
	VoiceWaveform _waveform;
	char _wavemax;

};

void countVoiceWaveform(DocumentData *document) {
	if (VoiceData *voice = document->voice()) {
		if (_localLoader) {
			voice->waveform.resize(1 + sizeof(TaskId));
			voice->waveform[0] = -1; // counting
			TaskId taskId = _localLoader->addTask(MakeShared<CountWaveformTask>(document));
			memcpy(voice->waveform.data() + 1, &taskId, sizeof(taskId));
		}
	}
}

void cancelTask(TaskId id) {
	if (_localLoader) {
		_localLoader->cancelTask(id);
	}
}

void _writeStickerSet(QDataStream &stream, const Stickers::Set &set) {
	bool notLoaded = (set.flags & MTPDstickerSet_ClientFlag::f_not_loaded);
	if (notLoaded) {
		stream << uint64_t(set.id) << uint64_t(set.access) << set.title << set.shortName << int32_t(-set.count) << int32_t(set.hash) << int32_t(set.flags);
		return;
	} else {
		if (set.stickers.isEmpty()) return;
	}

	stream << uint64_t(set.id) << uint64_t(set.access) << set.title << set.shortName << int32_t(set.stickers.size()) << int32_t(set.hash) << int32_t(set.flags);
	for (StickerPack::const_iterator j = set.stickers.cbegin(), e = set.stickers.cend(); j != e; ++j) {
		Serialize::Document::writeToStream(stream, *j);
	}

	if (AppVersion > 9018) {
		stream << int32_t(set.emoji.size());
		for (auto j = set.emoji.cbegin(), e = set.emoji.cend(); j != e; ++j) {
			stream << j.key()->id() << int32_t(j->size());
			for (int32_t k = 0, l = j->size(); k < l; ++k) {
				stream << uint64_t(j->at(k)->id);
			}
		}
	}
}

// In generic method _writeStickerSets() we look through all the sets and call a
// callback on each set to see, if we write it, skip it or abort the whole write.
enum class StickerSetCheckResult {
	Write,
	Skip,
	Abort,
};

// CheckSet is a functor on Stickers::Set, which returns a StickerSetCheckResult.
template <typename CheckSet>
void _writeStickerSets(FileKey &stickersKey, CheckSet checkSet, const Stickers::Order &order) {
	if (!_working()) return;

	auto &sets = Global::StickerSets();
	if (sets.isEmpty()) {
		if (stickersKey) {
			clearKey(stickersKey);
			stickersKey = 0;
			_mapChanged = true;
		}
		_writeMap();
		return;
	}
	int32_t setsCount = 0;
	QByteArray hashToWrite;
	uint32_t size = sizeof(uint32_t) + Serialize::bytearraySize(hashToWrite);
	for_const (auto &set, sets) {
		auto result = checkSet(set);
		if (result == StickerSetCheckResult::Abort) {
			return;
		} else if (result == StickerSetCheckResult::Skip) {
			continue;
		}

		// id + access + title + shortName + stickersCount + hash + flags
		size += sizeof(uint64_t) * 2 + Serialize::stringSize(set.title) + Serialize::stringSize(set.shortName) + sizeof(uint32_t) + sizeof(int32_t) * 2;
		for_const (auto &sticker, set.stickers) {
			size += Serialize::Document::sizeInStream(sticker);
		}

		size += sizeof(int32_t); // emojiCount
		for (auto j = set.emoji.cbegin(), e = set.emoji.cend(); j != e; ++j) {
			size += Serialize::stringSize(j.key()->id()) + sizeof(int32_t) + (j->size() * sizeof(uint64_t));
		}

		++setsCount;
	}
	if (!setsCount && order.isEmpty()) {
		if (stickersKey) {
			clearKey(stickersKey);
			stickersKey = 0;
			_mapChanged = true;
		}
		_writeMap();
		return;
	}
	size += sizeof(int32_t) + (order.size() * sizeof(uint64_t));

	if (!stickersKey) {
		stickersKey = genKey();
		_mapChanged = true;
		_writeMap(WriteMapWhen::Fast);
	}
	EncryptedDescriptor data(size);
	data.stream << uint32_t(setsCount) << hashToWrite;
	for_const (auto &set, sets) {
		auto result = checkSet(set);
		if (result == StickerSetCheckResult::Abort) {
			return;
		} else if (result == StickerSetCheckResult::Skip) {
			continue;
		}
		_writeStickerSet(data.stream, set);
	}
	data.stream << order;

	FileWriteDescriptor file(stickersKey);
	file.writeEncrypted(data);
}

void _readStickerSets(FileKey &stickersKey, Stickers::Order *outOrder = nullptr, MTPDstickerSet::Flags readingFlags = 0) {
	FileReadDescriptor stickers;
	if (!readEncryptedFile(stickers, stickersKey)) {
		clearKey(stickersKey);
		stickersKey = 0;
		_writeMap();
		return;
	}

	bool readingInstalled = (readingFlags == MTPDstickerSet::Flag::f_installed);

	auto &sets = Global::RefStickerSets();
	if (outOrder) outOrder->clear();

	uint32_t cnt;
	QByteArray hash;
	stickers.stream >> cnt >> hash; // ignore hash, it is counted
	if (readingInstalled && stickers.version < 8019) { // bad data in old caches
		cnt += 2; // try to read at least something
	}
	for (uint32_t i = 0; i < cnt; ++i) {
		uint64_t setId = 0, setAccess = 0;
		QString setTitle, setShortName;
		int32_t scnt = 0;
		stickers.stream >> setId >> setAccess >> setTitle >> setShortName >> scnt;

		int32_t setHash = 0;
		MTPDstickerSet::Flags setFlags = 0;
		if (stickers.version > 8033) {
			int32_t setFlagsValue = 0;
			stickers.stream >> setHash >> setFlagsValue;
			setFlags = MTPDstickerSet::Flags{ setFlagsValue };
			if (setFlags & MTPDstickerSet_ClientFlag::f_not_loaded__old) {
				setFlags &= ~MTPDstickerSet_ClientFlag::f_not_loaded__old;
				setFlags |= MTPDstickerSet_ClientFlag::f_not_loaded;
			}
		}
		if (readingInstalled && stickers.version < 9061) {
			setFlags |= MTPDstickerSet::Flag::f_installed;
		}

		if (setId == Stickers::DefaultSetId) {
			setTitle = lang(lng_stickers_default_set);
			setFlags |= MTPDstickerSet::Flag::f_official | MTPDstickerSet_ClientFlag::f_special;
			if (readingInstalled && outOrder && stickers.version < 9061) {
				outOrder->push_front(setId);
			}
		} else if (setId == Stickers::CustomSetId) {
			setTitle = qsl("Custom stickers");
			setFlags |= MTPDstickerSet_ClientFlag::f_special;
		} else if (setId == Stickers::CloudRecentSetId) {
			setTitle = lang(lng_recent_stickers);
			setFlags |= MTPDstickerSet_ClientFlag::f_special;
		} else if (setId == Stickers::FavedSetId) {
			setTitle = lang(lng_faved_stickers);
			setFlags |= MTPDstickerSet_ClientFlag::f_special;
		} else if (setId) {
			if (readingInstalled && outOrder && stickers.version < 9061) {
				outOrder->push_back(setId);
			}
		} else {
			continue;
		}

		auto it = sets.find(setId);
		if (it == sets.cend()) {
			// We will set this flags from order lists when reading those stickers.
			setFlags &= ~(MTPDstickerSet::Flag::f_installed | MTPDstickerSet_ClientFlag::f_featured);
			it = sets.insert(setId, Stickers::Set(setId, setAccess, setTitle, setShortName, 0, setHash, MTPDstickerSet::Flags(setFlags)));
		}
		auto &set = it.value();
		auto inputSet = MTP_inputStickerSetID(MTP_long(set.id), MTP_long(set.access));

		if (scnt < 0) { // disabled not loaded set
			if (!set.count || set.stickers.isEmpty()) {
				set.count = -scnt;
			}
			continue;
		}

		bool fillStickers = set.stickers.isEmpty();
		if (fillStickers) {
			set.stickers.reserve(scnt);
			set.count = 0;
		}

		Serialize::Document::StickerSetInfo info(setId, setAccess, setShortName);
		OrderedSet<DocumentId> read;
		for (int32_t j = 0; j < scnt; ++j) {
			auto document = Serialize::Document::readStickerFromStream(stickers.version, stickers.stream, info);
			if (!document || !document->sticker()) continue;

			if (read.contains(document->id)) continue;
			read.insert(document->id);

			if (fillStickers) {
				set.stickers.push_back(document);
				if (!(set.flags & MTPDstickerSet_ClientFlag::f_special)) {
					if (document->sticker()->set.type() != mtpc_inputStickerSetID) {
						document->sticker()->set = inputSet;
					}
				}
				++set.count;
			}
		}

		if (stickers.version > 9018) {
			int32_t emojiCount;
			stickers.stream >> emojiCount;
			for (int32_t j = 0; j < emojiCount; ++j) {
				QString emojiString;
				int32_t stickersCount;
				stickers.stream >> emojiString >> stickersCount;
				StickerPack pack;
				pack.reserve(stickersCount);
				for (int32_t k = 0; k < stickersCount; ++k) {
					uint64_t id;
					stickers.stream >> id;
					DocumentData *doc = App::document(id);
					if (!doc || !doc->sticker()) continue;

					pack.push_back(doc);
				}
				if (fillStickers) {
					if (auto emoji = Ui::Emoji::Find(emojiString)) {
						emoji = emoji->original();
						set.emoji.insert(emoji, pack);
					}
				}
			}
		}
	}

	// Read orders of installed and featured stickers.
	if (outOrder && stickers.version >= 9061) {
		stickers.stream >> *outOrder;
	}

	// Set flags that we dropped above from the order.
	if (readingFlags && outOrder) {
		for_const (auto setId, *outOrder) {
			auto it = sets.find(setId);
			if (it != sets.cend()) {
				it->flags |= readingFlags;
			}
		}
	}
}

void writeInstalledStickers() {
	if (!Global::started()) return;

	_writeStickerSets(_installedStickersKey, [](const Stickers::Set &set) {
		if (set.id == Stickers::CloudRecentSetId || set.id == Stickers::FavedSetId) { // separate files for them
			return StickerSetCheckResult::Skip;
		} else if (set.flags & MTPDstickerSet_ClientFlag::f_special) {
			if (set.stickers.isEmpty()) { // all other special are "installed"
				return StickerSetCheckResult::Skip;
			}
		} else if (!(set.flags & MTPDstickerSet::Flag::f_installed) || (set.flags & MTPDstickerSet::Flag::f_archived)) {
			return StickerSetCheckResult::Skip;
		} else if (set.flags & MTPDstickerSet_ClientFlag::f_not_loaded) { // waiting to receive
			return StickerSetCheckResult::Abort;
		} else if (set.stickers.isEmpty()) {
			return StickerSetCheckResult::Skip;
		}
		return StickerSetCheckResult::Write;
	}, Global::StickerSetsOrder());
}

void writeFeaturedStickers() {
	if (!Global::started()) return;

	_writeStickerSets(_featuredStickersKey, [](const Stickers::Set &set) {
		if (set.id == Stickers::CloudRecentSetId || set.id == Stickers::FavedSetId) { // separate files for them
			return StickerSetCheckResult::Skip;
		} else if (set.flags & MTPDstickerSet_ClientFlag::f_special) {
			return StickerSetCheckResult::Skip;
		} else if (!(set.flags & MTPDstickerSet_ClientFlag::f_featured)) {
			return StickerSetCheckResult::Skip;
		} else if (set.flags & MTPDstickerSet_ClientFlag::f_not_loaded) { // waiting to receive
			return StickerSetCheckResult::Abort;
		} else if (set.stickers.isEmpty()) {
			return StickerSetCheckResult::Skip;
		}
		return StickerSetCheckResult::Write;
	}, Global::FeaturedStickerSetsOrder());
}

void writeRecentStickers() {
	if (!Global::started()) return;

	_writeStickerSets(_recentStickersKey, [](const Stickers::Set &set) {
		if (set.id != Stickers::CloudRecentSetId || set.stickers.isEmpty()) {
			return StickerSetCheckResult::Skip;
		}
		return StickerSetCheckResult::Write;
	}, Stickers::Order());
}

void writeFavedStickers() {
	if (!Global::started()) return;

	_writeStickerSets(_favedStickersKey, [](const Stickers::Set &set) {
		if (set.id != Stickers::FavedSetId || set.stickers.isEmpty()) {
			return StickerSetCheckResult::Skip;
		}
		return StickerSetCheckResult::Write;
	}, Stickers::Order());
}

void writeArchivedStickers() {
	if (!Global::started()) return;

	_writeStickerSets(_archivedStickersKey, [](const Stickers::Set &set) {
		if (!(set.flags & MTPDstickerSet::Flag::f_archived) || set.stickers.isEmpty()) {
			return StickerSetCheckResult::Skip;
		}
		return StickerSetCheckResult::Write;
	}, Global::ArchivedStickerSetsOrder());
}

void importOldRecentStickers() {
	if (!_recentStickersKeyOld) return;

	FileReadDescriptor stickers;
	if (!readEncryptedFile(stickers, _recentStickersKeyOld)) {
		clearKey(_recentStickersKeyOld);
		_recentStickersKeyOld = 0;
		_writeMap();
		return;
	}

	auto &sets = Global::RefStickerSets();
	sets.clear();

	auto &order = Global::RefStickerSetsOrder();
	order.clear();

	auto &recent = cRefRecentStickers();
	recent.clear();

	auto &def = sets.insert(Stickers::DefaultSetId, Stickers::Set(Stickers::DefaultSetId, 0, lang(lng_stickers_default_set), QString(), 0, 0, MTPDstickerSet::Flag::f_official | MTPDstickerSet::Flag::f_installed | MTPDstickerSet_ClientFlag::f_special)).value();
	auto &custom = sets.insert(Stickers::CustomSetId, Stickers::Set(Stickers::CustomSetId, 0, qsl("Custom stickers"), QString(), 0, 0, MTPDstickerSet::Flag::f_installed | MTPDstickerSet_ClientFlag::f_special)).value();

	QMap<uint64_t, bool> read;
	while (!stickers.stream.atEnd()) {
		uint64_t id, access;
		QString name, mime, alt;
		int32_t date, dc, size, width, height, type;
		int16_t value;
		stickers.stream >> id >> value >> access >> date >> name >> mime >> dc >> size >> width >> height >> type;
		if (stickers.version >= 7021) {
			stickers.stream >> alt;
		}
		if (!value || read.contains(id)) continue;
		read.insert(id, true);

		QVector<MTPDocumentAttribute> attributes;
		if (!name.isEmpty()) attributes.push_back(MTP_documentAttributeFilename(MTP_string(name)));
		if (type == AnimatedDocument) {
			attributes.push_back(MTP_documentAttributeAnimated());
		} else if (type == StickerDocument) {
			attributes.push_back(MTP_documentAttributeSticker(MTP_flags(0), MTP_string(alt), MTP_inputStickerSetEmpty(), MTPMaskCoords()));
		}
		if (width > 0 && height > 0) {
			attributes.push_back(MTP_documentAttributeImageSize(MTP_int(width), MTP_int(height)));
		}

		DocumentData *doc = App::documentSet(id, 0, access, 0, date, attributes, mime, ImagePtr(), dc, size, StorageImageLocation());
		if (!doc->sticker()) continue;

		if (value > 0) {
			def.stickers.push_back(doc);
			++def.count;
		} else {
			custom.stickers.push_back(doc);
			++custom.count;
		}
		if (recent.size() < Global::StickersRecentLimit() && qAbs(value) > 1) {
			recent.push_back(qMakePair(doc, qAbs(value)));
		}
	}
	if (def.stickers.isEmpty()) {
		sets.remove(Stickers::DefaultSetId);
	} else {
		order.push_front(Stickers::DefaultSetId);
	}
	if (custom.stickers.isEmpty()) sets.remove(Stickers::CustomSetId);

	writeInstalledStickers();
	writeUserSettings();

	clearKey(_recentStickersKeyOld);
	_recentStickersKeyOld = 0;
	_writeMap();
}

void readInstalledStickers() {
	if (!_installedStickersKey) {
		return importOldRecentStickers();
	}

	Global::RefStickerSets().clear();
	_readStickerSets(_installedStickersKey, &Global::RefStickerSetsOrder(), MTPDstickerSet::Flag::f_installed);
}

void readFeaturedStickers() {
	_readStickerSets(_featuredStickersKey, &Global::RefFeaturedStickerSetsOrder(), MTPDstickerSet::Flags() | MTPDstickerSet_ClientFlag::f_featured);

	auto &sets = Global::StickerSets();
	int unreadCount = 0;
	for_const (auto setId, Global::FeaturedStickerSetsOrder()) {
		auto it = sets.constFind(setId);
		if (it != sets.cend() && (it->flags & MTPDstickerSet_ClientFlag::f_unread)) {
			++unreadCount;
		}
	}
	if (Global::FeaturedStickerSetsUnreadCount() != unreadCount) {
		Global::SetFeaturedStickerSetsUnreadCount(unreadCount);
		Global::RefFeaturedStickerSetsUnreadCountChanged().notify();
	}
}

void readRecentStickers() {
	_readStickerSets(_recentStickersKey);
}

void readFavedStickers() {
	_readStickerSets(_favedStickersKey);
}

void readArchivedStickers() {
	static bool archivedStickersRead = false;
	if (!archivedStickersRead) {
		_readStickerSets(_archivedStickersKey, &Global::RefArchivedStickerSetsOrder());
		archivedStickersRead = true;
	}
}

int32_t countDocumentVectorHash(const QVector<DocumentData*> vector) {
	uint32_t acc = 0;
	for_const (auto doc, vector) {
		auto docId = doc->id;
		acc = (acc * 20261) + uint32_t(docId >> 32);
		acc = (acc * 20261) + uint32_t(docId & 0xFFFFFFFF);
	}
	return int32_t(acc & 0x7FFFFFFF);
}

int32_t countSpecialStickerSetHash(uint64_t setId) {
	auto &sets = Global::StickerSets();
	auto it = sets.constFind(setId);
	if (it != sets.cend()) {
		return countDocumentVectorHash(it->stickers);
	}
	return 0;
}

int32_t countStickersHash(bool checkOutdatedInfo) {
	uint32_t acc = 0;
	bool foundOutdated = false;
	auto &sets = Global::StickerSets();
	auto &order = Global::StickerSetsOrder();
	for (auto i = order.cbegin(), e = order.cend(); i != e; ++i) {
		auto j = sets.constFind(*i);
		if (j != sets.cend()) {
			if (j->id == Stickers::DefaultSetId) {
				foundOutdated = true;
			} else if (!(j->flags & MTPDstickerSet_ClientFlag::f_special)
				&& !(j->flags & MTPDstickerSet::Flag::f_archived)) {
				acc = (acc * 20261) + j->hash;
			}
		}
	}
	return (!checkOutdatedInfo || !foundOutdated) ? int32_t(acc & 0x7FFFFFFF) : 0;
}

int32_t countRecentStickersHash() {
	return countSpecialStickerSetHash(Stickers::CloudRecentSetId);
}

int32_t countFavedStickersHash() {
	return countSpecialStickerSetHash(Stickers::FavedSetId);
}

int32_t countFeaturedStickersHash() {
	uint32_t acc = 0;
	auto &sets = Global::StickerSets();
	auto &featured = Global::FeaturedStickerSetsOrder();
	for_const (auto setId, featured) {
		acc = (acc * 20261) + uint32_t(setId >> 32);
		acc = (acc * 20261) + uint32_t(setId & 0xFFFFFFFF);

		auto it = sets.constFind(setId);
		if (it != sets.cend() && (it->flags & MTPDstickerSet_ClientFlag::f_unread)) {
			acc = (acc * 20261) + 1U;
		}
	}
	return int32_t(acc & 0x7FFFFFFF);
}

int32_t countSavedGifsHash() {
	return countDocumentVectorHash(cSavedGifs());
}

void writeSavedGifs() {
	if (!_working()) return;

	auto &saved = cSavedGifs();
	if (saved.isEmpty()) {
		if (_savedGifsKey) {
			clearKey(_savedGifsKey);
			_savedGifsKey = 0;
			_mapChanged = true;
		}
		_writeMap();
	} else {
		uint32_t size = sizeof(uint32_t); // count
		for_const (auto gif, saved) {
			size += Serialize::Document::sizeInStream(gif);
		}

		if (!_savedGifsKey) {
			_savedGifsKey = genKey();
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}
		EncryptedDescriptor data(size);
		data.stream << uint32_t(saved.size());
		for_const (auto gif, saved) {
			Serialize::Document::writeToStream(data.stream, gif);
		}
		FileWriteDescriptor file(_savedGifsKey);
		file.writeEncrypted(data);
	}
}

void readSavedGifs() {
	if (!_savedGifsKey) return;

	FileReadDescriptor gifs;
	if (!readEncryptedFile(gifs, _savedGifsKey)) {
		clearKey(_savedGifsKey);
		_savedGifsKey = 0;
		_writeMap();
		return;
	}

	SavedGifs &saved(cRefSavedGifs());
	saved.clear();

	uint32_t cnt;
	gifs.stream >> cnt;
	saved.reserve(cnt);
	OrderedSet<DocumentId> read;
	for (uint32_t i = 0; i < cnt; ++i) {
		auto document = Serialize::Document::readFromStream(gifs.version, gifs.stream);
		if (!document || !document->isGifv()) continue;

		if (read.contains(document->id)) continue;
		read.insert(document->id);

		saved.push_back(document);
	}
}

void writeBackground(int32_t id, const QImage &img) {
	if (!_working() || !_backgroundCanWrite) return;

	if (!LocalKey) {
		LOG(("App Error: localkey not created in writeBackground()"));
		return;
	}

	QByteArray bmp;
	if (!img.isNull()) {
		QBuffer buf(&bmp);
		if (!img.save(&buf, "BMP")) return;
	}
	if (!_backgroundKey) {
		_backgroundKey = genKey();
		_mapChanged = true;
		_writeMap(WriteMapWhen::Fast);
	}
	uint32_t size = sizeof(int32_t) + sizeof(uint32_t) + (bmp.isEmpty() ? 0 : (sizeof(uint32_t) + bmp.size()));
	EncryptedDescriptor data(size);
	data.stream << int32_t(id) << bmp;

	FileWriteDescriptor file(_backgroundKey);
	file.writeEncrypted(data);
}

bool readBackground() {
	if (_backgroundWasRead) {
		return false;
	}
	_backgroundWasRead = true;

	FileReadDescriptor bg;
	if (!readEncryptedFile(bg, _backgroundKey)) {
		clearKey(_backgroundKey);
		_backgroundKey = 0;
		_writeMap();
		return false;
	}

	QByteArray pngData;
	int32_t id;
	bg.stream >> id >> pngData;
	auto oldEmptyImage = (bg.stream.status() != QDataStream::Ok);
	if (oldEmptyImage
		|| id == Window::Theme::kInitialBackground
		|| id == Window::Theme::kDefaultBackground) {
		_backgroundCanWrite = false;
		if (oldEmptyImage || bg.version < 8005) {
			Window::Theme::Background()->setImage(Window::Theme::kDefaultBackground);
			Window::Theme::Background()->setTile(false);
		} else {
			Window::Theme::Background()->setImage(id);
		}
		_backgroundCanWrite = true;
		return true;
	} else if (id == Window::Theme::kThemeBackground && pngData.isEmpty()) {
		_backgroundCanWrite = false;
		Window::Theme::Background()->setImage(id);
		_backgroundCanWrite = true;
		return true;
	}

	QImage image;
	QBuffer buf(&pngData);
	QImageReader reader(&buf);
#ifndef OS_MAC_OLD
	reader.setAutoTransform(true);
#endif // OS_MAC_OLD
	if (reader.read(&image)) {
		_backgroundCanWrite = false;
		Window::Theme::Background()->setImage(id, std::move(image));
		_backgroundCanWrite = true;
		return true;
	}
	return false;
}

bool readThemeUsingKey(FileKey key) {
	FileReadDescriptor theme;
	if (!readEncryptedFile(theme, key, FileOption::Safe, SettingsKey)) {
		return false;
	}

	QByteArray themeContent;
	QString pathRelative, pathAbsolute;
	Window::Theme::Cached cache;
	theme.stream >> themeContent;
	theme.stream >> pathRelative >> pathAbsolute;
	if (theme.stream.status() != QDataStream::Ok) {
		return false;
	}

	_themeAbsolutePath = pathAbsolute;
	_themePaletteAbsolutePath = Window::Theme::IsPaletteTestingPath(pathAbsolute) ? pathAbsolute : QString();

	QFile file(pathRelative);
	if (pathRelative.isEmpty() || !file.exists()) {
		file.setFileName(pathAbsolute);
	}

	auto changed = false;
	if (!file.fileName().isEmpty() && file.exists() && file.open(QIODevice::ReadOnly)) {
		if (file.size() > kThemeFileSizeLimit) {
			LOG(("Error: theme file too large: %1 (should be less than 5 MB, got %2)").arg(file.fileName()).arg(file.size()));
			return false;
		}
		auto fileContent = file.readAll();
		file.close();
		if (themeContent != fileContent) {
			themeContent = fileContent;
			changed = true;
		}
	}
	if (!changed) {
		uint32_t backgroundIsTiled = 0;
		theme.stream >> cache.paletteChecksum >> cache.contentChecksum >> cache.colors >> cache.background >> backgroundIsTiled;
		cache.tiled = (backgroundIsTiled == 1);
		if (theme.stream.status() != QDataStream::Ok) {
			return false;
		}
	}
	return Window::Theme::Load(pathRelative, pathAbsolute, themeContent, cache);
}

void writeTheme(const QString &pathRelative, const QString &pathAbsolute, const QByteArray &content, const Window::Theme::Cached &cache) {
	if (content.isEmpty()) {
		_themeAbsolutePath = _themePaletteAbsolutePath = QString();
		if (_themeKey) {
			clearKey(_themeKey);
			_themeKey = 0;
			writeSettings();
		}
		return;
	}

	_themeAbsolutePath = pathAbsolute;
	_themePaletteAbsolutePath = Window::Theme::IsPaletteTestingPath(pathAbsolute) ? pathAbsolute : QString();
	if (!_themeKey) {
		_themeKey = genKey(FileOption::Safe);
		writeSettings();
	}

	auto backgroundTiled = static_cast<uint32_t>(cache.tiled ? 1 : 0);
	uint32_t size = Serialize::bytearraySize(content);
	size += Serialize::stringSize(pathRelative) + Serialize::stringSize(pathAbsolute);
	size += sizeof(int32_t) * 2 + Serialize::bytearraySize(cache.colors) + Serialize::bytearraySize(cache.background) + sizeof(uint32_t);
	EncryptedDescriptor data(size);
	data.stream << content;
	data.stream << pathRelative << pathAbsolute;
	data.stream << cache.paletteChecksum << cache.contentChecksum << cache.colors << cache.background << backgroundTiled;

	FileWriteDescriptor file(_themeKey, FileOption::Safe);
	file.writeEncrypted(data, SettingsKey);
}

void clearTheme() {
	writeTheme(QString(), QString(), QByteArray(), Window::Theme::Cached());
}

void readTheme() {
	if (_themeKey && !readThemeUsingKey(_themeKey)) {
		clearTheme();
	}
}

bool hasTheme() {
	return (_themeKey != 0);
}

void readLangPack() {
	FileReadDescriptor langpack;
	if (!_langPackKey || !readEncryptedFile(langpack, _langPackKey, FileOption::Safe, SettingsKey)) {
		return;
	}
	auto data = QByteArray();
	langpack.stream >> data;
	if (langpack.stream.status() == QDataStream::Ok) {
		Lang::Current().fillFromSerialized(data);
	}
}

void writeLangPack() {
	auto langpack = Lang::Current().serialize();
	if (!_langPackKey) {
		_langPackKey = genKey(FileOption::Safe);
		writeSettings();
	}

	EncryptedDescriptor data(Serialize::bytearraySize(langpack));
	data.stream << langpack;

	FileWriteDescriptor file(_langPackKey, FileOption::Safe);
	file.writeEncrypted(data, SettingsKey);
}

QString themePaletteAbsolutePath() {
	return _themePaletteAbsolutePath;
}

QString themeAbsolutePath() {
	return _themeAbsolutePath;
}

bool copyThemeColorsToPalette(const QString &path) {
	if (!_themeKey) {
		return false;
	}

	FileReadDescriptor theme;
	if (!readEncryptedFile(theme, _themeKey, FileOption::Safe, SettingsKey)) {
		return false;
	}

	QByteArray themeContent;
	theme.stream >> themeContent;
	if (theme.stream.status() != QDataStream::Ok) {
		return false;
	}

	return Window::Theme::CopyColorsToPalette(path, themeContent);
}

uint32_t _peerSize(PeerData *peer) {
	uint32_t result = sizeof(uint64_t) + sizeof(uint64_t) + Serialize::storageImageLocationSize();
	if (peer->isUser()) {
		UserData *user = peer->asUser();

		// first + last + phone + username + access
		result += Serialize::stringSize(user->firstName) + Serialize::stringSize(user->lastName) + Serialize::stringSize(user->phone()) + Serialize::stringSize(user->username) + sizeof(uint64_t);

		// flags
		if (AppVersion >= 9012) {
			result += sizeof(int32_t);
		}

		// onlineTill + contact + botInfoVersion
		result += sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t);
	} else if (peer->isChat()) {
		ChatData *chat = peer->asChat();

		// name + count + date + version + admin + forbidden + left + inviteLink
		result += Serialize::stringSize(chat->name) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + Serialize::stringSize(chat->inviteLink());
	} else if (peer->isChannel()) {
		ChannelData *channel = peer->asChannel();

		// name + access + date + version + forbidden + flags + inviteLink
		result += Serialize::stringSize(channel->name) + sizeof(uint64_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + Serialize::stringSize(channel->inviteLink());
	}
	return result;
}

void _writePeer(QDataStream &stream, PeerData *peer) {
	stream << uint64_t(peer->id) << uint64_t(peer->photoId);
	Serialize::writeStorageImageLocation(stream, peer->photoLoc);
	if (peer->isUser()) {
		UserData *user = peer->asUser();

		stream << user->firstName << user->lastName << user->phone() << user->username << uint64_t(user->access);
		if (AppVersion >= 9012) {
			stream << int32_t(user->flags);
		}
		if (AppVersion >= 9016) {
			stream << (user->botInfo ? user->botInfo->inlinePlaceholder : QString());
		}
		stream << int32_t(user->onlineTill) << int32_t(user->contact) << int32_t(user->botInfo ? user->botInfo->version : -1);
	} else if (peer->isChat()) {
		ChatData *chat = peer->asChat();

		int32_t flagsData = (AppVersion >= 9012) ? chat->flags : (chat->haveLeft() ? 1 : 0);

		stream << chat->name << int32_t(chat->count) << int32_t(chat->date) << int32_t(chat->version) << int32_t(chat->creator);
		stream << int32_t(chat->isForbidden() ? 1 : 0) << int32_t(flagsData) << chat->inviteLink();
	} else if (peer->isChannel()) {
		ChannelData *channel = peer->asChannel();

		stream << channel->name << uint64_t(channel->access) << int32_t(channel->date) << int32_t(channel->version);
		stream << int32_t(channel->isForbidden() ? 1 : 0) << int32_t(channel->flags) << channel->inviteLink();
	}
}

PeerData *_readPeer(FileReadDescriptor &from, int32_t fileVersion = 0) {
	uint64_t peerId = 0, photoId = 0;
	from.stream >> peerId >> photoId;

	StorageImageLocation photoLoc(Serialize::readStorageImageLocation(from.stream));

	PeerData *result = App::peerLoaded(peerId);
	bool wasLoaded = (result != nullptr);
	if (!wasLoaded) {
		result = App::peer(peerId);
		result->loadedStatus = PeerData::FullLoaded;
	}
	if (result->isUser()) {
		UserData *user = result->asUser();

		QString first, last, phone, username, inlinePlaceholder;
		uint64_t access;
		int32_t flags = 0, onlineTill, contact, botInfoVersion;
		from.stream >> first >> last >> phone >> username >> access;
		if (from.version >= 9012) {
			from.stream >> flags;
		}
		if (from.version >= 9016 || fileVersion >= 9016) {
			from.stream >> inlinePlaceholder;
		}
		from.stream >> onlineTill >> contact >> botInfoVersion;

		bool showPhone = !isServiceUser(user->id) && (user->id != Auth().userPeerId()) && (contact <= 0);
		QString pname = (showPhone && !phone.isEmpty()) ? App::formatPhone(phone) : QString();

		if (!wasLoaded) {
			user->setPhone(phone);
			user->setName(first, last, pname, username);

			user->access = access;
			user->flags = MTPDuser::Flags(flags);
			user->onlineTill = onlineTill;
			user->contact = contact;
			user->setBotInfoVersion(botInfoVersion);
			if (!inlinePlaceholder.isEmpty() && user->botInfo) {
				user->botInfo->inlinePlaceholder = inlinePlaceholder;
			}

			if (user->id == Auth().userPeerId()) {
				user->input = MTP_inputPeerSelf();
				user->inputUser = MTP_inputUserSelf();
			} else {
				user->input = MTP_inputPeerUser(MTP_int(peerToUser(user->id)), MTP_long(user->isInaccessible() ? 0 : user->access));
				user->inputUser = MTP_inputUser(MTP_int(peerToUser(user->id)), MTP_long(user->isInaccessible() ? 0 : user->access));
			}

			user->setUserpic(photoLoc.isNull() ? ImagePtr() : ImagePtr(photoLoc));
		}
	} else if (result->isChat()) {
		ChatData *chat = result->asChat();

		QString name, inviteLink;
		int32_t count, date, version, creator, forbidden, flagsData, flags;
		from.stream >> name >> count >> date >> version >> creator >> forbidden >> flagsData >> inviteLink;

		if (from.version >= 9012) {
			flags = flagsData;
		} else {
			// flagsData was haveLeft
			flags = (flagsData == 1) ? MTPDchat::Flags(MTPDchat::Flag::f_left) : MTPDchat::Flags(0);
		}
		if (!wasLoaded) {
			chat->setName(name);
			chat->count = count;
			chat->date = date;
			chat->version = version;
			chat->creator = creator;
			chat->setIsForbidden(forbidden == 1);
			chat->flags = MTPDchat::Flags(flags);
			chat->setInviteLink(inviteLink);

			chat->input = MTP_inputPeerChat(MTP_int(peerToChat(chat->id)));
			chat->inputChat = MTP_int(peerToChat(chat->id));

			chat->setUserpic(photoLoc.isNull() ? ImagePtr() : ImagePtr(photoLoc));
		}
	} else if (result->isChannel()) {
		ChannelData *channel = result->asChannel();

		QString name, inviteLink;
		uint64_t access;
		int32_t date, version, forbidden, flags;
		from.stream >> name >> access >> date >> version >> forbidden >> flags >> inviteLink;

		if (!wasLoaded) {
			channel->setName(name, QString());
			channel->access = access;
			channel->date = date;
			channel->version = version;
			channel->setIsForbidden(forbidden == 1);
			channel->flags = MTPDchannel::Flags(flags);
			channel->setInviteLink(inviteLink);

			channel->input = MTP_inputPeerChannel(MTP_int(peerToChannel(channel->id)), MTP_long(access));
			channel->inputChannel = MTP_inputChannel(MTP_int(peerToChannel(channel->id)), MTP_long(access));

			channel->setUserpic(photoLoc.isNull() ? ImagePtr() : ImagePtr(photoLoc));
		}
	}
	return result;
}

void writeRecentHashtagsAndBots() {
	if (!_working()) return;

	const RecentHashtagPack &write(cRecentWriteHashtags()), &search(cRecentSearchHashtags());
	const RecentInlineBots &bots(cRecentInlineBots());
	if (write.isEmpty() && search.isEmpty() && bots.isEmpty()) readRecentHashtagsAndBots();
	if (write.isEmpty() && search.isEmpty() && bots.isEmpty()) {
		if (_recentHashtagsAndBotsKey) {
			clearKey(_recentHashtagsAndBotsKey);
			_recentHashtagsAndBotsKey = 0;
			_mapChanged = true;
		}
		_writeMap();
	} else {
		if (!_recentHashtagsAndBotsKey) {
			_recentHashtagsAndBotsKey = genKey();
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}
		uint32_t size = sizeof(uint32_t) * 3, writeCnt = 0, searchCnt = 0, botsCnt = cRecentInlineBots().size();
		for (RecentHashtagPack::const_iterator i = write.cbegin(), e = write.cend(); i != e;  ++i) {
			if (!i->first.isEmpty()) {
				size += Serialize::stringSize(i->first) + sizeof(uint16_t);
				++writeCnt;
			}
		}
		for (RecentHashtagPack::const_iterator i = search.cbegin(), e = search.cend(); i != e; ++i) {
			if (!i->first.isEmpty()) {
				size += Serialize::stringSize(i->first) + sizeof(uint16_t);
				++searchCnt;
			}
		}
		for (RecentInlineBots::const_iterator i = bots.cbegin(), e = bots.cend(); i != e; ++i) {
			size += _peerSize(*i);
		}

		EncryptedDescriptor data(size);
		data.stream << uint32_t(writeCnt) << uint32_t(searchCnt);
		for (RecentHashtagPack::const_iterator i = write.cbegin(), e = write.cend(); i != e; ++i) {
			if (!i->first.isEmpty()) data.stream << i->first << uint16_t(i->second);
		}
		for (RecentHashtagPack::const_iterator i = search.cbegin(), e = search.cend(); i != e; ++i) {
			if (!i->first.isEmpty()) data.stream << i->first << uint16_t(i->second);
		}
		data.stream << uint32_t(botsCnt);
		for (RecentInlineBots::const_iterator i = bots.cbegin(), e = bots.cend(); i != e; ++i) {
			_writePeer(data.stream, *i);
		}
		FileWriteDescriptor file(_recentHashtagsAndBotsKey);
		file.writeEncrypted(data);
	}
}

void readRecentHashtagsAndBots() {
	if (_recentHashtagsAndBotsWereRead) return;
	_recentHashtagsAndBotsWereRead = true;

	if (!_recentHashtagsAndBotsKey) return;

	FileReadDescriptor hashtags;
	if (!readEncryptedFile(hashtags, _recentHashtagsAndBotsKey)) {
		clearKey(_recentHashtagsAndBotsKey);
		_recentHashtagsAndBotsKey = 0;
		_writeMap();
		return;
	}

	uint32_t writeCount = 0, searchCount = 0, botsCount = 0;
	hashtags.stream >> writeCount >> searchCount;

	QString tag;
	uint16_t count;

	RecentHashtagPack write, search;
	RecentInlineBots bots;
	if (writeCount) {
		write.reserve(writeCount);
		for (uint32_t i = 0; i < writeCount; ++i) {
			hashtags.stream >> tag >> count;
			write.push_back(qMakePair(tag.trimmed(), count));
		}
	}
	if (searchCount) {
		search.reserve(searchCount);
		for (uint32_t i = 0; i < searchCount; ++i) {
			hashtags.stream >> tag >> count;
			search.push_back(qMakePair(tag.trimmed(), count));
		}
	}
	cSetRecentWriteHashtags(write);
	cSetRecentSearchHashtags(search);

	if (!hashtags.stream.atEnd()) {
		hashtags.stream >> botsCount;
		if (botsCount) {
			bots.reserve(botsCount);
			for (uint32_t i = 0; i < botsCount; ++i) {
				PeerData *peer = _readPeer(hashtags, 9016);
				if (peer && peer->isUser() && peer->asUser()->botInfo && !peer->asUser()->botInfo->inlinePlaceholder.isEmpty() && !peer->asUser()->username.isEmpty()) {
					bots.push_back(peer->asUser());
				}
			}
		}
		cSetRecentInlineBots(bots);
	}
}

void writeSavedPeers() {
	if (!_working()) return;

	const SavedPeers &saved(cSavedPeers());
	if (saved.isEmpty()) {
		if (_savedPeersKey) {
			clearKey(_savedPeersKey);
			_savedPeersKey = 0;
			_mapChanged = true;
		}
		_writeMap();
	} else {
		if (!_savedPeersKey) {
			_savedPeersKey = genKey();
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}
		uint32_t size = sizeof(uint32_t);
		for (SavedPeers::const_iterator i = saved.cbegin(); i != saved.cend(); ++i) {
			size += _peerSize(i.key()) + Serialize::dateTimeSize();
		}

		EncryptedDescriptor data(size);
		data.stream << uint32_t(saved.size());
		for (SavedPeers::const_iterator i = saved.cbegin(); i != saved.cend(); ++i) {
			_writePeer(data.stream, i.key());
			data.stream << i.value();
		}

		FileWriteDescriptor file(_savedPeersKey);
		file.writeEncrypted(data);
	}
}

void readSavedPeers() {
	if (!_savedPeersKey) return;

	FileReadDescriptor saved;
	if (!readEncryptedFile(saved, _savedPeersKey)) {
		clearKey(_savedPeersKey);
		_savedPeersKey = 0;
		_writeMap();
		return;
	}
	if (saved.version == 9011) { // broken dev version
		clearKey(_savedPeersKey);
		_savedPeersKey = 0;
		_writeMap();
		return;
	}

	uint32_t count = 0;
	saved.stream >> count;
	cRefSavedPeers().clear();
	cRefSavedPeersByTime().clear();
	QList<PeerData*> peers;
	peers.reserve(count);
	for (uint32_t i = 0; i < count; ++i) {
		PeerData *peer = _readPeer(saved);
		if (!peer) break;

		QDateTime t;
		saved.stream >> t;

		cRefSavedPeers().insert(peer, t);
		cRefSavedPeersByTime().insert(t, peer);
		peers.push_back(peer);
	}

	Auth().api().requestPeers(peers);
}

void addSavedPeer(PeerData *peer, const QDateTime &position) {
	auto &savedPeers = cRefSavedPeers();
	auto i = savedPeers.find(peer);
	if (i == savedPeers.cend()) {
		savedPeers.insert(peer, position);
	} else if (i.value() != position) {
		cRefSavedPeersByTime().remove(i.value(), peer);
		i.value() = position;
		cRefSavedPeersByTime().insert(i.value(), peer);
	}
	writeSavedPeers();
}

void removeSavedPeer(PeerData *peer) {
	auto &savedPeers = cRefSavedPeers();
	if (savedPeers.isEmpty()) return;

	auto i = savedPeers.find(peer);
	if (i != savedPeers.cend()) {
		cRefSavedPeersByTime().remove(i.value(), peer);
		savedPeers.erase(i);

		writeSavedPeers();
	}
}

void writeReportSpamStatuses() {
	_writeReportSpamStatuses();
}

void writeTrustedBots() {
	if (!_working()) return;

	if (_trustedBots.isEmpty()) {
		if (_trustedBotsKey) {
			clearKey(_trustedBotsKey);
			_trustedBotsKey = 0;
			_mapChanged = true;
			_writeMap();
		}
	} else {
		if (!_trustedBotsKey) {
			_trustedBotsKey = genKey();
			_mapChanged = true;
			_writeMap(WriteMapWhen::Fast);
		}
		uint32_t size = sizeof(int32_t) + _trustedBots.size() * sizeof(uint64_t);
		EncryptedDescriptor data(size);
		data.stream << int32_t(_trustedBots.size());
		for_const (auto botId, _trustedBots) {
			data.stream << uint64_t(botId);
		}

		FileWriteDescriptor file(_trustedBotsKey);
		file.writeEncrypted(data);
	}
}

void readTrustedBots() {
	if (!_trustedBotsKey) return;

	FileReadDescriptor trusted;
	if (!readEncryptedFile(trusted, _trustedBotsKey)) {
		clearKey(_trustedBotsKey);
		_trustedBotsKey = 0;
		_writeMap();
		return;
	}

	int32_t size = 0;
	trusted.stream >> size;
	for (int i = 0; i < size; ++i) {
		uint64_t botId = 0;
		trusted.stream >> botId;
		_trustedBots.insert(botId);
	}
}

void makeBotTrusted(UserData *bot) {
	if (!isBotTrusted(bot)) {
		_trustedBots.insert(bot->id);
		writeTrustedBots();
	}
}

bool isBotTrusted(UserData *bot) {
	if (!_trustedBotsRead) {
		readTrustedBots();
		_trustedBotsRead = true;
	}
	return _trustedBots.contains(bot->id);
}

bool encrypt(const void *src, void *dst, uint32_t len, const void *key128) {
	if (!LocalKey) {
		return false;
	}
	MTP::aesEncryptLocal(src, dst, len, LocalKey, key128);
	return true;
}

bool decrypt(const void *src, void *dst, uint32_t len, const void *key128) {
	if (!LocalKey) {
		return false;
	}
	MTP::aesDecryptLocal(src, dst, len, LocalKey, key128);
	return true;
}

struct ClearManagerData {
	QThread *thread;
	StorageMap images, stickers, audios;
	WebFilesMap webFiles;
	QMutex mutex;
	QList<int> tasks;
	bool working;
};

ClearManager::ClearManager() : data(new ClearManagerData()) {
	data->thread = new QThread();
	data->working = true;
}

bool ClearManager::addTask(int task) {
	QMutexLocker lock(&data->mutex);
	if (!data->working) return false;

	if (!data->tasks.isEmpty() && (data->tasks.at(0) == ClearManagerAll)) return true;
	if (task == ClearManagerAll) {
		data->tasks.clear();
		if (!_imagesMap.isEmpty()) {
			_imagesMap.clear();
			_storageImagesSize = 0;
			_mapChanged = true;
		}
		if (!_stickerImagesMap.isEmpty()) {
			_stickerImagesMap.clear();
			_storageStickersSize = 0;
			_mapChanged = true;
		}
		if (!_audiosMap.isEmpty()) {
			_audiosMap.clear();
			_storageAudiosSize = 0;
			_mapChanged = true;
		}
		if (!_draftsMap.isEmpty()) {
			_draftsMap.clear();
			_mapChanged = true;
		}
		if (!_draftCursorsMap.isEmpty()) {
			_draftCursorsMap.clear();
			_mapChanged = true;
		}
		if (_locationsKey) {
			_locationsKey = 0;
			_mapChanged = true;
		}
		if (_reportSpamStatusesKey) {
			_reportSpamStatusesKey = 0;
			_mapChanged = true;
		}
		if (_trustedBotsKey) {
			_trustedBotsKey = 0;
			_mapChanged = true;
		}
		if (_recentStickersKeyOld) {
			_recentStickersKeyOld = 0;
			_mapChanged = true;
		}
		if (_installedStickersKey || _featuredStickersKey || _recentStickersKey || _archivedStickersKey) {
			_installedStickersKey = _featuredStickersKey = _recentStickersKey = _archivedStickersKey = 0;
			_mapChanged = true;
		}
		if (_recentHashtagsAndBotsKey) {
			_recentHashtagsAndBotsKey = 0;
			_mapChanged = true;
		}
		if (_savedPeersKey) {
			_savedPeersKey = 0;
			_mapChanged = true;
		}
		_writeMap();
	} else {
		if (task & ClearManagerStorage) {
			if (data->images.isEmpty()) {
				data->images = _imagesMap;
			} else {
				for (StorageMap::const_iterator i = _imagesMap.cbegin(), e = _imagesMap.cend(); i != e; ++i) {
					StorageKey k = i.key();
					while (data->images.constFind(k) != data->images.cend()) {
						++k.second;
					}
					data->images.insert(k, i.value());
				}
			}
			if (!_imagesMap.isEmpty()) {
				_imagesMap.clear();
				_storageImagesSize = 0;
				_mapChanged = true;
			}
			if (data->stickers.isEmpty()) {
				data->stickers = _stickerImagesMap;
			} else {
				for (StorageMap::const_iterator i = _stickerImagesMap.cbegin(), e = _stickerImagesMap.cend(); i != e; ++i) {
					StorageKey k = i.key();
					while (data->stickers.constFind(k) != data->stickers.cend()) {
						++k.second;
					}
					data->stickers.insert(k, i.value());
				}
			}
			if (!_stickerImagesMap.isEmpty()) {
				_stickerImagesMap.clear();
				_storageStickersSize = 0;
				_mapChanged = true;
			}
			if (data->webFiles.isEmpty()) {
				data->webFiles = _webFilesMap;
			} else {
				for (WebFilesMap::const_iterator i = _webFilesMap.cbegin(), e = _webFilesMap.cend(); i != e; ++i) {
					QString k = i.key();
					while (data->webFiles.constFind(k) != data->webFiles.cend()) {
						k += '#';
					}
					data->webFiles.insert(k, i.value());
				}
			}
			if (!_webFilesMap.isEmpty()) {
				_webFilesMap.clear();
				_storageWebFilesSize = 0;
				_writeLocations();
			}
			if (data->audios.isEmpty()) {
				data->audios = _audiosMap;
			} else {
				for (StorageMap::const_iterator i = _audiosMap.cbegin(), e = _audiosMap.cend(); i != e; ++i) {
					StorageKey k = i.key();
					while (data->audios.constFind(k) != data->audios.cend()) {
						++k.second;
					}
					data->audios.insert(k, i.value());
				}
			}
			if (!_audiosMap.isEmpty()) {
				_audiosMap.clear();
				_storageAudiosSize = 0;
				_mapChanged = true;
			}
			_writeMap();
		}
		for (int32_t i = 0, l = data->tasks.size(); i < l; ++i) {
			if (data->tasks.at(i) == task) return true;
		}
	}
	data->tasks.push_back(task);
	return true;
}

bool ClearManager::hasTask(ClearManagerTask task) {
	QMutexLocker lock(&data->mutex);
	if (data->tasks.isEmpty()) return false;
	if (data->tasks.at(0) == ClearManagerAll) return true;
	for (int32_t i = 0, l = data->tasks.size(); i < l; ++i) {
		if (data->tasks.at(i) == task) return true;
	}
	return false;
}

void ClearManager::start() {
	moveToThread(data->thread);
	connect(data->thread, SIGNAL(started()), this, SLOT(onStart()));
	connect(data->thread, SIGNAL(finished()), data->thread, SLOT(deleteLater()));
	connect(data->thread, SIGNAL(finished()), this, SLOT(deleteLater()));
	data->thread->start();
}

void ClearManager::stop() {
	{
		QMutexLocker lock(&data->mutex);
		data->tasks.clear();
	}
	auto thread = data->thread;
	thread->quit();
	thread->wait();
}

ClearManager::~ClearManager() {
	delete data;
}

void ClearManager::onStart() {
	while (true) {
		int task = 0;
		bool result = false;
		StorageMap images, stickers, audios;
		WebFilesMap webFiles;
		{
			QMutexLocker lock(&data->mutex);
			if (data->tasks.isEmpty()) {
				data->working = false;
				break;
			}
			task = data->tasks.at(0);
			images = data->images;
			stickers = data->stickers;
			audios = data->audios;
			webFiles = data->webFiles;
		}
		switch (task) {
		case ClearManagerAll: {
			result = QDir(cTempDir()).removeRecursively();
			QDirIterator di(_userBasePath, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
			while (di.hasNext()) {
				di.next();
				const QFileInfo& fi = di.fileInfo();
				if (fi.isDir() && !fi.isSymLink()) {
					if (!QDir(di.filePath()).removeRecursively()) result = false;
				} else {
					QString path = di.filePath();
					if (!path.endsWith(qstr("map0")) && !path.endsWith(qstr("map1"))) {
						if (!QFile::remove(di.filePath())) result = false;
					}
				}
			}
		} break;
		case ClearManagerDownloads:
			result = QDir(cTempDir()).removeRecursively();
		break;
		case ClearManagerStorage:
			for (StorageMap::const_iterator i = images.cbegin(), e = images.cend(); i != e; ++i) {
				clearKey(i.value().first, FileOption::User);
			}
			for (StorageMap::const_iterator i = stickers.cbegin(), e = stickers.cend(); i != e; ++i) {
				clearKey(i.value().first, FileOption::User);
			}
			for (StorageMap::const_iterator i = audios.cbegin(), e = audios.cend(); i != e; ++i) {
				clearKey(i.value().first, FileOption::User);
			}
			for (WebFilesMap::const_iterator i = webFiles.cbegin(), e = webFiles.cend(); i != e; ++i) {
				clearKey(i.value().first, FileOption::User);
			}
			result = true;
		break;
		}
		{
			QMutexLocker lock(&data->mutex);
			if (!data->tasks.isEmpty() && data->tasks.at(0) == task) {
				data->tasks.pop_front();
			}
			if (data->tasks.isEmpty()) {
				data->working = false;
			}
			if (result) {
				emit succeed(task, data->working ? 0 : this);
			} else {
				emit failed(task, data->working ? 0 : this);
			}
			if (!data->working) break;
		}
	}
}

namespace internal {

Manager::Manager() {
	_mapWriteTimer.setSingleShot(true);
	connect(&_mapWriteTimer, SIGNAL(timeout()), this, SLOT(mapWriteTimeout()));
	_locationsWriteTimer.setSingleShot(true);
	connect(&_locationsWriteTimer, SIGNAL(timeout()), this, SLOT(locationsWriteTimeout()));
}

void Manager::writeMap(bool fast) {
	if (!_mapWriteTimer.isActive() || fast) {
		_mapWriteTimer.start(fast ? 1 : WriteMapTimeout);
	} else if (_mapWriteTimer.remainingTime() <= 0) {
		mapWriteTimeout();
	}
}

void Manager::writingMap() {
	_mapWriteTimer.stop();
}

void Manager::writeLocations(bool fast) {
	if (!_locationsWriteTimer.isActive() || fast) {
		_locationsWriteTimer.start(fast ? 1 : WriteMapTimeout);
	} else if (_locationsWriteTimer.remainingTime() <= 0) {
		locationsWriteTimeout();
	}
}

void Manager::writingLocations() {
	_locationsWriteTimer.stop();
}

void Manager::mapWriteTimeout() {
	_writeMap(WriteMapWhen::Now);
}

void Manager::locationsWriteTimeout() {
	_writeLocations(WriteMapWhen::Now);
}

void Manager::finish() {
	if (_mapWriteTimer.isActive()) {
		mapWriteTimeout();
	}
	if (_locationsWriteTimer.isActive()) {
		locationsWriteTimeout();
	}
}

} // namespace internal
} // namespace Local
