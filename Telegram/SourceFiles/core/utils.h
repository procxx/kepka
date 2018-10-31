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

#include <QByteArray>
#include <QDateTime>
#include <QFileInfo>
#include <QMimeType>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QStringList>
#include <set>

#include "base/assertion.h"
#include "base/flags.h"
#include "core/basic_types.h"
#include "logs.h"

// Define specializations for QByteArray for Qt 5.3.2, because
// QByteArray in Qt 5.3.2 doesn't declare "pointer" subtype.
#ifdef OS_MAC_OLD
namespace gsl {

template <> inline span<char> make_span<QByteArray>(QByteArray &cont) {
	return span<char>(cont.data(), cont.size());
}

template <> inline span<const char> make_span(const QByteArray &cont) {
	return span<const char>(cont.constData(), cont.size());
}

} // namespace gsl
#endif // OS_MAC_OLD

namespace base {

template <typename T, size_t N> inline constexpr size_t array_size(const T (&)[N]) {
	return N;
}

template <typename T> inline T take(T &source) {
	return std::exchange(source, T());
}

namespace internal {

template <typename D, typename T> inline constexpr D up_cast_helper(std::true_type, T object) {
	return object;
}

template <typename D, typename T> inline constexpr D up_cast_helper(std::false_type, T object) {
	return nullptr;
}

} // namespace internal

template <typename D, typename T> inline constexpr D up_cast(T object) {
	using DV = std::decay_t<decltype(*D())>;
	using TV = std::decay_t<decltype(*T())>;
	return internal::up_cast_helper<D>(std::integral_constant < bool,
	                                   std::is_base_of<DV, TV>::value || std::is_same<DV, TV>::value > (), object);
}

template <typename Container, typename T> inline bool contains(const Container &container, const T &value) {
	auto end = std::end(container);
	return std::find(std::begin(container), end, value) != end;
}

// We need a custom comparator for std::set<std::unique_ptr<T>>::find to work with pointers.
// thanks to http://stackoverflow.com/questions/18939882/raw-pointer-lookup-for-sets-of-unique-ptrs
template <typename T> struct pointer_comparator {
	using is_transparent = std::true_type;

	// helper does some magic in order to reduce the number of
	// pairs of types we need to know how to compare: it turns
	// everything into a pointer, and then uses `std::less<T*>`
	// to do the comparison:
	struct helper {
		T *ptr = nullptr;
		helper() = default;
		helper(const helper &other) = default;
		helper(T *p)
		    : ptr(p) {}
		template <typename... Ts>
		helper(const std::shared_ptr<Ts...> &other)
		    : ptr(other.get()) {}
		template <typename... Ts>
		helper(const std::unique_ptr<Ts...> &other)
		    : ptr(other.get()) {}
		bool operator<(helper other) const {
			return std::less<T *>()(ptr, other.ptr);
		}
	};

	// without helper, we'd need 2^n different overloads, where
	// n is the number of types we want to support (so, 8 with
	// raw pointers, unique pointers, and shared pointers).  That
	// seems silly.
	// && helps enforce rvalue use only
	bool operator()(const helper &&lhs, const helper &&rhs) const {
		return lhs < rhs;
	}
};

template <typename T> using set_of_unique_ptr = std::set<std::unique_ptr<T>, base::pointer_comparator<T>>;

template <typename T> using set_of_shared_ptr = std::set<std::shared_ptr<T>, base::pointer_comparator<T>>;

using byte_span = gsl::span<gsl::byte>;
using const_byte_span = gsl::span<const gsl::byte>;
using byte_vector = std::vector<gsl::byte>;
template <size_t N> using byte_array = std::array<gsl::byte, N>;

inline void copy_bytes(byte_span destination, const_byte_span source) {
	Expects(destination.size() >= source.size());
	memcpy(destination.data(), source.data(), source.size());
}

inline void move_bytes(byte_span destination, const_byte_span source) {
	Expects(destination.size() >= source.size());
	memmove(destination.data(), source.data(), source.size());
}

inline void set_bytes(byte_span destination, gsl::byte value) {
	memset(destination.data(), gsl::to_integer<unsigned char>(value), destination.size());
}

inline int compare_bytes(const_byte_span a, const_byte_span b) {
	auto aSize = a.size(), bSize = b.size();
	return (aSize > bSize) ? 1 : (aSize < bSize) ? -1 : memcmp(a.data(), b.data(), aSize);
}

// Thanks https://stackoverflow.com/a/28139075

template <typename Container> struct reversion_wrapper { Container &container; };

template <typename Container> auto begin(reversion_wrapper<Container> wrapper) {
	return std::rbegin(wrapper.container);
}

template <typename Container> auto end(reversion_wrapper<Container> wrapper) {
	return std::rend(wrapper.container);
}

template <typename Container> reversion_wrapper<Container> reversed(Container &&container) {
	return {container};
}

} // namespace base

// using for_const instead of plain range-based for loop to ensure usage of const_iterator
// it is important for the copy-on-write Qt containers
// if you have "QVector<T*> v" then "for (T * const p : v)" will still call QVector::detach(),
// while "for_const (T *p, v)" won't and "for_const (T *&p, v)" won't compile
#define for_const(range_declaration, range_expression) for (range_declaration : std::as_const(range_expression))

template <typename Lambda> inline void InvokeQueued(QObject *context, Lambda &&lambda) {
	QObject proxy;
	QObject::connect(&proxy, &QObject::destroyed, context, std::forward<Lambda>(lambda), Qt::QueuedConnection);
}

static const qint32 ScrollMax = INT_MAX;

extern quint64 _SharedMemoryLocation[];
template <typename T, unsigned int N> T *SharedMemoryLocation() {
	static_assert(N < 4, "Only 4 shared memory locations!");
	return reinterpret_cast<T *>(_SharedMemoryLocation + N);
}

// see https://github.com/boostcon/cppnow_presentations_2012/blob/master/wed/schurr_cpp11_tools_for_class_authors.pdf
class str_const { // constexpr string
public:
	template <std::size_t N>
	constexpr str_const(const char (&a)[N])
	    : _str(a)
	    , _size(N - 1) {}
	constexpr char operator[](std::size_t n) const {
		return (n < _size) ? _str[n] :
#ifndef OS_MAC_OLD
		                     throw std::out_of_range("");
#else // OS_MAC_OLD
		                     throw std::exception();
#endif // OS_MAC_OLD
	}
	constexpr std::size_t size() const {
		return _size;
	}
	const char *c_str() const {
		return _str;
	}

private:
	const char *const _str;
	const std::size_t _size;
};

inline QString str_const_toString(const str_const &str) {
	return QString::fromUtf8(str.c_str(), static_cast<int>(str.size()));
}

inline QByteArray str_const_toByteArray(const str_const &str) {
	return QByteArray::fromRawData(str.c_str(), static_cast<int>(str.size()));
}

template <typename T> inline void accumulate_max(T &a, const T &b) {
	if (a < b) a = b;
}

template <typename T> inline void accumulate_min(T &a, const T &b) {
	if (a > b) a = b;
}

class Exception : public std::exception {
public:
	Exception(const QString &msg, bool isFatal = true)
	    : _fatal(isFatal)
	    , _msg(msg.toUtf8()) {
		LOG(("Exception: %1").arg(msg));
	}
	bool fatal() const {
		return _fatal;
	}

	virtual const char *what() const throw() {
		return _msg.constData();
	}
	virtual ~Exception() throw() {}

private:
	bool _fatal;
	QByteArray _msg;
};

class MTPint;
using TimeId = qint32;
TimeId myunixtime();
void unixtimeInit();
void unixtimeSet(TimeId servertime, bool force = false);
TimeId unixtime();
TimeId fromServerTime(const MTPint &serverTime);
void toServerTime(const TimeId &clientTime, MTPint &outServerTime);
quint64 msgid();
qint32 reqid();

inline QDateTime date(qint32 time = -1) {
	QDateTime result;
	if (time >= 0) result.setTime_t(time);
	return result;
}

inline QDateTime dateFromServerTime(const MTPint &time) {
	return date(fromServerTime(time));
}

inline QDateTime date(const MTPint &time) {
	return dateFromServerTime(time);
}

QDateTime dateFromServerTime(TimeId time);

inline void mylocaltime(struct tm *_Tm, const time_t *_Time) {
#ifdef Q_OS_WIN
	localtime_s(_Tm, _Time);
#else
	localtime_r(_Time, _Tm);
#endif
}

namespace ThirdParty {

void start();
void finish();

} // namespace ThirdParty

using TimeMs = qint64; // @todo use std::chrono::milliseconds
bool checkms(); // returns true if time has changed
TimeMs getms(bool checked = false);

const static quint32 _md5_block_size = 64;
class HashMd5 {
public:
	HashMd5(const void *input = 0, quint32 length = 0);
	void feed(const void *input, quint32 length);
	qint32 *result();

private:
	void init();
	void finalize();
	void transform(const uchar *block);

	bool _finalized;
	uchar _buffer[_md5_block_size];
	quint32 _count[2];
	quint32 _state[4];
	uchar _digest[16];
};

qint32 hashCrc32(const void *data, quint32 len);

qint32 *hashSha1(const void *data, quint32 len, void *dest); // dest - ptr to 20 bytes, returns (qint32*)dest
inline std::array<char, 20> hashSha1(const void *data, int size) {
	auto result = std::array<char, 20>();
	hashSha1(data, size, result.data());
	return result;
}

qint32 *hashSha256(const void *data, quint32 len, void *dest); // dest - ptr to 32 bytes, returns (qint32*)dest
inline std::array<char, 32> hashSha256(const void *data, int size) {
	auto result = std::array<char, 32>();
	hashSha256(data, size, result.data());
	return result;
}

qint32 *hashMd5(const void *data, quint32 len, void *dest); // dest = ptr to 16 bytes, returns (qint32*)dest
inline std::array<char, 16> hashMd5(const void *data, int size) {
	auto result = std::array<char, 16>();
	hashMd5(data, size, result.data());
	return result;
}

char *hashMd5Hex(const qint32 *hashmd5, void *dest); // dest = ptr to 32 bytes, returns (char*)dest
inline char *hashMd5Hex(const void *data, quint32 len, void *dest) { // dest = ptr to 32 bytes, returns (char*)dest
	return hashMd5Hex(HashMd5(data, len).result(), dest);
}
inline std::array<char, 32> hashMd5Hex(const void *data, int size) {
	auto result = std::array<char, 32>();
	hashMd5Hex(data, size, result.data());
	return result;
}

// good random (using openssl implementation)
void memset_rand(void *data, quint32 len);
template <typename T> T rand_value() {
	T result;
	memset_rand(&result, sizeof(result));
	return result;
}

inline void memset_rand_bad(void *data, quint32 len) {
	for (uchar *i = reinterpret_cast<uchar *>(data), *e = i + len; i != e; ++i) {
		*i = uchar(rand() & 0xFF);
	}
}

template <typename T> inline void memsetrnd_bad(T &value) {
	memset_rand_bad(&value, sizeof(value));
}

class ReadLockerAttempt {
public:
	ReadLockerAttempt(not_null<QReadWriteLock *> lock)
	    : _lock(lock)
	    , _locked(_lock->tryLockForRead()) {}
	ReadLockerAttempt(const ReadLockerAttempt &other) = delete;
	ReadLockerAttempt &operator=(const ReadLockerAttempt &other) = delete;
	ReadLockerAttempt(ReadLockerAttempt &&other)
	    : _lock(other._lock)
	    , _locked(base::take(other._locked)) {}
	ReadLockerAttempt &operator=(ReadLockerAttempt &&other) {
		_lock = other._lock;
		_locked = base::take(other._locked);
		return *this;
	}
	~ReadLockerAttempt() {
		if (_locked) {
			_lock->unlock();
		}
	}

	operator bool() const {
		return _locked;
	}

private:
	not_null<QReadWriteLock *> _lock;
	bool _locked = false;
};

inline QString fromUtf8Safe(const char *str, qint32 size = -1) {
	if (!str || !size) return QString();
	if (size < 0) size = qint32(strlen(str));
	QString result(QString::fromUtf8(str, size));
	QByteArray back = result.toUtf8();
	if (back.size() != size || memcmp(back.constData(), str, size)) return QString::fromLocal8Bit(str, size);
	return result;
}

inline QString fromUtf8Safe(const QByteArray &str) {
	return fromUtf8Safe(str.constData(), str.size());
}

static const QRegularExpression::PatternOptions reMultiline(QRegularExpression::DotMatchesEverythingOption |
                                                            QRegularExpression::MultilineOption);

template <typename T> inline T snap(const T &v, const T &_min, const T &_max) { // @todo std::clamp()
	return (v < _min) ? _min : ((v > _max) ? _max : v);
}

template <typename T> class ManagedPtr {
public:
	ManagedPtr() = default;
	ManagedPtr(T *p)
	    : _data(p) {}
	T *operator->() const {
		return _data;
	}
	T *v() const {
		return _data;
	}

	explicit operator bool() const {
		return _data != nullptr;
	}

protected:
	using Parent = ManagedPtr<T>;
	T *_data = nullptr;
};

QString translitRusEng(const QString &rus);
QString rusKeyboardLayoutSwitch(const QString &from);

enum DBISendKey {
	dbiskEnter = 0,
	dbiskCtrlEnter = 1,
};

enum DBINotifyView {
	dbinvShowPreview = 0,
	dbinvShowName = 1,
	dbinvShowNothing = 2,
};

enum DBIWorkMode {
	dbiwmWindowAndTray = 0,
	dbiwmTrayOnly = 1,
	dbiwmWindowOnly = 2,
};

enum DBIConnectionType {
	dbictAuto = 0,
	dbictHttpAuto = 1, // not used
	dbictHttpProxy = 2,
	dbictTcpProxy = 3,
};

struct ProxyData {
	QString host;
	quint32 port = 0;
	QString user, password;
};

enum DBIScale {
	dbisAuto = 0,
	dbisOne = 1,
	dbisOneAndQuarter = 2,
	dbisOneAndHalf = 3,
	dbisTwo = 4,

	dbisScaleCount = 5,
};

static const int MatrixRowShift = 40000;

enum DBIPlatform {
	dbipWindows = 0,
	dbipMac = 1,
	dbipLinux64 = 2,
	dbipLinux32 = 3,
	dbipMacOld = 4,
};

enum DBIPeerReportSpamStatus {
	dbiprsNoButton = 0, // hidden, but not in the cloud settings yet
	dbiprsUnknown = 1, // contacts not loaded yet
	dbiprsShowButton = 2, // show report spam button, each show peer request setting from cloud
	dbiprsReportSent = 3, // report sent, but the report spam panel is not hidden yet
	dbiprsHidden = 4, // hidden in the cloud or not needed (bots, contacts, etc), no more requests
	dbiprsRequesting = 5, // requesting the cloud setting right now
};

template <int Size> inline QString strMakeFromLetters(const quint32 (&letters)[Size]) {
	QString result;
	result.reserve(Size);
	for (qint32 i = 0; i < Size; ++i) {
		result.push_back(QChar((((letters[i] >> 16) & 0xFF) << 8) | (letters[i] & 0xFF)));
	}
	return result;
}

class MimeType {
public:
	enum class Known {
		Unknown,
		TDesktopTheme,
		TDesktopPalette,
		WebP,
	};

	MimeType(const QMimeType &type)
	    : _typeStruct(type) {}
	MimeType(Known type)
	    : _type(type) {}
	QStringList globPatterns() const;
	QString filterString() const;
	QString name() const;

private:
	QMimeType _typeStruct;
	Known _type = Known::Unknown;
};

MimeType mimeTypeForName(const QString &mime);
MimeType mimeTypeForFile(const QFileInfo &file);
MimeType mimeTypeForData(const QByteArray &data);

#include <cmath>

inline int rowscount(int fullCount, int countPerRow) {
	return (fullCount + countPerRow - 1) / countPerRow;
}
inline int floorclamp(int value, int step, int lowest, int highest) {
	return qMin(qMax(value / step, lowest), highest);
}
inline int floorclamp(double value, int step, int lowest, int highest) {
	return qMin(qMax(static_cast<int>(std::floor(value / step)), lowest), highest);
}
inline int ceilclamp(int value, int step, int lowest, int highest) {
	return qMax(qMin((value + step - 1) / step, highest), lowest);
}
inline int ceilclamp(double value, qint32 step, qint32 lowest, qint32 highest) {
	return qMax(qMin(static_cast<int>(std::ceil(value / step)), highest), lowest);
}

enum ForwardWhatMessages {
	ForwardSelectedMessages,
	ForwardContextMessage,
	ForwardPressedMessage,
	ForwardPressedLinkMessage
};

enum ShowLayerOption {
	CloseOtherLayers = (1 << 0),
	KeepOtherLayers = (1 << 1),
	ShowAfterOtherLayers = (1 << 2),

	AnimatedShowLayer = (1 << 3),
	ForceFastShowLayer = (1 << 4),
};
using ShowLayerOptions = base::flags<ShowLayerOption>;
inline constexpr auto is_flag_type(ShowLayerOption) {
	return true;
};

static qint32 FullArcLength = 360 * 16;
static qint32 QuarterArcLength = (FullArcLength / 4);
static qint32 MinArcLength = (FullArcLength / 360);
static qint32 AlmostFullArcLength = (FullArcLength - MinArcLength);

template <typename T, typename... Args> inline QSharedPointer<T> MakeShared(Args &&... args) {
	return QSharedPointer<T>(new T(std::forward<Args>(args)...));
}

// This pointer is used for global non-POD variables that are allocated
// on demand by createIfNull(lambda) and are never automatically freed.
template <typename T> class NeverFreedPointer {
public:
	NeverFreedPointer() = default;
	NeverFreedPointer(const NeverFreedPointer<T> &other) = delete;
	NeverFreedPointer &operator=(const NeverFreedPointer<T> &other) = delete;

	template <typename... Args> void createIfNull(Args &&... args) {
		if (isNull()) {
			reset(new T(std::forward<Args>(args)...));
		}
	};

	T *data() const {
		return _p;
	}
	T *release() {
		return base::take(_p);
	}
	void reset(T *p = nullptr) {
		delete _p;
		_p = p;
	}
	bool isNull() const {
		return data() == nullptr;
	}

	void clear() {
		reset();
	}
	T *operator->() const {
		return data();
	}
	T &operator*() const {
		Assert(!isNull());
		return *data();
	}
	explicit operator bool() const {
		return !isNull();
	}

private:
	T *_p;
};

// This pointer is used for static non-POD variables that are allocated
// on first use by constructor and are never automatically freed.
template <typename T> class StaticNeverFreedPointer {
public:
	explicit StaticNeverFreedPointer(T *p)
	    : _p(p) {}
	StaticNeverFreedPointer(const StaticNeverFreedPointer<T> &other) = delete;
	StaticNeverFreedPointer &operator=(const StaticNeverFreedPointer<T> &other) = delete;

	T *data() const {
		return _p;
	}
	T *release() {
		return base::take(_p);
	}
	void reset(T *p = nullptr) {
		delete _p;
		_p = p;
	}
	bool isNull() const {
		return data() == nullptr;
	}

	void clear() {
		reset();
	}
	T *operator->() const {
		return data();
	}
	T &operator*() const {
		Assert(!isNull());
		return *data();
	}
	explicit operator bool() const {
		return !isNull();
	}

private:
	T *_p = nullptr;
};
