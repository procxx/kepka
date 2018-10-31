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
#include "mtproto/session.h"
#include "mtproto/mtp_instance.h"

#include "facade.h"
#include "mtproto/auth_key.h"
#include "mtproto/connection.h"
#include "mtproto/dcenter.h"
#include "scheme.h"

namespace MTP {
namespace internal {

void SessionData::setKey(const AuthKeyPtr &key) {
	if (_authKey != key) {
		quint64 session = rand_value<quint64>();
		_authKey = key;

		DEBUG_LOG(("MTP Info: new auth key set in SessionData, id %1, setting random server_session %2")
		              .arg(key ? key->keyId() : 0)
		              .arg(session));
		QWriteLocker locker(&_lock);
		if (_session != session) {
			_session = session;
			_messagesSent = 0;
		}
		_layerInited = false;
	}
}

void SessionData::clear(Instance *instance) {
	RPCCallbackClears clearCallbacks;
	{
		QReadLocker locker1(haveSentMutex()), locker2(toResendMutex()), locker3(haveReceivedMutex()),
		    locker4(wereAckedMutex());
		clearCallbacks.reserve(_haveSent.size() + _wereAcked.size());
		for (auto i = _haveSent.cbegin(), e = _haveSent.cend(); i != e; ++i) {
			auto requestId = i.value()->requestId;
			if (!_receivedResponses.contains(requestId)) {
				clearCallbacks.push_back(requestId);
			}
		}
		for (auto i = _toResend.cbegin(), e = _toResend.cend(); i != e; ++i) {
			auto requestId = i.value();
			if (!_receivedResponses.contains(requestId)) {
				clearCallbacks.push_back(requestId);
			}
		}
		for (auto i = _wereAcked.cbegin(), e = _wereAcked.cend(); i != e; ++i) {
			auto requestId = i.value();
			if (!_receivedResponses.contains(requestId)) {
				clearCallbacks.push_back(requestId);
			}
		}
	}
	{
		QWriteLocker locker(haveSentMutex());
		_haveSent.clear();
	}
	{
		QWriteLocker locker(toResendMutex());
		_toResend.clear();
	}
	{
		QWriteLocker locker(wereAckedMutex());
		_wereAcked.clear();
	}
	{
		QWriteLocker locker(receivedIdsMutex());
		_receivedIds.clear();
	}
	instance->clearCallbacksDelayed(clearCallbacks);
}

Session::Session(not_null<Instance *> instance, ShiftedDcId shiftedDcId)
    : QObject()
    , _instance(instance)
    , data(this)
    , dcWithShift(shiftedDcId) {
	connect(&timeouter, SIGNAL(timeout()), this, SLOT(checkRequestsByTimer()));
	timeouter.start(1000);

	data.setSystemLangCode(instance->systemLangCode());
	data.setCloudLangCode(instance->cloudLangCode());

	connect(&sender, SIGNAL(timeout()), this, SLOT(needToResumeAndSend()));
}

void Session::start() {
	createDcData();
	_connection = std::make_unique<Connection>(_instance);
	_connection->start(&data, dcWithShift);
	if (_instance->isKeysDestroyer()) {
		_instance->scheduleKeyDestroy(dcWithShift);
	}
}

void Session::createDcData() {
	if (dc) {
		return;
	}
	dc = _instance->getDcById(dcWithShift);

	if (auto lock = ReadLockerAttempt(keyMutex())) {
		data.setKey(dc->getKey());
		if (dc->connectionInited()) {
			data.setLayerWasInited(true);
		}
	}
	connect(dc.get(), SIGNAL(authKeyCreated()), this, SLOT(authKeyCreatedForDC()), Qt::QueuedConnection);
	connect(dc.get(), SIGNAL(layerWasInited(bool)), this, SLOT(layerWasInitedForDC(bool)), Qt::QueuedConnection);
}

void Session::registerRequest(mtpRequestId requestId, ShiftedDcId dcWithShift) {
	return _instance->registerRequest(requestId, dcWithShift);
}

mtpRequestId Session::storeRequest(mtpRequest &request, const RPCResponseHandler &parser) {
	return _instance->storeRequest(request, parser);
}

mtpRequest Session::getRequest(mtpRequestId requestId) {
	return _instance->getRequest(requestId);
}

bool Session::rpcErrorOccured(mtpRequestId requestId, const RPCFailHandlerPtr &onFail,
                              const RPCError &err) { // return true if need to clean request data
	return _instance->rpcErrorOccured(requestId, onFail, err);
}

void Session::restart() {
	if (_killed) {
		DEBUG_LOG(("Session Error: can't restart a killed session"));
		return;
	}
	data.setSystemLangCode(_instance->systemLangCode());
	data.setCloudLangCode(_instance->cloudLangCode());
	emit needToRestart();
}

void Session::stop() {
	if (_killed) {
		DEBUG_LOG(("Session Error: can't kill a killed session"));
		return;
	}
	DEBUG_LOG(("Session Info: stopping session dcWithShift %1").arg(dcWithShift));
	if (_connection) {
		_connection->kill();
		_instance->queueQuittingConnection(std::move(_connection));
	}
}

void Session::kill() {
	stop();
	_killed = true;
	DEBUG_LOG(("Session Info: marked session dcWithShift %1 as killed").arg(dcWithShift));
}

void Session::unpaused() {
	if (_needToReceive) {
		_needToReceive = false;
		QTimer::singleShot(0, this, SLOT(tryToReceive()));
	}
}

void Session::sendAnything(qint64 msCanWait) {
	if (_killed) {
		DEBUG_LOG(("Session Error: can't send anything in a killed session"));
		return;
	}
	auto ms = getms(true);
	if (msSendCall) {
		if (ms > msSendCall + msWait) {
			msWait = 0;
		} else {
			msWait = (msSendCall + msWait) - ms;
			if (msWait > msCanWait) {
				msWait = msCanWait;
			}
		}
	} else {
		msWait = msCanWait;
	}
	if (msWait) {
		DEBUG_LOG(("MTP Info: dcWithShift %1 can wait for %2ms from current %3")
		              .arg(dcWithShift)
		              .arg(msWait)
		              .arg(msSendCall));
		msSendCall = ms;
		sender.start(msWait);
	} else {
		DEBUG_LOG(("MTP Info: dcWithShift %1 stopped send timer, can wait for %2ms from current %3")
		              .arg(dcWithShift)
		              .arg(msWait)
		              .arg(msSendCall));
		sender.stop();
		msSendCall = 0;
		needToResumeAndSend();
	}
}

void Session::needToResumeAndSend() {
	if (_killed) {
		DEBUG_LOG(("Session Info: can't resume a killed session"));
		return;
	}
	if (!_connection) {
		DEBUG_LOG(("Session Info: resuming session dcWithShift %1").arg(dcWithShift));
		createDcData();
		_connection = std::make_unique<Connection>(_instance);
		_connection->start(&data, dcWithShift);
	}
	if (_ping) {
		_ping = false;
		emit needToPing();
	} else {
		emit needToSend();
	}
}

void Session::sendPong(quint64 msgId, quint64 pingId) {
	send(MTP_pong(MTP_long(msgId), MTP_long(pingId)));
}

void Session::sendMsgsStateInfo(quint64 msgId, QByteArray data) {
	auto info = std::string();
	if (!data.isEmpty()) {
		info.resize(data.size());
		auto src = gsl::as_bytes(gsl::make_span(data));
		//		auto dst = gsl::as_writeable_bytes(gsl::make_span(info));
		auto dst = gsl::as_writeable_bytes(gsl::make_span(&info[0], info.size()));
		base::copy_bytes(dst, src);
	}
	send(MTPMsgsStateInfo(MTP_msgs_state_info(MTP_long(msgId), MTP_string(std::move(info)))));
}

void Session::checkRequestsByTimer() {
	QVector<mtpMsgId> resendingIds;
	QVector<mtpMsgId> removingIds; // remove very old (10 minutes) containers and resend requests
	QVector<mtpMsgId> stateRequestIds;

	{
		QReadLocker locker(data.haveSentMutex());
		mtpRequestMap &haveSent(data.haveSentMap());
		quint32 haveSentCount(haveSent.size());
		auto ms = getms(true);
		for (mtpRequestMap::iterator i = haveSent.begin(), e = haveSent.end(); i != e; ++i) {
			mtpRequest &req(i.value());
			if (req->msDate > 0) {
				if (req->msDate + MTPCheckResendTimeout < ms) { // need to resend or check state
					if (mtpRequestData::messageSize(req) < MTPResendThreshold) { // resend
						resendingIds.reserve(haveSentCount);
						resendingIds.push_back(i.key());
					} else {
						req->msDate = ms;
						stateRequestIds.reserve(haveSentCount);
						stateRequestIds.push_back(i.key());
					}
				}
			} else if (unixtime() > (qint32)(i.key() >> 32) + MTPContainerLives) {
				removingIds.reserve(haveSentCount);
				removingIds.push_back(i.key());
			}
		}
	}

	if (stateRequestIds.size()) {
		DEBUG_LOG(("MTP Info: requesting state of msgs: %1").arg(Logs::vector(stateRequestIds)));
		{
			QWriteLocker locker(data.stateRequestMutex());
			for (quint32 i = 0, l = stateRequestIds.size(); i < l; ++i) {
				data.stateRequestMap().insert(stateRequestIds[i], true);
			}
		}
		sendAnything(MTPCheckResendWaiting);
	}
	if (!resendingIds.isEmpty()) {
		for (quint32 i = 0, l = resendingIds.size(); i < l; ++i) {
			DEBUG_LOG(("MTP Info: resending request %1").arg(resendingIds[i]));
			resend(resendingIds[i], MTPCheckResendWaiting);
		}
	}
	if (!removingIds.isEmpty()) {
		RPCCallbackClears clearCallbacks;
		{
			QWriteLocker locker(data.haveSentMutex());
			mtpRequestMap &haveSent(data.haveSentMap());
			for (quint32 i = 0, l = removingIds.size(); i < l; ++i) {
				mtpRequestMap::iterator j = haveSent.find(removingIds[i]);
				if (j != haveSent.cend()) {
					if (j.value()->requestId) {
						clearCallbacks.push_back(j.value()->requestId);
					}
					haveSent.erase(j);
				}
			}
		}
		_instance->clearCallbacksDelayed(clearCallbacks);
	}
}

void Session::onConnectionStateChange(qint32 newState) {
	_instance->onStateChange(dcWithShift, newState);
}

void Session::onResetDone() {
	_instance->onSessionReset(dcWithShift);
}

void Session::cancel(mtpRequestId requestId, mtpMsgId msgId) {
	if (requestId) {
		QWriteLocker locker(data.toSendMutex());
		data.toSendMap().remove(requestId);
	}
	if (msgId) {
		QWriteLocker locker(data.haveSentMutex());
		data.haveSentMap().remove(msgId);
	}
}

void Session::ping() {
	_ping = true;
	sendAnything(0);
}

qint32 Session::requestState(mtpRequestId requestId) const {
	qint32 result = MTP::RequestSent;

	bool connected = false;
	if (_connection) {
		qint32 s = _connection->state();
		if (s == ConnectedState) {
			connected = true;
		} else if (s == ConnectingState || s == DisconnectedState) {
			if (result < 0 || result == MTP::RequestSent) {
				result = MTP::RequestConnecting;
			}
		} else if (s < 0) {
			if ((result < 0 && s > result) || result == MTP::RequestSent) {
				result = s;
			}
		}
	}
	if (!connected) {
		return result;
	}
	if (!requestId) return MTP::RequestSent;

	QWriteLocker locker(data.toSendMutex());
	const mtpPreRequestMap &toSend(data.toSendMap());
	mtpPreRequestMap::const_iterator i = toSend.constFind(requestId);
	if (i != toSend.cend()) {
		return MTP::RequestSending;
	} else {
		return MTP::RequestSent;
	}
}

qint32 Session::getState() const {
	qint32 result = -86400000;

	if (_connection) {
		qint32 s = _connection->state();
		if (s == ConnectedState) {
			return s;
		} else if (s == ConnectingState || s == DisconnectedState) {
			if (result < 0) {
				return s;
			}
		} else if (s < 0) {
			if (result < 0 && s > result) {
				result = s;
			}
		}
	}
	if (result == -86400000) {
		result = DisconnectedState;
	}
	return result;
}

QString Session::transport() const {
	return _connection ? _connection->transport() : QString();
}

mtpRequestId Session::resend(quint64 msgId, qint64 msCanWait, bool forceContainer, bool sendMsgStateInfo) {
	mtpRequest request;
	{
		QWriteLocker locker(data.haveSentMutex());
		mtpRequestMap &haveSent(data.haveSentMap());

		mtpRequestMap::iterator i = haveSent.find(msgId);
		if (i == haveSent.end()) {
			if (sendMsgStateInfo) {
				char cantResend[2] = {1, 0};
				DEBUG_LOG(("Message Info: cant resend %1, request not found").arg(msgId));

				return send(MTP_msgs_state_info(MTP_long(msgId), MTP_string(std::string(cantResend, cantResend + 1))));
			}
			return 0;
		}

		request = i.value();
		haveSent.erase(i);
	}
	if (mtpRequestData::isSentContainer(request)) { // for container just resend all messages we can
		DEBUG_LOG(("Message Info: resending container from haveSent, msgId %1").arg(msgId));
		const mtpMsgId *ids = (const mtpMsgId *)(request->constData() + 8);
		for (quint32 i = 0, l = (request->size() - 8) >> 1; i < l; ++i) {
			resend(ids[i], 10, true);
		}
		return 0xFFFFFFFF;
	} else if (!mtpRequestData::isStateRequest(request)) {
		request->msDate = forceContainer ? 0 : getms(true);
		sendPrepared(request, msCanWait, false);
		{
			QWriteLocker locker(data.toResendMutex());
			data.toResendMap().insert(msgId, request->requestId);
		}
		return request->requestId;
	} else {
		return 0;
	}
}

void Session::resendMany(QVector<quint64> msgIds, qint64 msCanWait, bool forceContainer, bool sendMsgStateInfo) {
	for (qint32 i = 0, l = msgIds.size(); i < l; ++i) {
		resend(msgIds.at(i), msCanWait, forceContainer, sendMsgStateInfo);
	}
}

void Session::resendAll() {
	QVector<mtpMsgId> toResend;
	{
		QReadLocker locker(data.haveSentMutex());
		const mtpRequestMap &haveSent(data.haveSentMap());
		toResend.reserve(haveSent.size());
		for (mtpRequestMap::const_iterator i = haveSent.cbegin(), e = haveSent.cend(); i != e; ++i) {
			if (i.value()->requestId) toResend.push_back(i.key());
		}
	}
	for (quint32 i = 0, l = toResend.size(); i < l; ++i) {
		resend(toResend[i], 10, true);
	}
}

void Session::sendPrepared(const mtpRequest &request, TimeMs msCanWait,
                           bool newRequest) { // returns true, if emit of needToSend() is needed
	{
		QWriteLocker locker(data.toSendMutex());
		data.toSendMap().insert(request->requestId, request);

		if (newRequest) {
			*(mtpMsgId *)(request->data() + 4) = 0;
			*(request->data() + 6) = 0;
		}
	}

	DEBUG_LOG(("MTP Info: added, requestId %1").arg(request->requestId));

	sendAnything(msCanWait);
}

QReadWriteLock *Session::keyMutex() const {
	return dc->keyMutex();
}

void Session::authKeyCreatedForDC() {
	DEBUG_LOG(("AuthKey Info: Session::authKeyCreatedForDC slot, emitting authKeyCreated(), dcWithShift %1")
	              .arg(dcWithShift));
	data.setKey(dc->getKey());
	emit authKeyCreated();
}

void Session::notifyKeyCreated(AuthKeyPtr &&key) {
	DEBUG_LOG(("AuthKey Info: Session::keyCreated(), setting, dcWithShift %1").arg(dcWithShift));
	dc->setKey(std::move(key));
}

void Session::layerWasInitedForDC(bool wasInited) {
	DEBUG_LOG(("MTP Info: Session::layerWasInitedForDC slot, dcWithShift %1").arg(dcWithShift));
	data.setLayerWasInited(wasInited);
}

void Session::notifyLayerInited(bool wasInited) {
	DEBUG_LOG(
	    ("MTP Info: emitting MTProtoDC::layerWasInited(%1), dcWithShift %2").arg(Logs::b(wasInited)).arg(dcWithShift));
	dc->setConnectionInited(wasInited);
	emit dc->layerWasInited(wasInited);
}

void Session::destroyKey() {
	if (!dc) return;

	if (data.getKey()) {
		DEBUG_LOG(("MTP Info: destroying auth_key for dcWithShift %1").arg(dcWithShift));
		if (data.getKey() == dc->getKey()) {
			dc->destroyKey();
		}
		data.setKey(AuthKeyPtr());
	}
}

qint32 Session::getDcWithShift() const {
	return dcWithShift;
}

void Session::tryToReceive() {
	if (_killed) {
		DEBUG_LOG(("Session Error: can't receive in a killed session"));
		return;
	}
	if (paused()) {
		_needToReceive = true;
		return;
	}
	while (true) {
		auto requestId = mtpRequestId(0);
		auto isUpdate = false;
		auto message = SerializedMessage();
		{
			QWriteLocker locker(data.haveReceivedMutex());
			auto &responses = data.haveReceivedResponses();
			auto response = responses.begin();
			if (response == responses.cend()) {
				auto &updates = data.haveReceivedUpdates();
				auto update = updates.begin();
				if (update == updates.cend()) {
					return;
				} else {
					message = std::move(*update);
					isUpdate = true;
					updates.pop_front();
				}
			} else {
				requestId = response.key();
				message = std::move(response.value());
				responses.erase(response);
			}
		}
		if (isUpdate) {
			if (dcWithShift == bareDcId(dcWithShift)) { // call globalCallback only in main session
				_instance->globalCallback(message.constData(), message.constData() + message.size());
			}
		} else {
			_instance->execCallback(requestId, message.constData(), message.constData() + message.size());
		}
	}
}

Session::~Session() {
	Assert(_connection == nullptr);
}

MTPrpcError rpcClientError(const QString &type, const QString &description) {
	return MTP_rpc_error(
	    MTP_int(0),
	    MTP_string(("CLIENT_" + type + (description.length() ? (": " + description) : "")).toUtf8().constData()));
}

} // namespace internal
} // namespace MTP
