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
#include "mtproto/connection.h"

#include "mtproto/rsa_public_key.h"
#include "mtproto/rpc_sender.h"
#include "mtproto/dc_options.h"
#include "mtproto/connection_abstract.h"
#include "zlib.h"
#include "lang/lang_keys.h"
#include "base/openssl_help.h"
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/rand.h>

namespace MTP {
namespace internal {
namespace {

constexpr auto kRecreateKeyId = AuthKey::KeyId(0xFFFFFFFFFFFFFFFFULL);
constexpr auto kIntSize = static_cast<int>(sizeof(mtpPrime));
constexpr auto kMaxModExpSize = 256;

// Don't try to handle messages larger than this size.
constexpr auto kMaxMessageLength = 16 * 1024 * 1024;

bool IsGoodModExpFirst(const openssl::BigNum &modexp, const openssl::BigNum &prime) {
	auto diff = prime - modexp;
	if (modexp.failed() || prime.failed() || diff.failed()) {
		return false;
	}
	constexpr auto kMinDiffBitsCount = 2048 - 64;
	if (diff.isNegative() || diff.bitsSize() < kMinDiffBitsCount || modexp.bitsSize() < kMinDiffBitsCount) {
		return false;
	}
	Assert(modexp.bytesSize() <= kMaxModExpSize);
	return true;
}

bool IsPrimeAndGoodCheck(const openssl::BigNum &prime, int g) {
	constexpr auto kGoodPrimeBitsCount = 2048;

	if (prime.failed() || prime.isNegative() || prime.bitsSize() != kGoodPrimeBitsCount) {
		LOG(("MTP Error: Bad prime bits count %1, expected %2.").arg(prime.bitsSize()).arg(kGoodPrimeBitsCount));
		return false;
	}

	openssl::Context context;
	if (!prime.isPrime(context)) {
		LOG(("MTP Error: Bad prime."));
		return false;
	}

	switch (g) {
	case 2: {
		auto mod8 = prime.modWord(8);
		if (mod8 != 7) {
			LOG(("BigNum PT Error: bad g value: %1, mod8: %2").arg(g).arg(mod8));
			return false;
		}
	} break;
	case 3: {
		auto mod3 = prime.modWord(3);
		if (mod3 != 2) {
			LOG(("BigNum PT Error: bad g value: %1, mod3: %2").arg(g).arg(mod3));
			return false;
		}
	} break;
	case 4: break;
	case 5: {
		auto mod5 = prime.modWord(5);
		if (mod5 != 1 && mod5 != 4) {
			LOG(("BigNum PT Error: bad g value: %1, mod5: %2").arg(g).arg(mod5));
			return false;
		}
	} break;
	case 6: {
		auto mod24 = prime.modWord(24);
		if (mod24 != 19 && mod24 != 23) {
			LOG(("BigNum PT Error: bad g value: %1, mod24: %2").arg(g).arg(mod24));
			return false;
		}
	} break;
	case 7: {
		auto mod7 = prime.modWord(7);
		if (mod7 != 3 && mod7 != 5 && mod7 != 6) {
			LOG(("BigNum PT Error: bad g value: %1, mod7: %2").arg(g).arg(mod7));
			return false;
		}
	} break;
	default: {
		LOG(("BigNum PT Error: bad g value: %1").arg(g));
		return false;
	} break;
	}

	auto primeSubOneDivTwo = prime;
	primeSubOneDivTwo.setSubWord(1);
	primeSubOneDivTwo.setDivWord(2);
	if (!primeSubOneDivTwo.isPrime(context)) {
		LOG(("MTP Error: Bad (prime - 1) / 2."));
		return false;
	}

	return true;
}

bool IsPrimeAndGood(base::const_byte_span primeBytes, int g) {
	static constexpr unsigned char GoodPrime[] = {
		0xC7, 0x1C, 0xAE, 0xB9, 0xC6, 0xB1, 0xC9, 0x04, 0x8E, 0x6C, 0x52, 0x2F, 0x70, 0xF1, 0x3F, 0x73,
		0x98, 0x0D, 0x40, 0x23, 0x8E, 0x3E, 0x21, 0xC1, 0x49, 0x34, 0xD0, 0x37, 0x56, 0x3D, 0x93, 0x0F,
		0x48, 0x19, 0x8A, 0x0A, 0xA7, 0xC1, 0x40, 0x58, 0x22, 0x94, 0x93, 0xD2, 0x25, 0x30, 0xF4, 0xDB,
		0xFA, 0x33, 0x6F, 0x6E, 0x0A, 0xC9, 0x25, 0x13, 0x95, 0x43, 0xAE, 0xD4, 0x4C, 0xCE, 0x7C, 0x37,
		0x20, 0xFD, 0x51, 0xF6, 0x94, 0x58, 0x70, 0x5A, 0xC6, 0x8C, 0xD4, 0xFE, 0x6B, 0x6B, 0x13, 0xAB,
		0xDC, 0x97, 0x46, 0x51, 0x29, 0x69, 0x32, 0x84, 0x54, 0xF1, 0x8F, 0xAF, 0x8C, 0x59, 0x5F, 0x64,
		0x24, 0x77, 0xFE, 0x96, 0xBB, 0x2A, 0x94, 0x1D, 0x5B, 0xCD, 0x1D, 0x4A, 0xC8, 0xCC, 0x49, 0x88,
		0x07, 0x08, 0xFA, 0x9B, 0x37, 0x8E, 0x3C, 0x4F, 0x3A, 0x90, 0x60, 0xBE, 0xE6, 0x7C, 0xF9, 0xA4,
		0xA4, 0xA6, 0x95, 0x81, 0x10, 0x51, 0x90, 0x7E, 0x16, 0x27, 0x53, 0xB5, 0x6B, 0x0F, 0x6B, 0x41,
		0x0D, 0xBA, 0x74, 0xD8, 0xA8, 0x4B, 0x2A, 0x14, 0xB3, 0x14, 0x4E, 0x0E, 0xF1, 0x28, 0x47, 0x54,
		0xFD, 0x17, 0xED, 0x95, 0x0D, 0x59, 0x65, 0xB4, 0xB9, 0xDD, 0x46, 0x58, 0x2D, 0xB1, 0x17, 0x8D,
		0x16, 0x9C, 0x6B, 0xC4, 0x65, 0xB0, 0xD6, 0xFF, 0x9C, 0xA3, 0x92, 0x8F, 0xEF, 0x5B, 0x9A, 0xE4,
		0xE4, 0x18, 0xFC, 0x15, 0xE8, 0x3E, 0xBE, 0xA0, 0xF8, 0x7F, 0xA9, 0xFF, 0x5E, 0xED, 0x70, 0x05,
		0x0D, 0xED, 0x28, 0x49, 0xF4, 0x7B, 0xF9, 0x59, 0xD9, 0x56, 0x85, 0x0C, 0xE9, 0x29, 0x85, 0x1F,
		0x0D, 0x81, 0x15, 0xF6, 0x35, 0xB1, 0x05, 0xEE, 0x2E, 0x4E, 0x15, 0xD0, 0x4B, 0x24, 0x54, 0xBF,
		0x6F, 0x4F, 0xAD, 0xF0, 0x34, 0xB1, 0x04, 0x03, 0x11, 0x9C, 0xD8, 0xE3, 0xB9, 0x2F, 0xCC, 0x5B };

	if (!base::compare_bytes(gsl::as_bytes(gsl::make_span(GoodPrime)), primeBytes)) {
		if (g == 3 || g == 4 || g == 5 || g == 7) {
			return true;
		}
	}

	return IsPrimeAndGoodCheck(openssl::BigNum(primeBytes), g);
}

std::vector<gsl::byte> CreateAuthKey(base::const_byte_span firstBytes, base::const_byte_span randomBytes, base::const_byte_span primeBytes) {
	using openssl::BigNum;
	BigNum first(firstBytes);
	BigNum prime(primeBytes);
	if (!IsGoodModExpFirst(first, prime)) {
		LOG(("AuthKey Error: Bad first prime in CreateAuthKey()."));
		return std::vector<gsl::byte>();
	}
	return BigNum::ModExp(first, BigNum(randomBytes), prime).getBytes();
}

ModExpFirst CreateModExp(int g, base::const_byte_span primeBytes, base::const_byte_span randomSeed) {
	Expects(randomSeed.size() == ModExpFirst::kRandomPowerSize);

	using namespace openssl;

	BigNum prime(primeBytes);
	ModExpFirst result;
	constexpr auto kMaxModExpFirstTries = 5;
	for (auto tries = 0; tries != kMaxModExpFirstTries; ++tries) {
		FillRandom(result.randomPower);
		for (auto i = 0; i != ModExpFirst::kRandomPowerSize; ++i) {
			result.randomPower[i] ^= randomSeed[i];
		}
		auto modexp = BigNum::ModExp(BigNum(g), BigNum(result.randomPower), prime);
		if (IsGoodModExpFirst(modexp, prime)) {
			result.modexp = modexp.getBytes();
			break;
		}
	}
	return result;
}

void wrapInvokeAfter(mtpRequest &to, const mtpRequest &from, const mtpRequestMap &haveSent, int32_t skipBeforeRequest = 0) {
	mtpMsgId afterId(*(mtpMsgId*)(from->after->data() + 4));
	mtpRequestMap::const_iterator i = afterId ? haveSent.constFind(afterId) : haveSent.cend();
	int32_t size = to->size(), lenInInts = (from.innerLength() >> 2), headlen = 4, fulllen = headlen + lenInInts;
	if (i == haveSent.constEnd()) { // no invoke after or such msg was not sent or was completed recently
		to->resize(size + fulllen + skipBeforeRequest);
		if (skipBeforeRequest) {
			memcpy(to->data() + size, from->constData() + 4, headlen * sizeof(mtpPrime));
			memcpy(to->data() + size + headlen + skipBeforeRequest, from->constData() + 4 + headlen, lenInInts * sizeof(mtpPrime));
		} else {
			memcpy(to->data() + size, from->constData() + 4, fulllen * sizeof(mtpPrime));
		}
	} else {
		to->resize(size + fulllen + skipBeforeRequest + 3);
		memcpy(to->data() + size, from->constData() + 4, headlen * sizeof(mtpPrime));
		(*to)[size + 3] += 3 * sizeof(mtpPrime);
		*((mtpTypeId*)&((*to)[size + headlen + skipBeforeRequest])) = mtpc_invokeAfterMsg;
		memcpy(to->data() + size + headlen + skipBeforeRequest + 1, &afterId, 2 * sizeof(mtpPrime));
		memcpy(to->data() + size + headlen + skipBeforeRequest + 3, from->constData() + 4 + headlen, lenInInts * sizeof(mtpPrime));
		if (size + 3 != 7) (*to)[7] += 3 * sizeof(mtpPrime);
	}
}

bool parsePQ(const QByteArray &pqStr, QByteArray &pStr, QByteArray &qStr) {
	if (pqStr.length() > 8) return false; // more than 64 bit pq

	uint64_t pq = 0, p, q;
	const uchar *pqChars = (const uchar*)pqStr.constData();
	for (uint32_t i = 0, l = pqStr.length(); i < l; ++i) {
		pq <<= 8;
		pq |= (uint64_t)pqChars[i];
	}
	uint64_t pqSqrt = (uint64_t)sqrtl((long double)pq), ySqr, y;
	while (pqSqrt * pqSqrt > pq) --pqSqrt;
	while (pqSqrt * pqSqrt < pq) ++pqSqrt;
	for (ySqr = pqSqrt * pqSqrt - pq; ; ++pqSqrt, ySqr = pqSqrt * pqSqrt - pq) {
		y = (uint64_t)sqrtl((long double)ySqr);
		while (y * y > ySqr) --y;
		while (y * y < ySqr) ++y;
		if (!ySqr || y + pqSqrt >= pq) return false;
		if (y * y == ySqr) {
			p = pqSqrt + y;
			q = (pqSqrt > y) ? (pqSqrt - y) : (y - pqSqrt);
			break;
		}
	}
	if (p > q) std::swap(p, q);

	pStr.resize(4);
	uchar *pChars = (uchar*)pStr.data();
	for (uint32_t i = 0; i < 4; ++i) {
		*(pChars + 3 - i) = (uchar)(p & 0xFF);
		p >>= 8;
	}

	qStr.resize(4);
	uchar *qChars = (uchar*)qStr.data();
	for (uint32_t i = 0; i < 4; ++i) {
		*(qChars + 3 - i) = (uchar)(q & 0xFF);
		q >>= 8;
	}

	return true;
}

} // namespace

Connection::Connection(Instance *instance) : _instance(instance) {
}

void Connection::start(SessionData *sessionData, ShiftedDcId shiftedDcId) {
	Expects(thread == nullptr && data == nullptr);

	thread = std::make_unique<Thread>();
	auto newData = std::make_unique<ConnectionPrivate>(_instance, thread.get(), this, sessionData, shiftedDcId);

	// will be deleted in the thread::finished signal
	data = newData.release();
	thread->start();
}

void Connection::kill() {
	Expects(data != nullptr && thread != nullptr);
	data->stop();
	data = nullptr;
	thread->quit();
}

void Connection::waitTillFinish() {
	Expects(data == nullptr && thread != nullptr);

	DEBUG_LOG(("Waiting for connectionThread to finish"));
	thread->wait();
	thread.reset();
}

int32_t Connection::state() const {
	Expects(data != nullptr && thread != nullptr);

	return data->getState();
}

QString Connection::transport() const {
	Expects(data != nullptr && thread != nullptr);

	return data->transport();
}

Connection::~Connection() {
	Expects(data == nullptr);
	if (thread) {
		waitTillFinish();
	}
}

void ConnectionPrivate::createConn(bool createIPv4, bool createIPv6) {
	destroyConn();
	if (createIPv4) {
		QWriteLocker lock(&stateConnMutex);
		_conn4 = AbstractConnection::create(_dcType, thread());
		connect(_conn4, SIGNAL(error(int32_t)), this, SLOT(onError4(int32_t)));
		connect(_conn4, SIGNAL(receivedSome()), this, SLOT(onReceivedSome()));
	}
	if (createIPv6) {
		QWriteLocker lock(&stateConnMutex);
		_conn6 = AbstractConnection::create(_dcType, thread());
		connect(_conn6, SIGNAL(error(int32_t)), this, SLOT(onError6(int32_t)));
		connect(_conn6, SIGNAL(receivedSome()), this, SLOT(onReceivedSome()));
	}
	firstSentAt = 0;
	if (oldConnection) {
		oldConnection = false;
		DEBUG_LOG(("This connection marked as not old!"));
	}
	oldConnectionTimer.start(MTPConnectionOldTimeout);
}

void ConnectionPrivate::destroyConn(AbstractConnection **conn) {
	if (conn) {
		AbstractConnection *toDisconnect = nullptr;

		{
			QWriteLocker lock(&stateConnMutex);
			if (*conn) {
				toDisconnect = *conn;
				disconnect(*conn, SIGNAL(connected()), nullptr, nullptr);
				disconnect(*conn, SIGNAL(disconnected()), nullptr, nullptr);
				disconnect(*conn, SIGNAL(error(int32_t)), nullptr, nullptr);
				disconnect(*conn, SIGNAL(receivedData()), nullptr, nullptr);
				disconnect(*conn, SIGNAL(receivedSome()), nullptr, nullptr);
				*conn = nullptr;
			}
		}
		if (toDisconnect) {
			toDisconnect->disconnectFromServer();
			toDisconnect->deleteLater();
		}
	} else {
		destroyConn(&_conn4);
		destroyConn(&_conn6);
		_conn = nullptr;
	}
}

ConnectionPrivate::ConnectionPrivate(Instance *instance, QThread *thread, Connection *owner, SessionData *data, ShiftedDcId shiftedDcId) : QObject()
, _instance(instance)
, _state(DisconnectedState)
, _shiftedDcId(shiftedDcId)
, _owner(owner)
, _waitForReceived(MTPMinReceiveDelay)
, _waitForConnected(MTPMinConnectDelay)
//, sessionDataMutex(QReadWriteLock::Recursive)
, sessionData(data) {
	oldConnectionTimer.moveToThread(thread);
	_waitForConnectedTimer.moveToThread(thread);
	_waitForReceivedTimer.moveToThread(thread);
	_waitForIPv4Timer.moveToThread(thread);
	_pingSender.moveToThread(thread);
	retryTimer.moveToThread(thread);
	moveToThread(thread);

	Expects(_shiftedDcId != 0);

	connect(thread, &QThread::started, this, [this] { connectToServer(); });
	connect(thread, &QThread::finished, this, [this] { finishAndDestroy(); });
	connect(this, SIGNAL(finished(internal::Connection*)), _instance, SLOT(connectionFinished(internal::Connection*)), Qt::QueuedConnection);

	connect(&retryTimer, SIGNAL(timeout()), this, SLOT(retryByTimer()));
	connect(&_waitForConnectedTimer, SIGNAL(timeout()), this, SLOT(onWaitConnectedFailed()));
	connect(&_waitForReceivedTimer, SIGNAL(timeout()), this, SLOT(onWaitReceivedFailed()));
	connect(&_waitForIPv4Timer, SIGNAL(timeout()), this, SLOT(onWaitIPv4Failed()));
	connect(&oldConnectionTimer, SIGNAL(timeout()), this, SLOT(onOldConnection()));
	connect(&_pingSender, SIGNAL(timeout()), this, SLOT(onPingSender()));
	connect(sessionData->owner(), SIGNAL(authKeyCreated()), this, SLOT(updateAuthKey()), Qt::QueuedConnection);

	connect(sessionData->owner(), SIGNAL(needToRestart()), this, SLOT(restartNow()), Qt::QueuedConnection);
	connect(this, SIGNAL(needToReceive()), sessionData->owner(), SLOT(tryToReceive()), Qt::QueuedConnection);
	connect(this, SIGNAL(stateChanged(int32_t)), sessionData->owner(), SLOT(onConnectionStateChange(int32_t)), Qt::QueuedConnection);
	connect(sessionData->owner(), SIGNAL(needToSend()), this, SLOT(tryToSend()), Qt::QueuedConnection);
	connect(sessionData->owner(), SIGNAL(needToPing()), this, SLOT(onPingSendForce()), Qt::QueuedConnection);
	connect(this, SIGNAL(sessionResetDone()), sessionData->owner(), SLOT(onResetDone()), Qt::QueuedConnection);

	static bool _registered = false;
	if (!_registered) {
		_registered = true;
		qRegisterMetaType<QVector<uint64_t> >("QVector<uint64_t>");
	}

	connect(this, SIGNAL(needToSendAsync()), sessionData->owner(), SLOT(needToResumeAndSend()), Qt::QueuedConnection);
	connect(this, SIGNAL(sendAnythingAsync(int64_t)), sessionData->owner(), SLOT(sendAnything(int64_t)), Qt::QueuedConnection);
	connect(this, SIGNAL(sendHttpWaitAsync()), sessionData->owner(), SLOT(sendAnything()), Qt::QueuedConnection);
	connect(this, SIGNAL(sendPongAsync(uint64_t,uint64_t)), sessionData->owner(), SLOT(sendPong(uint64_t,uint64_t)), Qt::QueuedConnection);
	connect(this, SIGNAL(sendMsgsStateInfoAsync(uint64_t, QByteArray)), sessionData->owner(), SLOT(sendMsgsStateInfo(uint64_t,QByteArray)), Qt::QueuedConnection);
	connect(this, SIGNAL(resendAsync(uint64_t,int64_t,bool,bool)), sessionData->owner(), SLOT(resend(uint64_t,int64_t,bool,bool)), Qt::QueuedConnection);
	connect(this, SIGNAL(resendManyAsync(QVector<uint64_t>,int64_t,bool,bool)), sessionData->owner(), SLOT(resendMany(QVector<uint64_t>,int64_t,bool,bool)), Qt::QueuedConnection);
	connect(this, SIGNAL(resendAllAsync()), sessionData->owner(), SLOT(resendAll()));
}

void ConnectionPrivate::onConfigLoaded() {
	connectToServer(true);
}

void ConnectionPrivate::onCDNConfigLoaded() {
	restart();
}

int32_t ConnectionPrivate::getShiftedDcId() const {
	return _shiftedDcId;
}

int32_t ConnectionPrivate::getState() const {
	QReadLocker lock(&stateConnMutex);
	int32_t result = _state;
	if (_state < 0) {
		if (retryTimer.isActive()) {
			result = int32_t(getms(true) - retryWillFinish);
			if (result >= 0) {
				result = -1;
			}
		}
	}
	return result;
}

QString ConnectionPrivate::transport() const {
	QReadLocker lock(&stateConnMutex);
	if ((!_conn4 && !_conn6) || (_conn4 && _conn6) || (_state < 0)) {
		return QString();
	}
	QString result = (_conn4 ? _conn4 : _conn6)->transport();
	if (!result.isEmpty() && Global::TryIPv6()) result += (_conn4 ? "/IPv4" : "/IPv6");
	return result;
}

bool ConnectionPrivate::setState(int32_t state, int32_t ifState) {
	if (ifState != Connection::UpdateAlways) {
		QReadLocker lock(&stateConnMutex);
		if (_state != ifState) return false;
	}
	QWriteLocker lock(&stateConnMutex);
	if (_state == state) return false;
	_state = state;
	if (state < 0) {
		retryTimeout = -state;
		retryTimer.start(retryTimeout);
		retryWillFinish = getms(true) + retryTimeout;
	}
	emit stateChanged(state);
	return true;
}

void ConnectionPrivate::resetSession() { // recreate all msg_id and msg_seqno
	_needSessionReset = false;

	QWriteLocker locker1(sessionData->haveSentMutex());
	QWriteLocker locker2(sessionData->toResendMutex());
	QWriteLocker locker3(sessionData->toSendMutex());
	QWriteLocker locker4(sessionData->wereAckedMutex());
	mtpRequestMap &haveSent(sessionData->haveSentMap());
	mtpRequestIdsMap &toResend(sessionData->toResendMap());
	mtpPreRequestMap &toSend(sessionData->toSendMap());
	mtpRequestIdsMap &wereAcked(sessionData->wereAckedMap());

	mtpMsgId newId = msgid();
	mtpRequestMap setSeqNumbers;
	typedef QMap<mtpMsgId, mtpMsgId> Replaces;
	Replaces replaces;
	for (mtpRequestMap::const_iterator i = haveSent.cbegin(), e = haveSent.cend(); i != e; ++i) {
		if (!mtpRequestData::isSentContainer(i.value())) {
			if (!*(mtpMsgId*)(i.value()->constData() + 4)) continue;

			mtpMsgId id = i.key();
			if (id > newId) {
				while (true) {
					if (toResend.constFind(newId) == toResend.cend() && wereAcked.constFind(newId) == wereAcked.cend() && haveSent.constFind(newId) == haveSent.cend()) {
						break;
					}
					mtpMsgId m = msgid();
					if (m <= newId) break; // wtf

					newId = m;
				}

				MTP_LOG(_shiftedDcId, ("Replacing msgId %1 to %2!").arg(id).arg(newId));
				replaces.insert(id, newId);
				id = newId;
				*(mtpMsgId*)(i.value()->data() + 4) = id;
			}
			setSeqNumbers.insert(id, i.value());
		}
	}
	for (mtpRequestIdsMap::const_iterator i = toResend.cbegin(), e = toResend.cend(); i != e; ++i) { // collect all non-container requests
		mtpPreRequestMap::const_iterator j = toSend.constFind(i.value());
		if (j == toSend.cend()) continue;

		if (!mtpRequestData::isSentContainer(j.value())) {
			if (!*(mtpMsgId*)(j.value()->constData() + 4)) continue;

			mtpMsgId id = i.key();
			if (id > newId) {
				while (true) {
					if (toResend.constFind(newId) == toResend.cend() && wereAcked.constFind(newId) == wereAcked.cend() && haveSent.constFind(newId) == haveSent.cend()) {
						break;
					}
					mtpMsgId m = msgid();
					if (m <= newId) break; // wtf

					newId = m;
				}

				MTP_LOG(_shiftedDcId, ("Replacing msgId %1 to %2!").arg(id).arg(newId));
				replaces.insert(id, newId);
				id = newId;
				*(mtpMsgId*)(j.value()->data() + 4) = id;
			}
			setSeqNumbers.insert(id, j.value());
		}
	}

	uint64_t session = rand_value<uint64_t>();
	DEBUG_LOG(("MTP Info: creating new session after bad_msg_notification, setting random server_session %1").arg(session));
	sessionData->setSession(session);

	for (mtpRequestMap::const_iterator i = setSeqNumbers.cbegin(), e = setSeqNumbers.cend(); i != e; ++i) { // generate new seq_numbers
		bool wasNeedAck = (*(i.value()->data() + 6) & 1);
		*(i.value()->data() + 6) = sessionData->nextRequestSeqNumber(wasNeedAck);
	}
	if (!replaces.isEmpty()) {
		for (Replaces::const_iterator i = replaces.cbegin(), e = replaces.cend(); i != e; ++i) { // replace msgIds keys in all data structs
			mtpRequestMap::iterator j = haveSent.find(i.key());
			if (j != haveSent.cend()) {
				mtpRequest req = j.value();
				haveSent.erase(j);
				haveSent.insert(i.value(), req);
			}
			mtpRequestIdsMap::iterator k = toResend.find(i.key());
			if (k != toResend.cend()) {
				mtpRequestId req = k.value();
				toResend.erase(k);
				toResend.insert(i.value(), req);
			}
			k = wereAcked.find(i.key());
			if (k != wereAcked.cend()) {
				mtpRequestId req = k.value();
				wereAcked.erase(k);
				wereAcked.insert(i.value(), req);
			}
		}
		for (mtpRequestMap::const_iterator i = haveSent.cbegin(), e = haveSent.cend(); i != e; ++i) { // replace msgIds in saved containers
			if (mtpRequestData::isSentContainer(i.value())) {
				mtpMsgId *ids = (mtpMsgId *)(i.value()->data() + 8);
				for (uint32_t j = 0, l = (i.value()->size() - 8) >> 1; j < l; ++j) {
					Replaces::const_iterator k = replaces.constFind(ids[j]);
					if (k != replaces.cend()) {
						ids[j] = k.value();
					}
				}
			}
		}
	}

	ackRequestData.clear();
	resendRequestData.clear();
	{
		QWriteLocker locker5(sessionData->stateRequestMutex());
		sessionData->stateRequestMap().clear();
	}

	emit sessionResetDone();
}

mtpMsgId ConnectionPrivate::prepareToSend(mtpRequest &request, mtpMsgId currentLastId) {
	if (request->size() < 9) return 0;
	mtpMsgId msgId = *(mtpMsgId*)(request->constData() + 4);
	if (msgId) { // resending this request
		QWriteLocker locker(sessionData->toResendMutex());
		mtpRequestIdsMap &toResend(sessionData->toResendMap());
		mtpRequestIdsMap::iterator i = toResend.find(msgId);
		if (i != toResend.cend()) {
			toResend.erase(i);
		}
	} else {
		msgId = *(mtpMsgId*)(request->data() + 4) = currentLastId;
		*(request->data() + 6) = sessionData->nextRequestSeqNumber(mtpRequestData::needAck(request));
	}
	return msgId;
}

mtpMsgId ConnectionPrivate::replaceMsgId(mtpRequest &request, mtpMsgId newId) {
	if (request->size() < 9) return 0;

	mtpMsgId oldMsgId = *(mtpMsgId*)(request->constData() + 4);
	if (oldMsgId != newId) {
		if (oldMsgId) {
			QWriteLocker locker(sessionData->toResendMutex());
			// haveSentMutex() and wereAckedMutex() were locked in tryToSend()

			mtpRequestIdsMap &toResend(sessionData->toResendMap());
			mtpRequestIdsMap &wereAcked(sessionData->wereAckedMap());
			mtpRequestMap &haveSent(sessionData->haveSentMap());

			while (true) {
				if (toResend.constFind(newId) == toResend.cend() && wereAcked.constFind(newId) == wereAcked.cend() && haveSent.constFind(newId) == haveSent.cend()) {
					break;
				}
				mtpMsgId m = msgid();
				if (m <= newId) break; // wtf

				newId = m;
			}

			mtpRequestIdsMap::iterator i = toResend.find(oldMsgId);
			if (i != toResend.cend()) {
				mtpRequestId req = i.value();
				toResend.erase(i);
				toResend.insert(newId, req);
			}

			mtpRequestIdsMap::iterator j = wereAcked.find(oldMsgId);
			if (j != wereAcked.cend()) {
				mtpRequestId req = j.value();
				wereAcked.erase(j);
				wereAcked.insert(newId, req);
			}

			mtpRequestMap::iterator k = haveSent.find(oldMsgId);
			if (k != haveSent.cend()) {
				mtpRequest req = k.value();
				haveSent.erase(k);
				haveSent.insert(newId, req);
			}

			for (k = haveSent.begin(); k != haveSent.cend(); ++k) {
				mtpRequest req(k.value());
				if (mtpRequestData::isSentContainer(req)) {
					mtpMsgId *ids = (mtpMsgId *)(req->data() + 8);
					for (uint32_t i = 0, l = (req->size() - 8) >> 1; i < l; ++i) {
						if (ids[i] == oldMsgId) {
							ids[i] = newId;
						}
					}
				}
			}
		} else {
			*(request->data() + 6) = sessionData->nextRequestSeqNumber(mtpRequestData::needAck(request));
		}
		*(mtpMsgId*)(request->data() + 4) = newId;
	}
	return newId;
}

mtpMsgId ConnectionPrivate::placeToContainer(mtpRequest &toSendRequest, mtpMsgId &bigMsgId, mtpMsgId *&haveSentArr, mtpRequest &req) {
	mtpMsgId msgId = prepareToSend(req, bigMsgId);
	if (msgId > bigMsgId) msgId = replaceMsgId(req, bigMsgId);
	if (msgId >= bigMsgId) bigMsgId = msgid();
	*(haveSentArr++) = msgId;

	uint32_t from = toSendRequest->size(), len = mtpRequestData::messageSize(req);
	toSendRequest->resize(from + len);
	memcpy(toSendRequest->data() + from, req->constData() + 4, len * sizeof(mtpPrime));

	return msgId;
}

void ConnectionPrivate::tryToSend() {
	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData || !_conn) {
		return;
	}

	bool needsLayer = !sessionData->layerWasInited();
	int32_t state = getState();
	bool prependOnly = (state != ConnectedState);
	mtpRequest pingRequest;
	if (_shiftedDcId == bareDcId(_shiftedDcId)) { // main session
		if (!prependOnly && !_pingIdToSend && !_pingId && _pingSendAt <= getms(true)) {
			_pingIdToSend = rand_value<mtpPingId>();
		}
	}
	if (_pingIdToSend) {
		if (prependOnly || _shiftedDcId != bareDcId(_shiftedDcId)) {
			MTPPing ping(MTPping(MTP_long(_pingIdToSend)));
			uint32_t pingSize = ping.innerLength() >> 2; // copy from Session::send
			pingRequest = mtpRequestData::prepare(pingSize);
			ping.write(*pingRequest);
			DEBUG_LOG(("MTP Info: sending ping, ping_id: %1").arg(_pingIdToSend));
		} else {
			MTPPing_delay_disconnect ping(MTP_long(_pingIdToSend), MTP_int(MTPPingDelayDisconnect));
			uint32_t pingSize = ping.innerLength() >> 2; // copy from Session::send
			pingRequest = mtpRequestData::prepare(pingSize);
			ping.write(*pingRequest);
			DEBUG_LOG(("MTP Info: sending ping_delay_disconnect, ping_id: %1").arg(_pingIdToSend));
		}

		pingRequest->msDate = getms(true); // > 0 - can send without container
		_pingSendAt = pingRequest->msDate + (MTPPingSendAfterAuto * 1000LL);
		pingRequest->requestId = 0; // dont add to haveSent / wereAcked maps

		if (_shiftedDcId == bareDcId(_shiftedDcId) && !prependOnly) { // main session
			_pingSender.start(MTPPingSendAfter * 1000);
		}

		_pingId = _pingIdToSend;
		_pingIdToSend = 0;
	} else {
		if (prependOnly) {
			DEBUG_LOG(("MTP Info: dc %1 not sending, waiting for Connected state, state: %2").arg(_shiftedDcId).arg(state));
			return; // just do nothing, if is not connected yet
		} else {
			DEBUG_LOG(("MTP Info: dc %1 trying to send after ping, state: %2").arg(_shiftedDcId).arg(state));
		}
	}

	mtpRequest ackRequest, resendRequest, stateRequest, httpWaitRequest;
	if (!prependOnly && !ackRequestData.isEmpty()) {
		MTPMsgsAck ack(MTP_msgs_ack(MTP_vector<MTPlong>(ackRequestData)));

		ackRequest = mtpRequestData::prepare(ack.innerLength() >> 2);
		ack.write(*ackRequest);

		ackRequest->msDate = getms(true); // > 0 - can send without container
		ackRequest->requestId = 0; // dont add to haveSent / wereAcked maps

		ackRequestData.clear();
	}
	if (!prependOnly && !resendRequestData.isEmpty()) {
		MTPMsgResendReq resend(MTP_msg_resend_req(MTP_vector<MTPlong>(resendRequestData)));

		resendRequest = mtpRequestData::prepare(resend.innerLength() >> 2);
		resend.write(*resendRequest);

		resendRequest->msDate = getms(true); // > 0 - can send without container
		resendRequest->requestId = 0; // dont add to haveSent / wereAcked maps

		resendRequestData.clear();
	}
	if (!prependOnly) {
		QVector<MTPlong> stateReq;
		{
			QWriteLocker locker(sessionData->stateRequestMutex());
			mtpMsgIdsSet &ids(sessionData->stateRequestMap());
			if (!ids.isEmpty()) {
				stateReq.reserve(ids.size());
				for (mtpMsgIdsSet::const_iterator i = ids.cbegin(), e = ids.cend(); i != e; ++i) {
					stateReq.push_back(MTP_long(i.key()));
				}
			}
			ids.clear();
		}
		if (!stateReq.isEmpty()) {
			MTPMsgsStateReq req(MTP_msgs_state_req(MTP_vector<MTPlong>(stateReq)));

			stateRequest = mtpRequestData::prepare(req.innerLength() >> 2);
			req.write(*stateRequest);

			stateRequest->msDate = getms(true); // > 0 - can send without container
			stateRequest->requestId = reqid();// add to haveSent / wereAcked maps, but don't add to requestMap
		}
		if (_conn->usingHttpWait()) {
			MTPHttpWait req(MTP_http_wait(MTP_int(100), MTP_int(30), MTP_int(25000)));

			httpWaitRequest = mtpRequestData::prepare(req.innerLength() >> 2);
			req.write(*httpWaitRequest);

			httpWaitRequest->msDate = getms(true); // > 0 - can send without container
			httpWaitRequest->requestId = 0; // dont add to haveSent / wereAcked maps
		}
	}

	MTPInitConnection<mtpRequest> initWrapper;
	int32_t initSize = 0, initSizeInInts = 0;
	if (needsLayer) {
		auto systemLangCode = sessionData->systemLangCode();
		auto cloudLangCode = sessionData->cloudLangCode();
		auto langPack = "tdesktop";
		auto deviceModel = (_dcType == DcType::Cdn) ? "n/a" : cApiDeviceModel();
		auto systemVersion = (_dcType == DcType::Cdn) ? "n/a" : cApiSystemVersion();
		initWrapper = MTPInitConnection<mtpRequest>(MTP_int(ApiId), MTP_string(deviceModel), MTP_string(systemVersion), MTP_string(str_const_toString(AppVersionStr)), MTP_string(systemLangCode), MTP_string(langPack), MTP_string(cloudLangCode), mtpRequest());
		initSizeInInts = (initWrapper.innerLength() >> 2) + 2;
		initSize = initSizeInInts * sizeof(mtpPrime);
	}

	bool needAnyResponse = false;
	mtpRequest toSendRequest;
	{
		QWriteLocker locker1(sessionData->toSendMutex());

		mtpPreRequestMap toSendDummy, &toSend(prependOnly ? toSendDummy : sessionData->toSendMap());
		if (prependOnly) locker1.unlock();

		uint32_t toSendCount = toSend.size();
		if (pingRequest) ++toSendCount;
		if (ackRequest) ++toSendCount;
		if (resendRequest) ++toSendCount;
		if (stateRequest) ++toSendCount;
		if (httpWaitRequest) ++toSendCount;

		if (!toSendCount) return; // nothing to send

		mtpRequest first = pingRequest ? pingRequest : (ackRequest ? ackRequest : (resendRequest ? resendRequest : (stateRequest ? stateRequest : (httpWaitRequest ? httpWaitRequest : toSend.cbegin().value()))));
		if (toSendCount == 1 && first->msDate > 0) { // if can send without container
			toSendRequest = first;
			if (!prependOnly) {
				toSend.clear();
				locker1.unlock();
			}

			mtpMsgId msgId = prepareToSend(toSendRequest, msgid());
			if (pingRequest) {
				_pingMsgId = msgId;
				needAnyResponse = true;
			} else if (resendRequest || stateRequest) {
				needAnyResponse = true;
			}

			if (toSendRequest->requestId) {
				if (mtpRequestData::needAck(toSendRequest)) {
					toSendRequest->msDate = mtpRequestData::isStateRequest(toSendRequest) ? 0 : getms(true);

					QWriteLocker locker2(sessionData->haveSentMutex());
					mtpRequestMap &haveSent(sessionData->haveSentMap());
					haveSent.insert(msgId, toSendRequest);

					if (needsLayer && !toSendRequest->needsLayer) needsLayer = false;
					if (toSendRequest->after) {
						int32_t toSendSize = toSendRequest.innerLength() >> 2;
						mtpRequest wrappedRequest(mtpRequestData::prepare(toSendSize, toSendSize + 3)); // cons + msg_id
						wrappedRequest->resize(4);
						memcpy(wrappedRequest->data(), toSendRequest->constData(), 4 * sizeof(mtpPrime));
						wrapInvokeAfter(wrappedRequest, toSendRequest, haveSent);
						toSendRequest = wrappedRequest;
					}
					if (needsLayer) {
						int32_t noWrapSize = (toSendRequest.innerLength() >> 2), toSendSize = noWrapSize + initSizeInInts;
						mtpRequest wrappedRequest(mtpRequestData::prepare(toSendSize));
						memcpy(wrappedRequest->data(), toSendRequest->constData(), 7 * sizeof(mtpPrime)); // all except length
						wrappedRequest->push_back(mtpc_invokeWithLayer);
						wrappedRequest->push_back(MTP::internal::CurrentLayer);
						initWrapper.write(*wrappedRequest);
						wrappedRequest->resize(wrappedRequest->size() + noWrapSize);
						memcpy(wrappedRequest->data() + wrappedRequest->size() - noWrapSize, toSendRequest->constData() + 8, noWrapSize * sizeof(mtpPrime));
						toSendRequest = wrappedRequest;
					}

					needAnyResponse = true;
				} else {
					QWriteLocker locker3(sessionData->wereAckedMutex());
					sessionData->wereAckedMap().insert(msgId, toSendRequest->requestId);
				}
			}
		} else { // send in container
			bool willNeedInit = false;
			uint32_t containerSize = 1 + 1, idsWrapSize = (toSendCount << 1); // cons + vector size, idsWrapSize - size of "request-like" wrap for msgId vector
			if (pingRequest) containerSize += mtpRequestData::messageSize(pingRequest);
			if (ackRequest) containerSize += mtpRequestData::messageSize(ackRequest);
			if (resendRequest) containerSize += mtpRequestData::messageSize(resendRequest);
			if (stateRequest) containerSize += mtpRequestData::messageSize(stateRequest);
			if (httpWaitRequest) containerSize += mtpRequestData::messageSize(httpWaitRequest);
			for (mtpPreRequestMap::iterator i = toSend.begin(), e = toSend.end(); i != e; ++i) {
				containerSize += mtpRequestData::messageSize(i.value());
				if (needsLayer && i.value()->needsLayer) {
					containerSize += initSizeInInts;
					willNeedInit = true;
				}
			}
			mtpBuffer initSerialized;
			if (willNeedInit) {
				initSerialized.reserve(initSizeInInts);
				initSerialized.push_back(mtpc_invokeWithLayer);
				initSerialized.push_back(MTP::internal::CurrentLayer);
				initWrapper.write(initSerialized);
			}
			toSendRequest = mtpRequestData::prepare(containerSize, containerSize + 3 * toSend.size()); // prepare container + each in invoke after
			toSendRequest->push_back(mtpc_msg_container);
			toSendRequest->push_back(toSendCount);

			mtpMsgId bigMsgId = msgid(); // check for a valid container

			QWriteLocker locker2(sessionData->haveSentMutex()); // the fact of this lock is used in replaceMsgId()
			mtpRequestMap &haveSent(sessionData->haveSentMap());

			QWriteLocker locker3(sessionData->wereAckedMutex()); // the fact of this lock is used in replaceMsgId()
			mtpRequestIdsMap &wereAcked(sessionData->wereAckedMap());

			mtpRequest haveSentIdsWrap(mtpRequestData::prepare(idsWrapSize)); // prepare "request-like" wrap for msgId vector
			haveSentIdsWrap->requestId = 0;
			haveSentIdsWrap->resize(haveSentIdsWrap->size() + idsWrapSize);
			mtpMsgId *haveSentArr = (mtpMsgId*)(haveSentIdsWrap->data() + 8);

			if (pingRequest) {
				_pingMsgId = placeToContainer(toSendRequest, bigMsgId, haveSentArr, pingRequest);
				needAnyResponse = true;
			} else if (resendRequest || stateRequest) {
				needAnyResponse = true;
			}
			for (mtpPreRequestMap::iterator i = toSend.begin(), e = toSend.end(); i != e; ++i) {
				mtpRequest &req(i.value());
				mtpMsgId msgId = prepareToSend(req, bigMsgId);
				if (msgId > bigMsgId) msgId = replaceMsgId(req, bigMsgId);
				if (msgId >= bigMsgId) bigMsgId = msgid();
				*(haveSentArr++) = msgId;
				bool added = false;
				if (req->requestId) {
					if (mtpRequestData::needAck(req)) {
						req->msDate = mtpRequestData::isStateRequest(req) ? 0 : getms(true);
						int32_t reqNeedsLayer = (needsLayer && req->needsLayer) ? toSendRequest->size() : 0;
						if (req->after) {
							wrapInvokeAfter(toSendRequest, req, haveSent, reqNeedsLayer ? initSizeInInts : 0);
							if (reqNeedsLayer) {
								memcpy(toSendRequest->data() + reqNeedsLayer + 4, initSerialized.constData(), initSize);
								*(toSendRequest->data() + reqNeedsLayer + 3) += initSize;
							}
							added = true;
						} else if (reqNeedsLayer) {
							toSendRequest->resize(reqNeedsLayer + initSizeInInts + mtpRequestData::messageSize(req));
							memcpy(toSendRequest->data() + reqNeedsLayer, req->constData() + 4, 4 * sizeof(mtpPrime));
							memcpy(toSendRequest->data() + reqNeedsLayer + 4, initSerialized.constData(), initSize);
							memcpy(toSendRequest->data() + reqNeedsLayer + 4 + initSizeInInts, req->constData() + 8, req.innerLength());
							*(toSendRequest->data() + reqNeedsLayer + 3) += initSize;
							added = true;
						}
						haveSent.insert(msgId, req);

						needAnyResponse = true;
					} else {
						wereAcked.insert(msgId, req->requestId);
					}
				}
				if (!added) {
					uint32_t from = toSendRequest->size(), len = mtpRequestData::messageSize(req);
					toSendRequest->resize(from + len);
					memcpy(toSendRequest->data() + from, req->constData() + 4, len * sizeof(mtpPrime));
				}
			}
			if (stateRequest) {
				mtpMsgId msgId = placeToContainer(toSendRequest, bigMsgId, haveSentArr, stateRequest);
				stateRequest->msDate = 0; // 0 for state request, do not request state of it
				haveSent.insert(msgId, stateRequest);
			}
			if (resendRequest) placeToContainer(toSendRequest, bigMsgId, haveSentArr, resendRequest);
			if (ackRequest) placeToContainer(toSendRequest, bigMsgId, haveSentArr, ackRequest);
			if (httpWaitRequest) placeToContainer(toSendRequest, bigMsgId, haveSentArr, httpWaitRequest);

			mtpMsgId contMsgId = prepareToSend(toSendRequest, bigMsgId);
			*(mtpMsgId*)(haveSentIdsWrap->data() + 4) = contMsgId;
			(*haveSentIdsWrap)[6] = 0; // for container, msDate = 0, seqNo = 0
			haveSent.insert(contMsgId, haveSentIdsWrap);
			toSend.clear();
		}
	}
	mtpRequestData::padding(toSendRequest);
	sendRequest(toSendRequest, needAnyResponse, lockFinished);
}

void ConnectionPrivate::retryByTimer() {
	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData) return;

	if (retryTimeout < 3) {
		++retryTimeout;
	} else if (retryTimeout == 3) {
		retryTimeout = 1000;
	} else if (retryTimeout < 64000) {
		retryTimeout *= 2;
	}
	if (keyId == kRecreateKeyId) {
		if (sessionData->getKey()) {
			unlockKey();

			QWriteLocker lock(sessionData->keyMutex());
			sessionData->owner()->destroyKey();
		}
		keyId = 0;
	}
	connectToServer();
}

void ConnectionPrivate::restartNow() {
	retryTimeout = 1;
	retryTimer.stop();
	restart();
}

void ConnectionPrivate::connectToServer(bool afterConfig) {
	if (_finished) {
		DEBUG_LOG(("MTP Error: connectToServer() called for finished connection!"));
		return;
	}
	auto bareDc = bareDcId(_shiftedDcId);
	_dcType = _instance->dcOptions()->dcType(_shiftedDcId);
	if (_dcType == DcType::MediaDownload) { // using media_only addresses only if key for this dc is already created
		QReadLocker lockFinished(&sessionDataMutex);
		if (!sessionData || !sessionData->getKey()) {
			_dcType = DcType::Regular;
		}
	} else if (_dcType == DcType::Cdn && !_instance->isKeysDestroyer()) {
		if (!_instance->dcOptions()->hasCDNKeysForDc(bareDc)) {
			requestCDNConfig();
			return;
		}
	}

	using Variants = DcOptions::Variants;
	auto kIPv4 = Variants::IPv4;
	auto kIPv6 = Variants::IPv6;
	auto kTcp = Variants::Tcp;
	auto kHttp = Variants::Http;
	auto variants = _instance->dcOptions()->lookup(bareDc, _dcType);
	auto noIPv4 = (_dcType == DcType::Temporary) ? (variants.data[kIPv4][kTcp].port == 0) : (variants.data[kIPv4][kHttp].port == 0);
	auto noIPv6 = (_dcType == DcType::Temporary) ? true : (!Global::TryIPv6() || (variants.data[kIPv6][kHttp].port == 0));
	if (noIPv4 && noIPv6) {
		if (_instance->isKeysDestroyer()) {
			LOG(("MTP Error: DC %1 options for IPv4 over HTTP not found for auth key destruction!").arg(_shiftedDcId));
			if (Global::TryIPv6() && noIPv6) LOG(("MTP Error: DC %1 options for IPv6 over HTTP not found for auth key destruction!").arg(_shiftedDcId));
			emit _instance->keyDestroyed(_shiftedDcId);
			return;
		} else if (afterConfig) {
			LOG(("MTP Error: DC %1 options for IPv4 over HTTP not found right after config load!").arg(_shiftedDcId));
			if (Global::TryIPv6() && noIPv6) LOG(("MTP Error: DC %1 options for IPv6 over HTTP not found right after config load!").arg(_shiftedDcId));
			return restart();
		}
		DEBUG_LOG(("MTP Info: DC %1 options for IPv4 over HTTP not found, waiting for config").arg(_shiftedDcId));
		if (Global::TryIPv6() && noIPv6) DEBUG_LOG(("MTP Info: DC %1 options for IPv6 over HTTP not found, waiting for config").arg(_shiftedDcId));
		connect(_instance, SIGNAL(configLoaded()), this, SLOT(onConfigLoaded()), Qt::UniqueConnection);
		InvokeQueued(_instance, [instance = _instance] { instance->requestConfig(); });
		return;
	}

	if (afterConfig && (_conn4 || _conn6)) return;

	createConn(!noIPv4, !noIPv6);
	retryTimer.stop();
	_waitForConnectedTimer.stop();

	setState(ConnectingState);
	_pingId = _pingMsgId = _pingIdToSend = _pingSendAt = 0;
	_pingSender.stop();

	if (!noIPv4) DEBUG_LOG(("MTP Info: creating IPv4 connection to %1:%2 (tcp) and %3:%4 (http)...").arg(variants.data[kIPv4][kTcp].ip.c_str()).arg(variants.data[kIPv4][kTcp].port).arg(variants.data[kIPv4][kHttp].ip.c_str()).arg(variants.data[kIPv4][kHttp].port));
	if (!noIPv6) DEBUG_LOG(("MTP Info: creating IPv6 connection to [%1]:%2 (tcp) and [%3]:%4 (http)...").arg(variants.data[kIPv6][kTcp].ip.c_str()).arg(variants.data[kIPv6][kTcp].port).arg(variants.data[kIPv4][kHttp].ip.c_str()).arg(variants.data[kIPv4][kHttp].port));

	_waitForConnectedTimer.start(_waitForConnected);
	if (auto conn = _conn4) {
		connect(conn, SIGNAL(connected()), this, SLOT(onConnected4()));
		connect(conn, SIGNAL(disconnected()), this, SLOT(onDisconnected4()));
		conn->connectTcp(variants.data[kIPv4][kTcp]);
		conn->connectHttp(variants.data[kIPv4][kHttp]);
	}
	if (auto conn = _conn6) {
		connect(conn, SIGNAL(connected()), this, SLOT(onConnected6()));
		connect(conn, SIGNAL(disconnected()), this, SLOT(onDisconnected6()));
		conn->connectTcp(variants.data[kIPv6][kTcp]);
		conn->connectHttp(variants.data[kIPv6][kHttp]);
	}
}

void ConnectionPrivate::restart() {
	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData) return;

	DEBUG_LOG(("MTP Info: restarting Connection"));

	_waitForReceivedTimer.stop();
	_waitForConnectedTimer.stop();

	auto key = sessionData->getKey();
	if (key) {
		if (!sessionData->isCheckedKey()) {
			// No destroying in case of an error.
			//
			//if (mayBeBadKey) {
			//	clearMessages();
			//	keyId = kRecreateKeyId;
//				retryTimeout = 1; // no ddos please
			//	LOG(("MTP Info: key may be bad and was not checked - but won't be destroyed, no log outs because of bad server right now..."));
			//}
		} else {
			sessionData->setCheckedKey(false);
		}
	}

	lockFinished.unlock();
	doDisconnect();

	lockFinished.relock();
	if (sessionData && _needSessionReset) {
		resetSession();
	}
	restarted = true;
	if (retryTimer.isActive()) return;

	DEBUG_LOG(("MTP Info: restart timeout: %1ms").arg(retryTimeout));
	setState(-retryTimeout);
}

void ConnectionPrivate::onSentSome(uint64_t size) {
	if (!_waitForReceivedTimer.isActive()) {
		auto remain = static_cast<uint64_t>(_waitForReceived);
		if (!oldConnection) {
			auto remainBySize = size * _waitForReceived / 8192; // 8kb / sec, so 512 kb give 64 sec
			remain = snap(remainBySize, remain, uint64_t(MTPMaxReceiveDelay));
			if (remain != _waitForReceived) {
				DEBUG_LOG(("Checking connect for request with size %1 bytes, delay will be %2").arg(size).arg(remain));
			}
		}
		if (isUploadDcId(_shiftedDcId)) {
			remain *= kUploadSessionsCount;
		} else if (isDownloadDcId(_shiftedDcId)) {
			remain *= kDownloadSessionsCount;
		}
		_waitForReceivedTimer.start(remain);
	}
	if (!firstSentAt) firstSentAt = getms(true);
}

void ConnectionPrivate::onReceivedSome() {
	if (oldConnection) {
		oldConnection = false;
		DEBUG_LOG(("This connection marked as not old!"));
	}
	oldConnectionTimer.start(MTPConnectionOldTimeout);
	_waitForReceivedTimer.stop();
	if (firstSentAt > 0) {
		int32_t ms = getms(true) - firstSentAt;
		DEBUG_LOG(("MTP Info: response in %1ms, _waitForReceived: %2ms").arg(ms).arg(_waitForReceived));

		if (ms > 0 && ms * 2 < int32_t(_waitForReceived)) _waitForReceived = qMax(ms * 2, int32_t(MTPMinReceiveDelay));
		firstSentAt = -1;
	}
}

void ConnectionPrivate::onOldConnection() {
	oldConnection = true;
	_waitForReceived = MTPMinReceiveDelay;
	DEBUG_LOG(("This connection marked as old! _waitForReceived now %1ms").arg(_waitForReceived));
}

void ConnectionPrivate::onPingSender() {
	if (_pingId) {
		if (_pingSendAt + (MTPPingSendAfter - MTPPingSendAfterAuto - 1) * 1000LL < getms(true)) {
			LOG(("Could not send ping for MTPPingSendAfter seconds, restarting..."));
			return restart();
		} else {
			_pingSender.start(_pingSendAt + (MTPPingSendAfter - MTPPingSendAfterAuto) * 1000LL - getms(true));
		}
	} else {
		emit needToSendAsync();
	}
}

void ConnectionPrivate::onPingSendForce() {
	if (!_pingId) {
		_pingSendAt = 0;
		DEBUG_LOG(("Will send ping!"));
		tryToSend();
	}
}

void ConnectionPrivate::onWaitReceivedFailed() {
	if (Global::ConnectionType() != dbictAuto && Global::ConnectionType() != dbictTcpProxy) {
		return;
	}

	DEBUG_LOG(("MTP Info: bad connection, _waitForReceived: %1ms").arg(_waitForReceived));
	if (_waitForReceived < MTPMaxReceiveDelay) {
		_waitForReceived *= 2;
	}
	doDisconnect();
	restarted = true;
	if (retryTimer.isActive()) return;

	DEBUG_LOG(("MTP Info: immediate restart!"));
	InvokeQueued(this, [this] { connectToServer(); });
}

void ConnectionPrivate::onWaitConnectedFailed() {
	DEBUG_LOG(("MTP Info: can't connect in %1ms").arg(_waitForConnected));
	if (_waitForConnected < MTPMaxConnectDelay) _waitForConnected *= 2;

	doDisconnect();
	restarted = true;

	DEBUG_LOG(("MTP Info: immediate restart!"));
	InvokeQueued(this, [this] { connectToServer(); });
}

void ConnectionPrivate::onWaitIPv4Failed() {
	_conn = _conn6;
	destroyConn(&_conn4);

	if (_conn) {
		DEBUG_LOG(("MTP Info: can't connect through IPv4, using IPv6 connection."));

		updateAuthKey();
	} else {
		restart();
	}
}

void ConnectionPrivate::doDisconnect() {
	destroyConn();

	{
		QReadLocker lockFinished(&sessionDataMutex);
		if (sessionData) {
			unlockKey();
		}
	}

	clearAuthKeyData();

	setState(DisconnectedState);
	restarted = false;
}

void ConnectionPrivate::finishAndDestroy() {
	doDisconnect();
	_finished = true;
	emit finished(_owner);
	deleteLater();
}

void ConnectionPrivate::requestCDNConfig() {
	connect(_instance, SIGNAL(cdnConfigLoaded()), this, SLOT(onCDNConfigLoaded()), Qt::UniqueConnection);
	InvokeQueued(_instance, [instance = _instance] { instance->requestCDNConfig(); });
}

void ConnectionPrivate::handleReceived() {
	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData) return;

	onReceivedSome();

	auto restartOnError = [this, &lockFinished] {
		lockFinished.unlock();
		restart();
	};

	ReadLockerAttempt lock(sessionData->keyMutex());
	if (!lock) {
		DEBUG_LOG(("MTP Error: auth_key for dc %1 busy, cant lock").arg(_shiftedDcId));
		clearMessages();
		keyId = 0;

		return restartOnError();
	}

	auto key = sessionData->getKey();
	if (!key || key->keyId() != keyId) {
		DEBUG_LOG(("MTP Error: auth_key id for dc %1 changed").arg(_shiftedDcId));
		return restartOnError();
	}

	while (!_conn->received().empty()) {
		auto intsBuffer = std::move(_conn->received().front());
		_conn->received().pop_front();

		constexpr auto kExternalHeaderIntsCount = 6U; // 2 auth_key_id, 4 msg_key
		constexpr auto kEncryptedHeaderIntsCount = 8U; // 2 salt, 2 session, 2 msg_id, 1 seq_no, 1 length
		constexpr auto kMinimalEncryptedIntsCount = kEncryptedHeaderIntsCount + 4U; // + 1 data + 3 padding
		constexpr auto kMinimalIntsCount = kExternalHeaderIntsCount + kMinimalEncryptedIntsCount;
		auto intsCount = uint32_t(intsBuffer.size());
		auto ints = intsBuffer.constData();
		if ((intsCount < kMinimalIntsCount) || (intsCount > kMaxMessageLength / kIntSize)) {
			LOG(("TCP Error: bad message received, len %1").arg(intsCount * kIntSize));
			TCP_LOG(("TCP Error: bad message %1").arg(Logs::mb(ints, intsCount * kIntSize).str()));

			return restartOnError();
		}
		if (keyId != *(uint64_t*)ints) {
			LOG(("TCP Error: bad auth_key_id %1 instead of %2 received").arg(keyId).arg(*(uint64_t*)ints));
			TCP_LOG(("TCP Error: bad message %1").arg(Logs::mb(ints, intsCount * kIntSize).str()));

			return restartOnError();
		}

		auto encryptedInts = ints + kExternalHeaderIntsCount;
		auto encryptedIntsCount = (intsCount - kExternalHeaderIntsCount);
		auto encryptedBytesCount = encryptedIntsCount * kIntSize;
		auto decryptedBuffer = QByteArray(encryptedBytesCount, Qt::Uninitialized);
		auto msgKey = *(MTPint128*)(ints + 2);

#ifdef TDESKTOP_MTPROTO_OLD
		aesIgeDecrypt_oldmtp(encryptedInts, decryptedBuffer.data(), encryptedBytesCount, key, msgKey);
#else // TDESKTOP_MTPROTO_OLD
		aesIgeDecrypt(encryptedInts, decryptedBuffer.data(), encryptedBytesCount, key, msgKey);
#endif // TDESKTOP_MTPROTO_OLD

		auto decryptedInts = reinterpret_cast<const mtpPrime*>(decryptedBuffer.constData());
		auto serverSalt = *(uint64_t*)&decryptedInts[0];
		auto session = *(uint64_t*)&decryptedInts[2];
		auto msgId = *(uint64_t*)&decryptedInts[4];
		auto seqNo = *(uint32_t*)&decryptedInts[6];
		auto needAck = ((seqNo & 0x01) != 0);

		auto messageLength = *(uint32_t*)&decryptedInts[7];
		if (messageLength > kMaxMessageLength) {
			LOG(("TCP Error: bad messageLength %1").arg(messageLength));
			TCP_LOG(("TCP Error: bad message %1").arg(Logs::mb(ints, intsCount * kIntSize).str()));

			return restartOnError();

		}
		auto fullDataLength = kEncryptedHeaderIntsCount * kIntSize + messageLength; // Without padding.

		// Can underflow, but it is an unsigned type, so we just check the range later.
		auto paddingSize = static_cast<uint32_t>(encryptedBytesCount) - static_cast<uint32_t>(fullDataLength);

#ifdef TDESKTOP_MTPROTO_OLD
		constexpr auto kMinPaddingSize_oldmtp = 0U;
		constexpr auto kMaxPaddingSize_oldmtp = 15U;
		auto badMessageLength = (/*paddingSize < kMinPaddingSize_oldmtp || */paddingSize > kMaxPaddingSize_oldmtp);

		auto hashedDataLength = badMessageLength ? encryptedBytesCount : fullDataLength;
		auto sha1ForMsgKeyCheck = hashSha1(decryptedInts, hashedDataLength);

		constexpr auto kMsgKeyShift_oldmtp = 4U;
		if (memcmp(&msgKey, sha1ForMsgKeyCheck.data() + kMsgKeyShift_oldmtp, sizeof(msgKey)) != 0) {
			LOG(("TCP Error: bad SHA1 hash after aesDecrypt in message."));
			TCP_LOG(("TCP Error: bad message %1").arg(Logs::mb(encryptedInts, encryptedBytesCount).str()));

			return restartOnError();
		}
#else // TDESKTOP_MTPROTO_OLD
		constexpr auto kMinPaddingSize = 12U;
		constexpr auto kMaxPaddingSize = 1024U;
		auto badMessageLength = (paddingSize < kMinPaddingSize || paddingSize > kMaxPaddingSize);

		std::array<uchar, 32> sha256Buffer = { { 0 } };

		SHA256_CTX msgKeyLargeContext;
		SHA256_Init(&msgKeyLargeContext);
		SHA256_Update(&msgKeyLargeContext, key->partForMsgKey(false), 32);
		SHA256_Update(&msgKeyLargeContext, decryptedInts, encryptedBytesCount);
		SHA256_Final(sha256Buffer.data(), &msgKeyLargeContext);

		constexpr auto kMsgKeyShift = 8U;
		if (memcmp(&msgKey, sha256Buffer.data() + kMsgKeyShift, sizeof(msgKey)) != 0) {
			LOG(("TCP Error: bad SHA256 hash after aesDecrypt in message"));
			TCP_LOG(("TCP Error: bad message %1").arg(Logs::mb(encryptedInts, encryptedBytesCount).str()));

			return restartOnError();
		}
#endif // TDESKTOP_MTPROTO_OLD

		if (badMessageLength || (messageLength & 0x03)) {
			LOG(("TCP Error: bad msg_len received %1, data size: %2").arg(messageLength).arg(encryptedBytesCount));
			TCP_LOG(("TCP Error: bad message %1").arg(Logs::mb(encryptedInts, encryptedBytesCount).str()));

			return restartOnError();
		}

		TCP_LOG(("TCP Info: decrypted message %1,%2,%3 is %4 len").arg(msgId).arg(seqNo).arg(Logs::b(needAck)).arg(fullDataLength));

		uint64_t serverSession = sessionData->getSession();
		if (session != serverSession) {
			LOG(("MTP Error: bad server session received"));
			TCP_LOG(("MTP Error: bad server session %1 instead of %2 in message received").arg(session).arg(serverSession));

			return restartOnError();
		}

		int32_t serverTime((int32_t)(msgId >> 32)), clientTime(unixtime());
		bool isReply = ((msgId & 0x03) == 1);
		if (!isReply && ((msgId & 0x03) != 3)) {
			LOG(("MTP Error: bad msg_id %1 in message received").arg(msgId));

			return restartOnError();
		}

		bool badTime = false;
		uint64_t mySalt = sessionData->getSalt();
		if (serverTime > clientTime + 60 || serverTime + 300 < clientTime) {
			DEBUG_LOG(("MTP Info: bad server time from msg_id: %1, my time: %2").arg(serverTime).arg(clientTime));
			badTime = true;
		}

		bool wasConnected = (getState() == ConnectedState);
		if (serverSalt != mySalt) {
			if (!badTime) {
				DEBUG_LOG(("MTP Info: other salt received... received: %1, my salt: %2, updating...").arg(serverSalt).arg(mySalt));
				sessionData->setSalt(serverSalt);
				if (setState(ConnectedState, ConnectingState)) { // only connected
					if (restarted) {
						emit resendAllAsync();
						restarted = false;
					}
				}
			} else {
				DEBUG_LOG(("MTP Info: other salt received... received: %1, my salt: %2").arg(serverSalt).arg(mySalt));
			}
		} else {
			serverSalt = 0; // dont pass to handle method, so not to lock in setSalt()
		}

		if (needAck) ackRequestData.push_back(MTP_long(msgId));

		auto res = HandleResult::Success; // if no need to handle, then succeed
		auto from = decryptedInts + kEncryptedHeaderIntsCount;
		auto end = from + (messageLength / kIntSize);
		auto sfrom = decryptedInts + 4U; // msg_id + seq_no + length + message
		MTP_LOG(_shiftedDcId, ("Recv: ") + mtpTextSerialize(sfrom, end));

		bool needToHandle = false;
		{
			QWriteLocker lock(sessionData->receivedIdsMutex());
			needToHandle = sessionData->receivedIdsSet().registerMsgId(msgId, needAck);
		}
		if (needToHandle) {
			res = handleOneReceived(from, end, msgId, serverTime, serverSalt, badTime);
		}
		{
			QWriteLocker lock(sessionData->receivedIdsMutex());
			sessionData->receivedIdsSet().shrink();
		}

		// send acks
		uint32_t toAckSize = ackRequestData.size();
		if (toAckSize) {
			DEBUG_LOG(("MTP Info: will send %1 acks, ids: %2").arg(toAckSize).arg(Logs::vector(ackRequestData)));
			emit sendAnythingAsync(MTPAckSendWaiting);
		}

		bool emitSignal = false;
		{
			QReadLocker locker(sessionData->haveReceivedMutex());
			emitSignal = !sessionData->haveReceivedResponses().isEmpty() || !sessionData->haveReceivedUpdates().isEmpty();
			if (emitSignal) {
				DEBUG_LOG(("MTP Info: emitting needToReceive() - need to parse in another thread, %1 responses, %2 updates.").arg(sessionData->haveReceivedResponses().size()).arg(sessionData->haveReceivedUpdates().size()));
			}
		}

		if (emitSignal) {
			emit needToReceive();
		}

		if (res != HandleResult::Success && res != HandleResult::Ignored) {
			_needSessionReset = (res == HandleResult::ResetSession);

			return restartOnError();
		}
		retryTimeout = 1; // reset restart() timer

		if (!sessionData->isCheckedKey()) {
			DEBUG_LOG(("MTP Info: marked auth key as checked"));
			sessionData->setCheckedKey(true);
		}

		if (!wasConnected) {
			if (getState() == ConnectedState) {
				emit needToSendAsync();
			}
		}
	}
	if (_conn->needHttpWait()) {
		emit sendHttpWaitAsync();
	}
}

ConnectionPrivate::HandleResult ConnectionPrivate::handleOneReceived(const mtpPrime *from, const mtpPrime *end, uint64_t msgId, int32_t serverTime, uint64_t serverSalt, bool badTime) {
	mtpTypeId cons = *from;
	try {

	switch (cons) {

	case mtpc_gzip_packed: {
		DEBUG_LOG(("Message Info: gzip container"));
		mtpBuffer response = ungzip(++from, end);
		if (!response.size()) {
			return HandleResult::RestartConnection;
		}
		return handleOneReceived(response.data(), response.data() + response.size(), msgId, serverTime, serverSalt, badTime);
	}

	case mtpc_msg_container: {
		if (++from >= end) throw mtpErrorInsufficient();

		const mtpPrime *otherEnd;
		uint32_t msgsCount = (uint32_t)*(from++);
		DEBUG_LOG(("Message Info: container received, count: %1").arg(msgsCount));
		for (uint32_t i = 0; i < msgsCount; ++i) {
			if (from + 4 >= end) throw mtpErrorInsufficient();
			otherEnd = from + 4;

			MTPlong inMsgId;
			inMsgId.read(from, otherEnd);
			bool isReply = ((inMsgId.v & 0x03) == 1);
			if (!isReply && ((inMsgId.v & 0x03) != 3)) {
				LOG(("Message Error: bad msg_id %1 in contained message received").arg(inMsgId.v));
				return HandleResult::RestartConnection;
			}

			MTPint inSeqNo;
			inSeqNo.read(from, otherEnd);
			MTPint bytes;
			bytes.read(from, otherEnd);
			if ((bytes.v & 0x03) || bytes.v < 4) {
				LOG(("Message Error: bad length %1 of contained message received").arg(bytes.v));
				return HandleResult::RestartConnection;
			}

			bool needAck = (inSeqNo.v & 0x01);
			if (needAck) ackRequestData.push_back(inMsgId);

			DEBUG_LOG(("Message Info: message from container, msg_id: %1, needAck: %2").arg(inMsgId.v).arg(Logs::b(needAck)));

			otherEnd = from + (bytes.v >> 2);
			if (otherEnd > end) throw mtpErrorInsufficient();

			bool needToHandle = false;
			{
				QWriteLocker lock(sessionData->receivedIdsMutex());
				needToHandle = sessionData->receivedIdsSet().registerMsgId(inMsgId.v, needAck);
			}
			auto res = HandleResult::Success; // if no need to handle, then succeed
			if (needToHandle) {
				res = handleOneReceived(from, otherEnd, inMsgId.v, serverTime, serverSalt, badTime);
				badTime = false;
			}
			if (res != HandleResult::Success) {
				return res;
			}

			from = otherEnd;
		}
	} return HandleResult::Success;

	case mtpc_msgs_ack: {
		MTPMsgsAck msg;
		msg.read(from, end);
		auto &ids = msg.c_msgs_ack().vmsg_ids.v;
		uint32_t idsCount = ids.size();

		DEBUG_LOG(("Message Info: acks received, ids: %1").arg(Logs::vector(ids)));
		if (!idsCount) return (badTime ? HandleResult::Ignored : HandleResult::Success);

		if (badTime) {
			if (requestsFixTimeSalt(ids, serverTime, serverSalt)) {
				badTime = false;
			} else {
				return HandleResult::Ignored;
			}
		}
		requestsAcked(ids);
	} return HandleResult::Success;

	case mtpc_bad_msg_notification: {
		MTPBadMsgNotification msg;
		msg.read(from, end);
		const auto &data(msg.c_bad_msg_notification());
		LOG(("Message Info: bad message notification received (error_code %3) for msg_id = %1, seq_no = %2").arg(data.vbad_msg_id.v).arg(data.vbad_msg_seqno.v).arg(data.verror_code.v));

		mtpMsgId resendId = data.vbad_msg_id.v;
		if (resendId == _pingMsgId) {
			_pingId = 0;
		}
		int32_t errorCode = data.verror_code.v;
		if (errorCode == 16 || errorCode == 17 || errorCode == 32 || errorCode == 33 || errorCode == 64) { // can handle
			bool needResend = (errorCode == 16 || errorCode == 17); // bad msg_id
			if (errorCode == 64) { // bad container!
				needResend = true;
				if (cDebug()) {
					mtpRequest request;
					{
						QWriteLocker locker(sessionData->haveSentMutex());
						mtpRequestMap &haveSent(sessionData->haveSentMap());

						mtpRequestMap::const_iterator i = haveSent.constFind(resendId);
						if (i == haveSent.cend()) {
							LOG(("Message Error: Container not found!"));
						} else {
							request = i.value();
						}
					}
					if (request) {
						if (mtpRequestData::isSentContainer(request)) {
							QStringList lst;
							const mtpMsgId *ids = (const mtpMsgId *)(request->constData() + 8);
							for (uint32_t i = 0, l = (request->size() - 8) >> 1; i < l; ++i) {
								lst.push_back(QString::number(ids[i]));
							}
							LOG(("Message Info: bad container received! messages: %1").arg(lst.join(',')));
						} else {
							LOG(("Message Error: bad container received, but request is not a container!"));
						}
					}
				}
			}

			if (!wasSent(resendId)) {
				DEBUG_LOG(("Message Error: such message was not sent recently %1").arg(resendId));
				return (badTime ? HandleResult::Ignored : HandleResult::Success);
			}

			if (needResend) { // bad msg_id
				if (serverSalt) sessionData->setSalt(serverSalt);
				unixtimeSet(serverTime, true);

				DEBUG_LOG(("Message Info: unixtime updated, now %1, resending in container...").arg(serverTime));

				resend(resendId, 0, true);
			} else { // must create new session, because msg_id and msg_seqno are inconsistent
				if (badTime) {
					if (serverSalt) sessionData->setSalt(serverSalt);
					unixtimeSet(serverTime, true);
					badTime = false;
				}
				LOG(("Message Info: bad message notification received, msgId %1, error_code %2").arg(data.vbad_msg_id.v).arg(errorCode));
				return HandleResult::ResetSession;
			}
		} else { // fatal (except 48, but it must not get here)
			mtpMsgId resendId = data.vbad_msg_id.v;
			mtpRequestId requestId = wasSent(resendId);
			if (requestId) {
				LOG(("Message Error: bad message notification received, msgId %1, error_code %2, fatal: clearing callbacks").arg(data.vbad_msg_id.v).arg(errorCode));
				_instance->clearCallbacksDelayed(RPCCallbackClears(1, RPCCallbackClear(requestId, -errorCode)));
			} else {
				DEBUG_LOG(("Message Error: such message was not sent recently %1").arg(resendId));
			}
			return (badTime ? HandleResult::Ignored : HandleResult::Success);
		}
	} return HandleResult::Success;

	case mtpc_bad_server_salt: {
		MTPBadMsgNotification msg;
		msg.read(from, end);
		const auto &data(msg.c_bad_server_salt());
		DEBUG_LOG(("Message Info: bad server salt received (error_code %4) for msg_id = %1, seq_no = %2, new salt: %3").arg(data.vbad_msg_id.v).arg(data.vbad_msg_seqno.v).arg(data.vnew_server_salt.v).arg(data.verror_code.v));

		mtpMsgId resendId = data.vbad_msg_id.v;
		if (resendId == _pingMsgId) {
			_pingId = 0;
		} else if (!wasSent(resendId)) {
			DEBUG_LOG(("Message Error: such message was not sent recently %1").arg(resendId));
			return (badTime ? HandleResult::Ignored : HandleResult::Success);
		}

		uint64_t serverSalt = data.vnew_server_salt.v;
		sessionData->setSalt(serverSalt);
		unixtimeSet(serverTime);

		if (setState(ConnectedState, ConnectingState)) { // maybe only connected
			if (restarted) {
				emit resendAllAsync();
				restarted = false;
			}
		}

		badTime = false;

		DEBUG_LOG(("Message Info: unixtime updated, now %1, server_salt updated, now %2, resending...").arg(serverTime).arg(serverSalt));
		resend(resendId);
	} return HandleResult::Success;

	case mtpc_msgs_state_req: {
		if (badTime) {
			DEBUG_LOG(("Message Info: skipping with bad time..."));
			return HandleResult::Ignored;
		}
		MTPMsgsStateReq msg;
		msg.read(from, end);
		auto &ids = msg.c_msgs_state_req().vmsg_ids.v;
		auto idsCount = ids.size();
		DEBUG_LOG(("Message Info: msgs_state_req received, ids: %1").arg(Logs::vector(ids)));
		if (!idsCount) return HandleResult::Success;

		QByteArray info(idsCount, Qt::Uninitialized);
		{
			QReadLocker lock(sessionData->receivedIdsMutex());
			auto &receivedIds = sessionData->receivedIdsSet();
			auto minRecv = receivedIds.min();
			auto maxRecv = receivedIds.max();

			QReadLocker locker(sessionData->wereAckedMutex());
			const mtpRequestIdsMap &wereAcked(sessionData->wereAckedMap());
			mtpRequestIdsMap::const_iterator wereAckedEnd(wereAcked.cend());

			for (uint32_t i = 0, l = idsCount; i < l; ++i) {
				char state = 0;
				uint64_t reqMsgId = ids[i].v;
				if (reqMsgId < minRecv) {
					state |= 0x01;
				} else if (reqMsgId > maxRecv) {
					state |= 0x03;
				} else {
					auto msgIdState = receivedIds.lookup(reqMsgId);
					if (msgIdState == ReceivedMsgIds::State::NotFound) {
						state |= 0x02;
					} else {
						state |= 0x04;
						if (wereAcked.constFind(reqMsgId) != wereAckedEnd) {
							state |= 0x80; // we know, that server knows, that we received request
						}
						if (msgIdState == ReceivedMsgIds::State::NeedsAck) { // need ack, so we sent ack
							state |= 0x08;
						} else {
							state |= 0x10;
						}
					}
				}
				info[i] = state;
			}
		}
		emit sendMsgsStateInfoAsync(msgId, info);
	} return HandleResult::Success;

	case mtpc_msgs_state_info: {
		MTPMsgsStateInfo msg;
		msg.read(from, end);
		auto &data = msg.c_msgs_state_info();

		auto reqMsgId = data.vreq_msg_id.v;
		auto &states = data.vinfo.v;

		DEBUG_LOG(("Message Info: msg state received, msgId %1, reqMsgId: %2, HEX states %3").arg(msgId).arg(reqMsgId).arg(Logs::mb(states.data(), states.length()).str()));
		mtpRequest requestBuffer;
		{ // find this request in session-shared sent requests map
			QReadLocker locker(sessionData->haveSentMutex());
			const mtpRequestMap &haveSent(sessionData->haveSentMap());
			mtpRequestMap::const_iterator replyTo = haveSent.constFind(reqMsgId);
			if (replyTo == haveSent.cend()) { // do not look in toResend, because we do not resend msgs_state_req requests
				DEBUG_LOG(("Message Error: such message was not sent recently %1").arg(reqMsgId));
				return (badTime ? HandleResult::Ignored : HandleResult::Success);
			}
			if (badTime) {
				if (serverSalt) sessionData->setSalt(serverSalt); // requestsFixTimeSalt with no lookup
				unixtimeSet(serverTime, true);

				DEBUG_LOG(("Message Info: unixtime updated from mtpc_msgs_state_info, now %1").arg(serverTime));

				badTime = false;
			}
			requestBuffer = replyTo.value();
		}
		QVector<MTPlong> toAckReq(1, MTP_long(reqMsgId)), toAck;
		requestsAcked(toAck, true);

		if (requestBuffer->size() < 9) {
			LOG(("Message Error: bad request %1 found in requestMap, size: %2").arg(reqMsgId).arg(requestBuffer->size()));
			return HandleResult::RestartConnection;
		}
		try {
			const mtpPrime *rFrom = requestBuffer->constData() + 8, *rEnd = requestBuffer->constData() + requestBuffer->size();
			if (mtpTypeId(*rFrom) == mtpc_msgs_state_req) {
				MTPMsgsStateReq request;
				request.read(rFrom, rEnd);
				handleMsgsStates(request.c_msgs_state_req().vmsg_ids.v, states, toAck);
			} else {
				MTPMsgResendReq request;
				request.read(rFrom, rEnd);
				handleMsgsStates(request.c_msg_resend_req().vmsg_ids.v, states, toAck);
			}
		} catch(Exception &) {
			LOG(("Message Error: could not parse sent msgs_state_req"));
			throw;
		}

		requestsAcked(toAck);
	} return HandleResult::Success;

	case mtpc_msgs_all_info: {
		if (badTime) {
			DEBUG_LOG(("Message Info: skipping with bad time..."));
			return HandleResult::Ignored;
		}

		MTPMsgsAllInfo msg;
		msg.read(from, end);
		auto &data = msg.c_msgs_all_info();
		auto &ids = data.vmsg_ids.v;
		auto &states = data.vinfo.v;

		QVector<MTPlong> toAck;

		DEBUG_LOG(("Message Info: msgs all info received, msgId %1, reqMsgIds: %2, states %3").arg(msgId).arg(Logs::vector(ids)).arg(Logs::mb(states.data(), states.length()).str()));
		handleMsgsStates(ids, states, toAck);

		requestsAcked(toAck);
	} return HandleResult::Success;

	case mtpc_msg_detailed_info: {
		MTPMsgDetailedInfo msg;
		msg.read(from, end);
		const auto &data(msg.c_msg_detailed_info());

		DEBUG_LOG(("Message Info: msg detailed info, sent msgId %1, answerId %2, status %3, bytes %4").arg(data.vmsg_id.v).arg(data.vanswer_msg_id.v).arg(data.vstatus.v).arg(data.vbytes.v));

		QVector<MTPlong> ids(1, data.vmsg_id);
		if (badTime) {
			if (requestsFixTimeSalt(ids, serverTime, serverSalt)) {
				badTime = false;
			} else {
				DEBUG_LOG(("Message Info: error, such message was not sent recently %1").arg(data.vmsg_id.v));
				return HandleResult::Ignored;
			}
		}
		requestsAcked(ids);

		bool received = false;
		MTPlong resMsgId = data.vanswer_msg_id;
		{
			QReadLocker lock(sessionData->receivedIdsMutex());
			received = (sessionData->receivedIdsSet().lookup(resMsgId.v) != ReceivedMsgIds::State::NotFound);
		}
		if (received) {
			ackRequestData.push_back(resMsgId);
		} else {
			DEBUG_LOG(("Message Info: answer message %1 was not received, requesting...").arg(resMsgId.v));
			resendRequestData.push_back(resMsgId);
		}
	} return HandleResult::Success;

	case mtpc_msg_new_detailed_info: {
		if (badTime) {
			DEBUG_LOG(("Message Info: skipping msg_new_detailed_info with bad time..."));
			return HandleResult::Ignored;
		}
		MTPMsgDetailedInfo msg;
		msg.read(from, end);
		const auto &data(msg.c_msg_new_detailed_info());

		DEBUG_LOG(("Message Info: msg new detailed info, answerId %2, status %3, bytes %4").arg(data.vanswer_msg_id.v).arg(data.vstatus.v).arg(data.vbytes.v));

		bool received = false;
		MTPlong resMsgId = data.vanswer_msg_id;
		{
			QReadLocker lock(sessionData->receivedIdsMutex());
			received = (sessionData->receivedIdsSet().lookup(resMsgId.v) != ReceivedMsgIds::State::NotFound);
		}
		if (received) {
			ackRequestData.push_back(resMsgId);
		} else {
			DEBUG_LOG(("Message Info: answer message %1 was not received, requesting...").arg(resMsgId.v));
			resendRequestData.push_back(resMsgId);
		}
	} return HandleResult::Success;

	case mtpc_msg_resend_req: {
		MTPMsgResendReq msg;
		msg.read(from, end);
		auto &ids = msg.c_msg_resend_req().vmsg_ids.v;

		auto idsCount = ids.size();
		DEBUG_LOG(("Message Info: resend of msgs requested, ids: %1").arg(Logs::vector(ids)));
		if (!idsCount) return (badTime ? HandleResult::Ignored : HandleResult::Success);

		QVector<uint64_t> toResend(ids.size());
		for (int32_t i = 0, l = ids.size(); i < l; ++i) {
			toResend[i] = ids.at(i).v;
		}
		resendMany(toResend, 0, false, true);
	} return HandleResult::Success;

	case mtpc_rpc_result: {
		if (from + 3 > end) throw mtpErrorInsufficient();
		auto response = SerializedMessage();

		MTPlong reqMsgId;
		reqMsgId.read(++from, end);
		mtpTypeId typeId = from[0];

		DEBUG_LOG(("RPC Info: response received for %1, queueing...").arg(reqMsgId.v));

		QVector<MTPlong> ids(1, reqMsgId);
		if (badTime) {
			if (requestsFixTimeSalt(ids, serverTime, serverSalt)) {
				badTime = false;
			} else {
				DEBUG_LOG(("Message Info: error, such message was not sent recently %1").arg(reqMsgId.v));
				return HandleResult::Ignored;
			}
		}
		requestsAcked(ids, true);

		if (typeId == mtpc_gzip_packed) {
			DEBUG_LOG(("RPC Info: gzip container"));
			response = ungzip(++from, end);
			if (!response.size()) {
				return HandleResult::RestartConnection;
			}
			typeId = response[0];
		} else {
			response.resize(end - from);
			memcpy(response.data(), from, (end - from) * sizeof(mtpPrime));
		}
		if (typeId != mtpc_rpc_error) {
			// An error could be some RPC_CALL_FAIL or other error inside
			// the initConnection, so we're not sure yet that it was inited.
			// Wait till a good response is received.
			if (!sessionData->layerWasInited()) {
				sessionData->setLayerWasInited(true);
				sessionData->owner()->notifyLayerInited(true);
			}
		}

		auto requestId = wasSent(reqMsgId.v);
		if (requestId && requestId != mtpRequestId(0xFFFFFFFF)) {
			// Save rpc_result for processing in the main thread.
			QWriteLocker locker(sessionData->haveReceivedMutex());
			sessionData->haveReceivedResponses().insert(requestId, response);
		} else {
			DEBUG_LOG(("RPC Info: requestId not found for msgId %1").arg(reqMsgId.v));
		}
	} return HandleResult::Success;

	case mtpc_new_session_created: {
		const mtpPrime *start = from;
		MTPNewSession msg;
		msg.read(from, end);
		const auto &data(msg.c_new_session_created());

		if (badTime) {
			if (requestsFixTimeSalt(QVector<MTPlong>(1, data.vfirst_msg_id), serverTime, serverSalt)) {
				badTime = false;
			} else {
				DEBUG_LOG(("Message Info: error, such message was not sent recently %1").arg(data.vfirst_msg_id.v));
				return HandleResult::Ignored;
			}
		}

		DEBUG_LOG(("Message Info: new server session created, unique_id %1, first_msg_id %2, server_salt %3").arg(data.vunique_id.v).arg(data.vfirst_msg_id.v).arg(data.vserver_salt.v));
		sessionData->setSalt(data.vserver_salt.v);

		mtpMsgId firstMsgId = data.vfirst_msg_id.v;
		QVector<uint64_t> toResend;
		{
			QReadLocker locker(sessionData->haveSentMutex());
			const mtpRequestMap &haveSent(sessionData->haveSentMap());
			toResend.reserve(haveSent.size());
			for (mtpRequestMap::const_iterator i = haveSent.cbegin(), e = haveSent.cend(); i != e; ++i) {
				if (i.key() >= firstMsgId) break;
				if (i.value()->requestId) toResend.push_back(i.key());
			}
		}
		resendMany(toResend, 10, true);

		mtpBuffer update(from - start);
		if (from > start) memcpy(update.data(), start, (from - start) * sizeof(mtpPrime));

		// Notify main process about new session - need to get difference.
		QWriteLocker locker(sessionData->haveReceivedMutex());
		sessionData->haveReceivedUpdates().push_back(SerializedMessage(update));
	} return HandleResult::Success;

	case mtpc_ping: {
		if (badTime) return HandleResult::Ignored;

		MTPPing msg;
		msg.read(from, end);
		DEBUG_LOG(("Message Info: ping received, ping_id: %1, sending pong...").arg(msg.vping_id.v));

		emit sendPongAsync(msgId, msg.vping_id.v);
	} return HandleResult::Success;

	case mtpc_pong: {
		MTPPong msg;
		msg.read(from, end);
		const auto &data(msg.c_pong());
		DEBUG_LOG(("Message Info: pong received, msg_id: %1, ping_id: %2").arg(data.vmsg_id.v).arg(data.vping_id.v));

		if (!wasSent(data.vmsg_id.v)) {
			DEBUG_LOG(("Message Error: such msg_id %1 ping_id %2 was not sent recently").arg(data.vmsg_id.v).arg(data.vping_id.v));
			return HandleResult::Ignored;
		}
		if (data.vping_id.v == _pingId) {
			_pingId = 0;
		} else {
			DEBUG_LOG(("Message Info: just pong..."));
		}

		QVector<MTPlong> ids(1, data.vmsg_id);
		if (badTime) {
			if (requestsFixTimeSalt(ids, serverTime, serverSalt)) {
				badTime = false;
			} else {
				return HandleResult::Ignored;
			}
		}
		requestsAcked(ids, true);
	} return HandleResult::Success;

	}

	} catch (Exception &) {
		return HandleResult::RestartConnection;
	}

	if (badTime) {
		DEBUG_LOG(("Message Error: bad time in updates cons, must create new session"));
		return HandleResult::ResetSession;
	}

	if (_dcType == DcType::Regular) {
		mtpBuffer update(end - from);
		if (end > from) memcpy(update.data(), from, (end - from) * sizeof(mtpPrime));

		// Notify main process about the new updates.
		QWriteLocker locker(sessionData->haveReceivedMutex());
		sessionData->haveReceivedUpdates().push_back(SerializedMessage(update));

		if (cons != mtpc_updatesTooLong && cons != mtpc_updateShortMessage && cons != mtpc_updateShortChatMessage && cons != mtpc_updateShortSentMessage && cons != mtpc_updateShort && cons != mtpc_updatesCombined && cons != mtpc_updates) {
			LOG(("Message Error: unknown constructor %1").arg(cons)); // maybe new api?..
		}
	} else {
		LOG(("Message Error: unexpected updates in dcType: %1").arg(static_cast<int>(_dcType)));
	}

	return HandleResult::Success;
}

mtpBuffer ConnectionPrivate::ungzip(const mtpPrime *from, const mtpPrime *end) const {
	MTPstring packed;
	packed.read(from, end); // read packed string as serialized mtp string type
	uint32_t packedLen = packed.v.size(), unpackedChunk = packedLen, unpackedLen = 0;

	mtpBuffer result; // * 4 because of mtpPrime type
	result.resize(0);
	z_stream stream;
	stream.zalloc = 0;
	stream.zfree = 0;
	stream.opaque = 0;
	stream.avail_in = 0;
	stream.next_in = 0;
	int res = inflateInit2(&stream, 16 + MAX_WBITS);
	if (res != Z_OK) {
		LOG(("RPC Error: could not init zlib stream, code: %1").arg(res));
		return result;
	}
	stream.avail_in = packedLen;
	stream.next_in = reinterpret_cast<Bytef*>(packed.v.data());

	stream.avail_out = 0;
	while (!stream.avail_out) {
		result.resize(result.size() + unpackedChunk);
		stream.avail_out = unpackedChunk * sizeof(mtpPrime);
		stream.next_out = (Bytef*)&result[result.size() - unpackedChunk];
		int res = inflate(&stream, Z_NO_FLUSH);
		if (res != Z_OK && res != Z_STREAM_END) {
			inflateEnd(&stream);
			LOG(("RPC Error: could not unpack gziped data, code: %1").arg(res));
			DEBUG_LOG(("RPC Error: bad gzip: %1").arg(Logs::mb(packed.v.constData(), packedLen).str()));
			return mtpBuffer();
		}
	}
	if (stream.avail_out & 0x03) {
		uint32_t badSize = result.size() * sizeof(mtpPrime) - stream.avail_out;
		LOG(("RPC Error: bad length of unpacked data %1").arg(badSize));
		DEBUG_LOG(("RPC Error: bad unpacked data %1").arg(Logs::mb(result.data(), badSize).str()));
		return mtpBuffer();
	}
	result.resize(result.size() - (stream.avail_out >> 2));
	inflateEnd(&stream);
	if (!result.size()) {
		LOG(("RPC Error: bad length of unpacked data 0"));
	}
	return result;
}

bool ConnectionPrivate::requestsFixTimeSalt(const QVector<MTPlong> &ids, int32_t serverTime, uint64_t serverSalt) {
	uint32_t idsCount = ids.size();

	for (uint32_t i = 0; i < idsCount; ++i) {
		if (wasSent(ids[i].v)) {// found such msg_id in recent acked requests or in recent sent requests
			if (serverSalt) sessionData->setSalt(serverSalt);
			unixtimeSet(serverTime, true);
			return true;
		}
	}
	return false;
}

void ConnectionPrivate::requestsAcked(const QVector<MTPlong> &ids, bool byResponse) {
	uint32_t idsCount = ids.size();

	DEBUG_LOG(("Message Info: requests acked, ids %1").arg(Logs::vector(ids)));

	RPCCallbackClears clearedAcked;
	QVector<MTPlong> toAckMore;
	{
		QWriteLocker locker1(sessionData->wereAckedMutex());
		mtpRequestIdsMap &wereAcked(sessionData->wereAckedMap());

		{
			QWriteLocker locker2(sessionData->haveSentMutex());
			mtpRequestMap &haveSent(sessionData->haveSentMap());

			for (uint32_t i = 0; i < idsCount; ++i) {
				mtpMsgId msgId = ids[i].v;
				mtpRequestMap::iterator req = haveSent.find(msgId);
				if (req != haveSent.cend()) {
					if (!req.value()->msDate) {
						DEBUG_LOG(("Message Info: container ack received, msgId %1").arg(ids[i].v));
						uint32_t inContCount = ((*req)->size() - 8) / 2;
						const mtpMsgId *inContId = (const mtpMsgId *)(req.value()->constData() + 8);
						toAckMore.reserve(toAckMore.size() + inContCount);
						for (uint32_t j = 0; j < inContCount; ++j) {
							toAckMore.push_back(MTP_long(*(inContId++)));
						}
						haveSent.erase(req);
					} else {
						mtpRequestId reqId = req.value()->requestId;
						bool moveToAcked = byResponse;
						if (!moveToAcked) { // ignore ACK, if we need a response (if we have a handler)
							moveToAcked = !_instance->hasCallbacks(reqId);
						}
						if (moveToAcked) {
							wereAcked.insert(msgId, reqId);
							haveSent.erase(req);
						} else {
							DEBUG_LOG(("Message Info: ignoring ACK for msgId %1 because request %2 requires a response").arg(msgId).arg(reqId));
						}
					}
				} else {
					DEBUG_LOG(("Message Info: msgId %1 was not found in recent sent, while acking requests, searching in resend...").arg(msgId));
					QWriteLocker locker3(sessionData->toResendMutex());
					mtpRequestIdsMap &toResend(sessionData->toResendMap());
					mtpRequestIdsMap::iterator reqIt = toResend.find(msgId);
					if (reqIt != toResend.cend()) {
						mtpRequestId reqId = reqIt.value();
						bool moveToAcked = byResponse;
						if (!moveToAcked) { // ignore ACK, if we need a response (if we have a handler)
							moveToAcked = !_instance->hasCallbacks(reqId);
						}
						if (moveToAcked) {
							QWriteLocker locker4(sessionData->toSendMutex());
							mtpPreRequestMap &toSend(sessionData->toSendMap());
							mtpPreRequestMap::iterator req = toSend.find(reqId);
							if (req != toSend.cend()) {
								wereAcked.insert(msgId, req.value()->requestId);
								if (req.value()->requestId != reqId) {
									DEBUG_LOG(("Message Error: for msgId %1 found resent request, requestId %2, contains requestId %3").arg(msgId).arg(reqId).arg(req.value()->requestId));
								} else {
									DEBUG_LOG(("Message Info: acked msgId %1 that was prepared to resend, requestId %2").arg(msgId).arg(reqId));
								}
								toSend.erase(req);
							} else {
								DEBUG_LOG(("Message Info: msgId %1 was found in recent resent, requestId %2 was not found in prepared to send").arg(msgId));
							}
							toResend.erase(reqIt);
						} else {
							DEBUG_LOG(("Message Info: ignoring ACK for msgId %1 because request %2 requires a response").arg(msgId).arg(reqId));
						}
					} else {
						DEBUG_LOG(("Message Info: msgId %1 was not found in recent resent either").arg(msgId));
					}
				}
			}
		}

		uint32_t ackedCount = wereAcked.size();
		if (ackedCount > MTPIdsBufferSize) {
			DEBUG_LOG(("Message Info: removing some old acked sent msgIds %1").arg(ackedCount - MTPIdsBufferSize));
			clearedAcked.reserve(ackedCount - MTPIdsBufferSize);
			while (ackedCount-- > MTPIdsBufferSize) {
				mtpRequestIdsMap::iterator i(wereAcked.begin());
				clearedAcked.push_back(RPCCallbackClear(i.key(), RPCError::TimeoutError));
				wereAcked.erase(i);
			}
		}
	}

	if (clearedAcked.size()) {
		_instance->clearCallbacksDelayed(clearedAcked);
	}

	if (toAckMore.size()) {
		requestsAcked(toAckMore);
	}
}

void ConnectionPrivate::handleMsgsStates(const QVector<MTPlong> &ids, const QByteArray &states, QVector<MTPlong> &acked) {
	uint32_t idsCount = ids.size();
	if (!idsCount) {
		DEBUG_LOG(("Message Info: void ids vector in handleMsgsStates()"));
		return;
	}
	if (states.size() < idsCount) {
		LOG(("Message Error: got less states than required ids count."));
		return;
	}

	acked.reserve(acked.size() + idsCount);
	for (uint32_t i = 0, count = idsCount; i < count; ++i) {
		char state = states[i];
		uint64_t requestMsgId = ids[i].v;
		{
			QReadLocker locker(sessionData->haveSentMutex());
			const mtpRequestMap &haveSent(sessionData->haveSentMap());
			mtpRequestMap::const_iterator haveSentEnd = haveSent.cend();
			if (haveSent.find(requestMsgId) == haveSentEnd) {
				DEBUG_LOG(("Message Info: state was received for msgId %1, but request is not found, looking in resent requests...").arg(requestMsgId));
				QWriteLocker locker2(sessionData->toResendMutex());
				mtpRequestIdsMap &toResend(sessionData->toResendMap());
				mtpRequestIdsMap::iterator reqIt = toResend.find(requestMsgId);
				if (reqIt != toResend.cend()) {
					if ((state & 0x07) != 0x04) { // was received
						DEBUG_LOG(("Message Info: state was received for msgId %1, state %2, already resending in container").arg(requestMsgId).arg((int32_t)state));
					} else {
						DEBUG_LOG(("Message Info: state was received for msgId %1, state %2, ack, cancelling resend").arg(requestMsgId).arg((int32_t)state));
						acked.push_back(MTP_long(requestMsgId)); // will remove from resend in requestsAcked
					}
				} else {
					DEBUG_LOG(("Message Info: msgId %1 was not found in recent resent either").arg(requestMsgId));
				}
				continue;
			}
		}
		if ((state & 0x07) != 0x04) { // was received
			DEBUG_LOG(("Message Info: state was received for msgId %1, state %2, resending in container").arg(requestMsgId).arg((int32_t)state));
			resend(requestMsgId, 10, true);
		} else {
			DEBUG_LOG(("Message Info: state was received for msgId %1, state %2, ack").arg(requestMsgId).arg((int32_t)state));
			acked.push_back(MTP_long(requestMsgId));
		}
	}
}

void ConnectionPrivate::resend(uint64_t msgId, int64_t msCanWait, bool forceContainer, bool sendMsgStateInfo) {
	if (msgId == _pingMsgId) return;
	emit resendAsync(msgId, msCanWait, forceContainer, sendMsgStateInfo);
}

void ConnectionPrivate::resendMany(QVector<uint64_t> msgIds, int64_t msCanWait, bool forceContainer, bool sendMsgStateInfo) {
	for (int32_t i = 0, l = msgIds.size(); i < l; ++i) {
		if (msgIds.at(i) == _pingMsgId) {
			msgIds.remove(i);
			--l;
		}
	}
	emit resendManyAsync(msgIds, msCanWait, forceContainer, sendMsgStateInfo);
}

void ConnectionPrivate::onConnected4() {
	_waitForConnected = MTPMinConnectDelay;
	_waitForConnectedTimer.stop();

	_waitForIPv4Timer.stop();

	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData) return;

	disconnect(_conn4, SIGNAL(connected()), this, SLOT(onConnected4()));
	if (!_conn4->isConnected()) {
		LOG(("Connection Error: not connected in onConnected4(), state: %1").arg(_conn4->debugState()));

		lockFinished.unlock();
		return restart();
	}

	_conn = _conn4;
	destroyConn(&_conn6);

	DEBUG_LOG(("MTP Info: connection through IPv4 succeed."));

	lockFinished.unlock();
	updateAuthKey();
}

void ConnectionPrivate::onConnected6() {
	_waitForConnected = MTPMinConnectDelay;
	_waitForConnectedTimer.stop();

	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData) return;

	disconnect(_conn6, SIGNAL(connected()), this, SLOT(onConnected6()));
	if (!_conn6->isConnected()) {
		LOG(("Connection Error: not connected in onConnected(), state: %1").arg(_conn6->debugState()));

		lockFinished.unlock();
		return restart();
	}

	DEBUG_LOG(("MTP Info: connection through IPv6 succeed, waiting IPv4 for %1ms.").arg(MTPIPv4ConnectionWaitTimeout));

	_waitForIPv4Timer.start(MTPIPv4ConnectionWaitTimeout);
}

void ConnectionPrivate::onDisconnected4() {
	if (_conn && _conn == _conn6) return; // disconnected the unused

	if (_conn || !_conn6) {
		destroyConn();
		restart();
	} else {
		destroyConn(&_conn4);
	}
}

void ConnectionPrivate::onDisconnected6() {
	if (_conn && _conn == _conn4) return; // disconnected the unused

	if (_conn || !_conn4) {
		destroyConn();
		restart();
	} else {
		destroyConn(&_conn6);
	}
}

void ConnectionPrivate::updateAuthKey() 	{
	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData || !_conn) return;

	DEBUG_LOG(("AuthKey Info: Connection updating key from Session, dc %1").arg(_shiftedDcId));
	uint64_t newKeyId = 0;
	{
		ReadLockerAttempt lock(sessionData->keyMutex());
		if (!lock) {
			DEBUG_LOG(("MTP Info: could not lock auth_key for read, waiting signal emit"));
			clearMessages();
			keyId = newKeyId;
			return; // some other connection is getting key
		}
		auto key = sessionData->getKey();
		newKeyId = key ? key->keyId() : 0;
	}
	if (keyId != newKeyId) {
		clearMessages();
		keyId = newKeyId;
	}
	DEBUG_LOG(("AuthKey Info: Connection update key from Session, dc %1 result: %2").arg(_shiftedDcId).arg(Logs::mb(&keyId, sizeof(keyId)).str()));
	if (keyId) {
		return authKeyCreated();
	}

	DEBUG_LOG(("AuthKey Info: No key in updateAuthKey(), will be creating auth_key"));
	lockKey();

	auto &key = sessionData->getKey();
	if (key) {
		if (keyId != key->keyId()) clearMessages();
		keyId = key->keyId();
		unlockKey();
		return authKeyCreated();
	} else if (_instance->isKeysDestroyer()) {
		// We are here to destroy an old key, so we're done.
		LOG(("MTP Error: No key %1 in updateAuthKey() for destroying.").arg(_shiftedDcId));
		emit _instance->keyDestroyed(_shiftedDcId);
		return;
	}

	_authKeyData = std::make_unique<ConnectionPrivate::AuthKeyCreateData>();
	_authKeyStrings = std::make_unique<ConnectionPrivate::AuthKeyCreateStrings>();
	_authKeyData->req_num = 0;
	_authKeyData->nonce = rand_value<MTPint128>();

	MTPReq_pq req_pq;
	req_pq.vnonce = _authKeyData->nonce;

	connect(_conn, SIGNAL(receivedData()), this, SLOT(pqAnswered()));

	DEBUG_LOG(("AuthKey Info: sending Req_pq..."));
	lockFinished.unlock();
	sendRequestNotSecure(req_pq);
}

void ConnectionPrivate::clearMessages() {
	if (keyId && keyId != kRecreateKeyId && _conn) {
		_conn->received().clear();
	}
}

void ConnectionPrivate::pqAnswered() {
	disconnect(_conn, SIGNAL(receivedData()), this, SLOT(pqAnswered()));
	DEBUG_LOG(("AuthKey Info: receiving Req_pq answer..."));

	MTPReq_pq::ResponseType res_pq;
	if (!readResponseNotSecure(res_pq)) {
		return restart();
	}

	auto &res_pq_data = res_pq.c_resPQ();
	if (res_pq_data.vnonce != _authKeyData->nonce) {
		LOG(("AuthKey Error: received nonce <> sent nonce (in res_pq)!"));
		DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&res_pq_data.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));
		return restart();
	}

	auto rsaKey = internal::RSAPublicKey();
	if (!_instance->dcOptions()->getDcRSAKey(bareDcId(_shiftedDcId), res_pq.c_resPQ().vserver_public_key_fingerprints.v, &rsaKey)) {
		if (_dcType == DcType::Cdn) {
			LOG(("Warning: CDN public RSA key not found"));
			requestCDNConfig();
			return;
		}
		LOG(("AuthKey Error: could not choose public RSA key"));
		return restart();
	}
	Assert(rsaKey.isValid());

	_authKeyData->server_nonce = res_pq_data.vserver_nonce;
	_authKeyData->new_nonce = rand_value<MTPint256>();

	auto &pq = res_pq_data.vpq.v;
	auto p = QByteArray();
	auto q = QByteArray();
	if (!MTP::internal::parsePQ(pq, p, q)) {
		LOG(("AuthKey Error: could not factor pq!"));
		DEBUG_LOG(("AuthKey Error: problematic pq: %1").arg(Logs::mb(pq.constData(), pq.length()).str()));
		return restart();
	}

	auto p_q_inner = MTP_p_q_inner_data(res_pq_data.vpq, MTP_bytes(std::move(p)), MTP_bytes(std::move(q)), _authKeyData->nonce, _authKeyData->server_nonce, _authKeyData->new_nonce);
	auto dhEncString = encryptPQInnerRSA(p_q_inner, rsaKey);
	if (dhEncString.empty()) {
		return restart();
	}

	connect(_conn, SIGNAL(receivedData()), this, SLOT(dhParamsAnswered()));

	DEBUG_LOG(("AuthKey Info: sending Req_DH_params..."));

	MTPReq_DH_params req_DH_params;
	req_DH_params.vnonce = _authKeyData->nonce;
	req_DH_params.vserver_nonce = _authKeyData->server_nonce;
	req_DH_params.vpublic_key_fingerprint = MTP_long(rsaKey.getFingerPrint());
	req_DH_params.vp = p_q_inner.c_p_q_inner_data().vp;
	req_DH_params.vq = p_q_inner.c_p_q_inner_data().vq;
	req_DH_params.vencrypted_data = MTP_bytes(dhEncString);
	sendRequestNotSecure(req_DH_params);
}

base::byte_vector ConnectionPrivate::encryptPQInnerRSA(const MTPP_Q_inner_data &data, const MTP::internal::RSAPublicKey &key) {
	auto p_q_inner_size = data.innerLength();
	auto encSize = (p_q_inner_size >> 2) + 6;
	if (encSize >= 65) {
		auto tmp = mtpBuffer();
		tmp.reserve(encSize);
		data.write(tmp);
		LOG(("AuthKey Error: too large data for RSA encrypt, size %1").arg(encSize * sizeof(mtpPrime)));
		DEBUG_LOG(("AuthKey Error: bad data for RSA encrypt %1").arg(Logs::mb(&tmp[0], tmp.size() * 4).str()));
		return base::byte_vector(); // can't be 255-byte string
	}

	auto encBuffer = mtpBuffer();
	encBuffer.reserve(65); // 260 bytes
	encBuffer.resize(6);
	encBuffer[0] = 0;
	data.write(encBuffer);

	hashSha1(&encBuffer[6], p_q_inner_size, &encBuffer[1]);
	if (encSize < 65) {
		encBuffer.resize(65);
		memset_rand(&encBuffer[encSize], (65 - encSize) * sizeof(mtpPrime));
	}

	auto bytes = gsl::as_bytes(gsl::make_span(encBuffer));
	auto bytesToEncrypt = bytes.subspan(3, 256);
	return key.encrypt(bytesToEncrypt);
}

void ConnectionPrivate::dhParamsAnswered() {
	disconnect(_conn, SIGNAL(receivedData()), this, SLOT(dhParamsAnswered()));
	DEBUG_LOG(("AuthKey Info: receiving Req_DH_params answer..."));

	MTPReq_DH_params::ResponseType res_DH_params;
	if (!readResponseNotSecure(res_DH_params)) {
		return restart();
	}

	switch (res_DH_params.type()) {
	case mtpc_server_DH_params_ok: {
		const auto &encDH(res_DH_params.c_server_DH_params_ok());
		if (encDH.vnonce != _authKeyData->nonce) {
			LOG(("AuthKey Error: received nonce <> sent nonce (in server_DH_params_ok)!"));
			DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&encDH.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));
			return restart();
		}
		if (encDH.vserver_nonce != _authKeyData->server_nonce) {
			LOG(("AuthKey Error: received server_nonce <> sent server_nonce (in server_DH_params_ok)!"));
			DEBUG_LOG(("AuthKey Error: received server_nonce: %1, sent server_nonce: %2").arg(Logs::mb(&encDH.vserver_nonce, 16).str()).arg(Logs::mb(&_authKeyData->server_nonce, 16).str()));
			return restart();
		}

		auto &encDHStr = encDH.vencrypted_answer.v;
		uint32_t encDHLen = encDHStr.length(), encDHBufLen = encDHLen >> 2;
		if ((encDHLen & 0x03) || encDHBufLen < 6) {
			LOG(("AuthKey Error: bad encrypted data length %1 (in server_DH_params_ok)!").arg(encDHLen));
			DEBUG_LOG(("AuthKey Error: received encrypted data %1").arg(Logs::mb(encDHStr.constData(), encDHLen).str()));
			return restart();
		}

		uint32_t nlen = _authKeyData->new_nonce.innerLength(), slen = _authKeyData->server_nonce.innerLength();
		uchar tmp_aes[1024], sha1ns[20], sha1sn[20], sha1nn[20];
		memcpy(tmp_aes, &_authKeyData->new_nonce, nlen);
		memcpy(tmp_aes + nlen, &_authKeyData->server_nonce, slen);
		memcpy(tmp_aes + nlen + slen, &_authKeyData->new_nonce, nlen);
		memcpy(tmp_aes + nlen + slen + nlen, &_authKeyData->new_nonce, nlen);
		hashSha1(tmp_aes, nlen + slen, sha1ns);
		hashSha1(tmp_aes + nlen, nlen + slen, sha1sn);
		hashSha1(tmp_aes + nlen + slen, nlen + nlen, sha1nn);

		mtpBuffer decBuffer;
		decBuffer.resize(encDHBufLen);

		memcpy(_authKeyData->aesKey, sha1ns, 20);
		memcpy(_authKeyData->aesKey + 20, sha1sn, 12);
		memcpy(_authKeyData->aesIV, sha1sn + 12, 8);
		memcpy(_authKeyData->aesIV + 8, sha1nn, 20);
		memcpy(_authKeyData->aesIV + 28, &_authKeyData->new_nonce, 4);

		aesIgeDecryptRaw(encDHStr.constData(), &decBuffer[0], encDHLen, _authKeyData->aesKey, _authKeyData->aesIV);

		const mtpPrime *from(&decBuffer[5]), *to(from), *end(from + (encDHBufLen - 5));
		MTPServer_DH_inner_data dh_inner;
		dh_inner.read(to, end);
		const auto &dh_inner_data(dh_inner.c_server_DH_inner_data());
		if (dh_inner_data.vnonce != _authKeyData->nonce) {
			LOG(("AuthKey Error: received nonce <> sent nonce (in server_DH_inner_data)!"));
			DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&dh_inner_data.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));
			return restart();
		}
		if (dh_inner_data.vserver_nonce != _authKeyData->server_nonce) {
			LOG(("AuthKey Error: received server_nonce <> sent server_nonce (in server_DH_inner_data)!"));
			DEBUG_LOG(("AuthKey Error: received server_nonce: %1, sent server_nonce: %2").arg(Logs::mb(&dh_inner_data.vserver_nonce, 16).str()).arg(Logs::mb(&_authKeyData->server_nonce, 16).str()));
			return restart();
		}
		uchar sha1Buffer[20];
		if (memcmp(&decBuffer[0], hashSha1(&decBuffer[5], (to - from) * sizeof(mtpPrime), sha1Buffer), 20)) {
			LOG(("AuthKey Error: sha1 hash of encrypted part did not match!"));
			DEBUG_LOG(("AuthKey Error: sha1 did not match, server_nonce: %1, new_nonce %2, encrypted data %3").arg(Logs::mb(&_authKeyData->server_nonce, 16).str()).arg(Logs::mb(&_authKeyData->new_nonce, 16).str()).arg(Logs::mb(encDHStr.constData(), encDHLen).str()));
			return restart();
		}
		unixtimeSet(dh_inner_data.vserver_time.v);

		// check that dhPrime and (dhPrime - 1) / 2 are really prime
		if (!IsPrimeAndGood(bytesFromMTP(dh_inner_data.vdh_prime), dh_inner_data.vg.v)) {
			LOG(("AuthKey Error: bad dh_prime primality!"));
			return restart();
		}

		_authKeyStrings->dh_prime = byteVectorFromMTP(dh_inner_data.vdh_prime);
		_authKeyData->g = dh_inner_data.vg.v;
		_authKeyStrings->g_a = byteVectorFromMTP(dh_inner_data.vg_a);
		_authKeyData->retry_id = MTP_long(0);
		_authKeyData->retries = 0;
	} return dhClientParamsSend();

	case mtpc_server_DH_params_fail: {
		const auto &encDH(res_DH_params.c_server_DH_params_fail());
		if (encDH.vnonce != _authKeyData->nonce) {
			LOG(("AuthKey Error: received nonce <> sent nonce (in server_DH_params_fail)!"));
			DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&encDH.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));
			return restart();
		}
		if (encDH.vserver_nonce != _authKeyData->server_nonce) {
			LOG(("AuthKey Error: received server_nonce <> sent server_nonce (in server_DH_params_fail)!"));
			DEBUG_LOG(("AuthKey Error: received server_nonce: %1, sent server_nonce: %2").arg(Logs::mb(&encDH.vserver_nonce, 16).str()).arg(Logs::mb(&_authKeyData->server_nonce, 16).str()));
			return restart();
		}
		uchar sha1Buffer[20];
		if (encDH.vnew_nonce_hash != *(MTPint128*)(hashSha1(&_authKeyData->new_nonce, 32, sha1Buffer) + 1)) {
			LOG(("AuthKey Error: received new_nonce_hash did not match!"));
			DEBUG_LOG(("AuthKey Error: received new_nonce_hash: %1, new_nonce: %2").arg(Logs::mb(&encDH.vnew_nonce_hash, 16).str()).arg(Logs::mb(&_authKeyData->new_nonce, 32).str()));
			return restart();
		}
		LOG(("AuthKey Error: server_DH_params_fail received!"));
	} return restart();

	}
	LOG(("AuthKey Error: unknown server_DH_params received, typeId = %1").arg(res_DH_params.type()));
	return restart();
}

void ConnectionPrivate::dhClientParamsSend() {
	if (++_authKeyData->retries > 5) {
		LOG(("AuthKey Error: could not create auth_key for %1 retries").arg(_authKeyData->retries - 1));
		return restart();
	}

	// gen rand 'b'
	auto randomSeed = std::array<gsl::byte, ModExpFirst::kRandomPowerSize>();
	openssl::FillRandom(randomSeed);
	auto g_b_data = CreateModExp(_authKeyData->g, _authKeyStrings->dh_prime, randomSeed);
	if (g_b_data.modexp.empty()) {
		LOG(("AuthKey Error: could not generate good g_b."));
		return restart();
	}

	auto computedAuthKey = CreateAuthKey(_authKeyStrings->g_a, g_b_data.randomPower, _authKeyStrings->dh_prime);
	if (computedAuthKey.empty()) {
		LOG(("AuthKey Error: could not generate auth_key."));
		return restart();
	}
	AuthKey::FillData(_authKeyStrings->auth_key, computedAuthKey);

	// count auth_key hashes - parts of sha1(auth_key)
	auto auth_key_sha = hashSha1(_authKeyStrings->auth_key.data(), _authKeyStrings->auth_key.size());
	memcpy(&_authKeyData->auth_key_aux_hash, auth_key_sha.data(), 8);
	memcpy(&_authKeyData->auth_key_hash, auth_key_sha.data() + 12, 8);

	auto client_dh_inner = MTP_client_DH_inner_data(_authKeyData->nonce, _authKeyData->server_nonce, _authKeyData->retry_id, MTP_bytes(g_b_data.modexp));

	auto sdhEncString = encryptClientDHInner(client_dh_inner);

	connect(_conn, SIGNAL(receivedData()), this, SLOT(dhClientParamsAnswered()));

	MTPSet_client_DH_params req_client_DH_params;
	req_client_DH_params.vnonce = _authKeyData->nonce;
	req_client_DH_params.vserver_nonce = _authKeyData->server_nonce;
	req_client_DH_params.vencrypted_data = MTP_string(std::move(sdhEncString));

	DEBUG_LOG(("AuthKey Info: sending Req_client_DH_params..."));
	sendRequestNotSecure(req_client_DH_params);
}

std::string ConnectionPrivate::encryptClientDHInner(const MTPClient_DH_Inner_Data &data) {
	auto client_dh_inner_size = data.innerLength();
	auto encSize = (client_dh_inner_size >> 2) + 5;
	auto encFullSize = encSize;
	if (encSize & 0x03) {
		encFullSize += 4 - (encSize & 0x03);
	}

	auto encBuffer = mtpBuffer();
	encBuffer.reserve(encFullSize);
	encBuffer.resize(5);
	data.write(encBuffer);

	hashSha1(&encBuffer[5], client_dh_inner_size, &encBuffer[0]);
	if (encSize < encFullSize) {
		encBuffer.resize(encFullSize);
		memset_rand(&encBuffer[encSize], (encFullSize - encSize) * sizeof(mtpPrime));
	}

	auto sdhEncString = std::string(encFullSize * 4, ' ');

	aesIgeEncryptRaw(&encBuffer[0], &sdhEncString[0], encFullSize * sizeof(mtpPrime), _authKeyData->aesKey, _authKeyData->aesIV);

	return sdhEncString;
}

void ConnectionPrivate::dhClientParamsAnswered() {
	QReadLocker lockFinished(&sessionDataMutex);
	if (!sessionData) return;

	disconnect(_conn, SIGNAL(receivedData()), this, SLOT(dhClientParamsAnswered()));
	DEBUG_LOG(("AuthKey Info: receiving Req_client_DH_params answer..."));

	MTPSet_client_DH_params::ResponseType res_client_DH_params;
	if (!readResponseNotSecure(res_client_DH_params)) {
		lockFinished.unlock();
		return restart();
	}

	switch (res_client_DH_params.type()) {
	case mtpc_dh_gen_ok: {
		const auto &resDH(res_client_DH_params.c_dh_gen_ok());
		if (resDH.vnonce != _authKeyData->nonce) {
			LOG(("AuthKey Error: received nonce <> sent nonce (in dh_gen_ok)!"));
			DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&resDH.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));

			lockFinished.unlock();
			return restart();
		}
		if (resDH.vserver_nonce != _authKeyData->server_nonce) {
			LOG(("AuthKey Error: received server_nonce <> sent server_nonce (in dh_gen_ok)!"));
			DEBUG_LOG(("AuthKey Error: received server_nonce: %1, sent server_nonce: %2").arg(Logs::mb(&resDH.vserver_nonce, 16).str()).arg(Logs::mb(&_authKeyData->server_nonce, 16).str()));

			lockFinished.unlock();
			return restart();
		}
		_authKeyData->new_nonce_buf[32] = 1;
		uchar sha1Buffer[20];
		if (resDH.vnew_nonce_hash1 != *(MTPint128*)(hashSha1(_authKeyData->new_nonce_buf, 41, sha1Buffer) + 1)) {
			LOG(("AuthKey Error: received new_nonce_hash1 did not match!"));
			DEBUG_LOG(("AuthKey Error: received new_nonce_hash1: %1, new_nonce_buf: %2").arg(Logs::mb(&resDH.vnew_nonce_hash1, 16).str()).arg(Logs::mb(_authKeyData->new_nonce_buf, 41).str()));

			lockFinished.unlock();
			return restart();
		}

		uint64_t salt1 = _authKeyData->new_nonce.l.l, salt2 = _authKeyData->server_nonce.l, serverSalt = salt1 ^ salt2;
		sessionData->setSalt(serverSalt);

		auto authKey = std::make_shared<AuthKey>(AuthKey::Type::Generated, bareDcId(_shiftedDcId), _authKeyStrings->auth_key);

		DEBUG_LOG(("AuthKey Info: auth key gen succeed, id: %1, server salt: %2").arg(authKey->keyId()).arg(serverSalt));

		sessionData->owner()->notifyKeyCreated(std::move(authKey)); // slot will call authKeyCreated()
		sessionData->clear(_instance);
		unlockKey();
	} return;

	case mtpc_dh_gen_retry: {
		const auto &resDH(res_client_DH_params.c_dh_gen_retry());
		if (resDH.vnonce != _authKeyData->nonce) {
			LOG(("AuthKey Error: received nonce <> sent nonce (in dh_gen_retry)!"));
			DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&resDH.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));

			lockFinished.unlock();
			return restart();
		}
		if (resDH.vserver_nonce != _authKeyData->server_nonce) {
			LOG(("AuthKey Error: received server_nonce <> sent server_nonce (in dh_gen_retry)!"));
			DEBUG_LOG(("AuthKey Error: received server_nonce: %1, sent server_nonce: %2").arg(Logs::mb(&resDH.vserver_nonce, 16).str()).arg(Logs::mb(&_authKeyData->server_nonce, 16).str()));

			lockFinished.unlock();
			return restart();
		}
		_authKeyData->new_nonce_buf[32] = 2;
		uchar sha1Buffer[20];
		if (resDH.vnew_nonce_hash2 != *(MTPint128*)(hashSha1(_authKeyData->new_nonce_buf, 41, sha1Buffer) + 1)) {
			LOG(("AuthKey Error: received new_nonce_hash2 did not match!"));
			DEBUG_LOG(("AuthKey Error: received new_nonce_hash2: %1, new_nonce_buf: %2").arg(Logs::mb(&resDH.vnew_nonce_hash2, 16).str()).arg(Logs::mb(_authKeyData->new_nonce_buf, 41).str()));

			lockFinished.unlock();
			return restart();
		}
		_authKeyData->retry_id = _authKeyData->auth_key_aux_hash;
	} return dhClientParamsSend();

	case mtpc_dh_gen_fail: {
		const auto &resDH(res_client_DH_params.c_dh_gen_fail());
		if (resDH.vnonce != _authKeyData->nonce) {
			LOG(("AuthKey Error: received nonce <> sent nonce (in dh_gen_fail)!"));
			DEBUG_LOG(("AuthKey Error: received nonce: %1, sent nonce: %2").arg(Logs::mb(&resDH.vnonce, 16).str()).arg(Logs::mb(&_authKeyData->nonce, 16).str()));

			lockFinished.unlock();
			return restart();
		}
		if (resDH.vserver_nonce != _authKeyData->server_nonce) {
			LOG(("AuthKey Error: received server_nonce <> sent server_nonce (in dh_gen_fail)!"));
			DEBUG_LOG(("AuthKey Error: received server_nonce: %1, sent server_nonce: %2").arg(Logs::mb(&resDH.vserver_nonce, 16).str()).arg(Logs::mb(&_authKeyData->server_nonce, 16).str()));

			lockFinished.unlock();
			return restart();
		}
		_authKeyData->new_nonce_buf[32] = 3;
		uchar sha1Buffer[20];
		if (resDH.vnew_nonce_hash3 != *(MTPint128*)(hashSha1(_authKeyData->new_nonce_buf, 41, sha1Buffer) + 1)) {
			LOG(("AuthKey Error: received new_nonce_hash3 did not match!"));
			DEBUG_LOG(("AuthKey Error: received new_nonce_hash3: %1, new_nonce_buf: %2").arg(Logs::mb(&resDH.vnew_nonce_hash3, 16).str()).arg(Logs::mb(_authKeyData->new_nonce_buf, 41).str()));

			lockFinished.unlock();
			return restart();
		}
		LOG(("AuthKey Error: dh_gen_fail received!"));
	}

		lockFinished.unlock();
		return restart();

	}
	LOG(("AuthKey Error: unknown set_client_DH_params_answer received, typeId = %1").arg(res_client_DH_params.type()));

	lockFinished.unlock();
	return restart();
}

void ConnectionPrivate::authKeyCreated() {
	clearAuthKeyData();

	connect(_conn, SIGNAL(receivedData()), this, SLOT(handleReceived()));

	if (sessionData->getSalt()) { // else receive salt in bad_server_salt first, then try to send all the requests
		setState(ConnectedState);
		if (restarted) {
			emit resendAllAsync();
			restarted = false;
		}
	}

	_pingIdToSend = rand_value<uint64_t>(); // get server_salt

	emit needToSendAsync();
}

void ConnectionPrivate::clearAuthKeyData() {
	auto zeroMemory = [](base::byte_span bytes) {
#ifdef Q_OS_WIN2
		SecureZeroMemory(bytes.data(), bytes.size());
#else // Q_OS_WIN
		auto end = reinterpret_cast<char*>(bytes.data()) + bytes.size();
		for (volatile auto p = reinterpret_cast<volatile char*>(bytes.data()); p != end; ++p) {
			*p = 0;
		}
#endif // Q_OS_WIN
	};
	if (_authKeyData) {
		zeroMemory(gsl::make_span(reinterpret_cast<gsl::byte*>(_authKeyData.get()), sizeof(AuthKeyCreateData)));
		_authKeyData.reset();
	}
	if (_authKeyStrings) {
		if (!_authKeyStrings->dh_prime.empty()) {
			zeroMemory(_authKeyStrings->dh_prime);
		}
		if (!_authKeyStrings->g_a.empty()) {
			zeroMemory(_authKeyStrings->g_a);
		}
		zeroMemory(_authKeyStrings->auth_key);
		_authKeyStrings.reset();
	}
}

void ConnectionPrivate::onError4(int32_t errorCode) {
	if (_conn && _conn == _conn6) return; // error in the unused

	if (errorCode == -429) {
		LOG(("Protocol Error: -429 flood code returned!"));
	}
	if (_conn || !_conn6) {
		handleError(errorCode);
	} else {
		destroyConn(&_conn4);
	}
}

void ConnectionPrivate::onError6(int32_t errorCode) {
	if (_conn && _conn == _conn4) return; // error in the unused

	if (errorCode == -429) {
		LOG(("Protocol Error: -429 flood code returned!"));
	}
	if (_conn || !_conn4) {
		handleError(errorCode);
	} else {
		destroyConn(&_conn6);
	}
}

void ConnectionPrivate::handleError(int errorCode) {
	destroyConn();
	_waitForConnectedTimer.stop();

	if (errorCode == -404) {
		if (_instance->isKeysDestroyer()) {
			LOG(("MTP Info: -404 error received on destroying key %1, assuming it is destroyed.").arg(_shiftedDcId));
			emit _instance->keyDestroyed(_shiftedDcId);
			return;
		} else if (_dcType == DcType::Cdn) {
			LOG(("MTP Info: -404 error received in CDN dc %1, assuming it was destroyed, recreating.").arg(_shiftedDcId));
			clearMessages();
			keyId = kRecreateKeyId;
			return restart();
		}
	}
	MTP_LOG(_shiftedDcId, ("Restarting after error in connection, error code: %1...").arg(errorCode));
	return restart();
}

void ConnectionPrivate::onReadyData() {
}

template <typename TRequest>
void ConnectionPrivate::sendRequestNotSecure(const TRequest &request) {
	try {
		mtpBuffer buffer;
		uint32_t requestSize = request.innerLength() >> 2;

		buffer.resize(0);
		buffer.reserve(8 + requestSize);
		buffer.push_back(0); // tcp packet len
		buffer.push_back(0); // tcp packet num
		buffer.push_back(0);
		buffer.push_back(0);
		buffer.push_back(_authKeyData->req_num);
		buffer.push_back(unixtime());
		buffer.push_back(requestSize * 4);
		request.write(buffer);
		buffer.push_back(0); // tcp crc32 hash
		++_authKeyData->msgs_sent;

		DEBUG_LOG(("AuthKey Info: sending request, size: %1, num: %2, time: %3").arg(requestSize).arg(_authKeyData->req_num).arg(buffer[5]));

		_conn->sendData(buffer);

		onSentSome(buffer.size() * sizeof(mtpPrime));

	} catch (Exception &) {
		return restart();
	}
}

template <typename TResponse>
bool ConnectionPrivate::readResponseNotSecure(TResponse &response) {
	onReceivedSome();

	try {
		if (_conn->received().empty()) {
			LOG(("AuthKey Error: trying to read response from empty received list"));
			return false;
		}

		auto buffer = std::move(_conn->received().front());
		_conn->received().pop_front();

		auto answer = buffer.constData();
		auto len = buffer.size();
		if (len < 5) {
			LOG(("AuthKey Error: bad request answer, len = %1").arg(len * sizeof(mtpPrime)));
			DEBUG_LOG(("AuthKey Error: answer bytes %1").arg(Logs::mb(answer, len * sizeof(mtpPrime)).str()));
			return false;
		}
		if (answer[0] != 0 || answer[1] != 0 || (((uint32_t)answer[2]) & 0x03) != 1/* || (unixtime() - answer[3] > 300) || (answer[3] - unixtime() > 60)*/) { // didnt sync time yet
			LOG(("AuthKey Error: bad request answer start (%1 %2 %3)").arg(answer[0]).arg(answer[1]).arg(answer[2]));
			DEBUG_LOG(("AuthKey Error: answer bytes %1").arg(Logs::mb(answer, len * sizeof(mtpPrime)).str()));
			return false;
		}
		uint32_t answerLen = (uint32_t)answer[4];
		if (answerLen != (len - 5) * sizeof(mtpPrime)) {
			LOG(("AuthKey Error: bad request answer %1 <> %2").arg(answerLen).arg((len - 5) * sizeof(mtpPrime)));
			DEBUG_LOG(("AuthKey Error: answer bytes %1").arg(Logs::mb(answer, len * sizeof(mtpPrime)).str()));
			return false;
		}
		const mtpPrime *from(answer + 5), *end(from + len - 5);
		response.read(from, end);
	} catch (Exception &) {
		return false;
	}
	return true;
}

bool ConnectionPrivate::sendRequest(mtpRequest &request, bool needAnyResponse, QReadLocker &lockFinished) {
	uint32_t fullSize = request->size();
	if (fullSize < 9) return false;

	auto messageSize = mtpRequestData::messageSize(request);
	if (messageSize < 5 || fullSize < messageSize + 4) return false;

	auto lock = ReadLockerAttempt(sessionData->keyMutex());
	if (!lock) {
		DEBUG_LOG(("MTP Info: could not lock key for read in sendBuffer(), dc %1, restarting...").arg(_shiftedDcId));

		lockFinished.unlock();
		restart();
		return false;
	}

	auto key = sessionData->getKey();
	if (!key || key->keyId() != keyId) {
		DEBUG_LOG(("MTP Error: auth_key id for dc %1 changed").arg(_shiftedDcId));

		lockFinished.unlock();
		restart();
		return false;
	}

	auto session = sessionData->getSession();
	auto salt = sessionData->getSalt();

	memcpy(request->data() + 0, &salt, 2 * sizeof(mtpPrime));
	memcpy(request->data() + 2, &session, 2 * sizeof(mtpPrime));

	auto from = request->constData() + 4;
	MTP_LOG(_shiftedDcId, ("Send: ") + mtpTextSerialize(from, from + messageSize));

#ifdef TDESKTOP_MTPROTO_OLD
	uint32_t padding = fullSize - 4 - messageSize;

	uchar encryptedSHA[20];
	MTPint128 &msgKey(*(MTPint128*)(encryptedSHA + 4));
	hashSha1(request->constData(), (fullSize - padding) * sizeof(mtpPrime), encryptedSHA);

	mtpBuffer result;
	result.resize(9 + fullSize);
	*((uint64_t*)&result[2]) = keyId;
	*((MTPint128*)&result[4]) = msgKey;

	aesIgeEncrypt_oldmtp(request->constData(), &result[8], fullSize * sizeof(mtpPrime), key, msgKey);
#else // TDESKTOP_MTPROTO_OLD
	uchar encryptedSHA256[32];
	MTPint128 &msgKey(*(MTPint128*)(encryptedSHA256 + 8));

	SHA256_CTX msgKeyLargeContext;
	SHA256_Init(&msgKeyLargeContext);
	SHA256_Update(&msgKeyLargeContext, key->partForMsgKey(true), 32);
	SHA256_Update(&msgKeyLargeContext, request->constData(), fullSize * sizeof(mtpPrime));
	SHA256_Final(encryptedSHA256, &msgKeyLargeContext);

	mtpBuffer result;
	result.resize(9 + fullSize);
	*((uint64_t*)&result[2]) = keyId;
	*((MTPint128*)&result[4]) = msgKey;

	aesIgeEncrypt(request->constData(), &result[8], fullSize * sizeof(mtpPrime), key, msgKey);
#endif // TDESKTOP_MTPROTO_OLD

	DEBUG_LOG(("MTP Info: sending request, size: %1, num: %2, time: %3").arg(fullSize + 6).arg((*request)[4]).arg((*request)[5]));

	_conn->setSentEncrypted();
	_conn->sendData(result);

	if (needAnyResponse) {
		onSentSome(result.size() * sizeof(mtpPrime));
	}

	return true;
}

mtpRequestId ConnectionPrivate::wasSent(mtpMsgId msgId) const {
	if (msgId == _pingMsgId) return mtpRequestId(0xFFFFFFFF);
	{
		QReadLocker locker(sessionData->haveSentMutex());
		const mtpRequestMap &haveSent(sessionData->haveSentMap());
		mtpRequestMap::const_iterator i = haveSent.constFind(msgId);
		if (i != haveSent.cend()) return i.value()->requestId ? i.value()->requestId : mtpRequestId(0xFFFFFFFF);
	}
	{
		QReadLocker locker(sessionData->toResendMutex());
		const mtpRequestIdsMap &toResend(sessionData->toResendMap());
		mtpRequestIdsMap::const_iterator i = toResend.constFind(msgId);
		if (i != toResend.cend()) return i.value();
	}
	{
		QReadLocker locker(sessionData->wereAckedMutex());
		const mtpRequestIdsMap &wereAcked(sessionData->wereAckedMap());
		mtpRequestIdsMap::const_iterator i = wereAcked.constFind(msgId);
		if (i != wereAcked.cend()) return i.value();
	}
	return 0;
}

void ConnectionPrivate::lockKey() {
	unlockKey();
	sessionData->keyMutex()->lockForWrite();
	myKeyLock = true;
}

void ConnectionPrivate::unlockKey() {
	if (myKeyLock) {
		myKeyLock = false;
		sessionData->keyMutex()->unlock();
	}
}

ConnectionPrivate::~ConnectionPrivate() {
	clearAuthKeyData();
	Assert(_finished && _conn == nullptr && _conn4 == nullptr && _conn6 == nullptr);
}

void ConnectionPrivate::stop() {
	QWriteLocker lockFinished(&sessionDataMutex);
	if (sessionData) {
		if (myKeyLock) {
			sessionData->owner()->notifyKeyCreated(AuthKeyPtr()); // release key lock, let someone else create it
			sessionData->keyMutex()->unlock();
			myKeyLock = false;
		}
		sessionData = nullptr;
	}
}

} // namespace internal

bool IsPrimeAndGood(base::const_byte_span primeBytes, int g) {
	return internal::IsPrimeAndGood(primeBytes, g);
}

ModExpFirst CreateModExp(int g, base::const_byte_span primeBytes, base::const_byte_span randomSeed) {
	return internal::CreateModExp(g, primeBytes, randomSeed);
}

std::vector<gsl::byte> CreateAuthKey(base::const_byte_span firstBytes, base::const_byte_span randomBytes, base::const_byte_span primeBytes) {
	return internal::CreateAuthKey(firstBytes, randomBytes, primeBytes);
}

} // namespace MTP
