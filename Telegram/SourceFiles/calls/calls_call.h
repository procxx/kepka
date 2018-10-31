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

#include "base/timer.h"
#include "base/weak_unique_ptr.h"
#include "mtproto/auth_key.h"
#include "mtproto/sender.h"

namespace Media {
namespace Audio {
class Track;
} // namespace Audio
} // namespace Media

namespace tgvoip {
class VoIPController;
} // namespace tgvoip

namespace Calls {

struct DhConfig {
	qint32 version = 0;
	qint32 g = 0;
	std::vector<gsl::byte> p;
};

class Call : public base::enable_weak_from_this, private MTP::Sender {
public:
	class Delegate {
	public:
		virtual DhConfig getDhConfig() const = 0;
		virtual void callFinished(not_null<Call *> call) = 0;
		virtual void callFailed(not_null<Call *> call) = 0;
		virtual void callRedial(not_null<Call *> call) = 0;

		enum class Sound {
			Connecting,
			Busy,
			Ended,
		};
		virtual void playSound(Sound sound) = 0;
	};

	static constexpr auto kRandomPowerSize = 256;
	static constexpr auto kSha256Size = 32;
	static constexpr auto kSoundSampleMs = 100;

	enum class Type {
		Incoming,
		Outgoing,
	};
	Call(not_null<Delegate *> delegate, not_null<UserData *> user, Type type);

	Type type() const {
		return _type;
	}
	not_null<UserData *> user() const {
		return _user;
	}
	bool isIncomingWaiting() const;

	void start(base::const_byte_span random);
	bool handleUpdate(const MTPPhoneCall &call);

	enum State {
		Starting,
		WaitingInit,
		WaitingInitAck,
		Established,
		FailedHangingUp,
		Failed,
		HangingUp,
		Ended,
		EndedByOtherDevice,
		ExchangingKeys,
		Waiting,
		Requesting,
		WaitingIncoming,
		Ringing,
		Busy,
	};
	State state() const {
		return _state;
	}
	base::Observable<State> &stateChanged() {
		return _stateChanged;
	}

	void setMute(bool mute);
	bool isMute() const {
		return _mute;
	}
	base::Observable<bool> &muteChanged() {
		return _muteChanged;
	}

	TimeMs getDurationMs() const;
	double getWaitingSoundPeakValue() const;

	void answer();
	void hangup();
	void redial();

	bool isKeyShaForFingerprintReady() const;
	std::array<gsl::byte, kSha256Size> getKeyShaForFingerprint() const;

	QString getDebugLog() const;

	~Call();

private:
	enum class FinishType {
		None,
		Ended,
		Failed,
	};
	void handleRequestError(const RPCError &error);
	void handleControllerError(int error);
	void finish(FinishType type, const MTPPhoneCallDiscardReason &reason = MTP_phoneCallDiscardReasonDisconnect());
	void startOutgoing();
	void startIncoming();
	void startWaitingTrack();

	void generateModExpFirst(base::const_byte_span randomSeed);
	void handleControllerStateChange(tgvoip::VoIPController *controller, int state);
	void createAndStartController(const MTPDphoneCall &call);

	template <typename T> bool checkCallCommonFields(const T &call);
	bool checkCallFields(const MTPDphoneCall &call);
	bool checkCallFields(const MTPDphoneCallAccepted &call);

	void confirmAcceptedCall(const MTPDphoneCallAccepted &call);
	void startConfirmedCall(const MTPDphoneCall &call);
	void setState(State state);
	void setStateQueued(State state);
	void setFailedQueued(int error);
	void destroyController();

	not_null<Delegate *> _delegate;
	not_null<UserData *> _user;
	Type _type = Type::Outgoing;
	State _state = State::Starting;
	FinishType _finishAfterRequestingCall = FinishType::None;
	bool _answerAfterDhConfigReceived = false;
	base::Observable<State> _stateChanged;
	TimeMs _startTime = 0;
	base::DelayedCallTimer _finishByTimeoutTimer;
	base::Timer _discardByTimeoutTimer;

	bool _mute = false;
	base::Observable<bool> _muteChanged;

	DhConfig _dhConfig;
	std::vector<gsl::byte> _ga;
	std::vector<gsl::byte> _gb;
	std::array<gsl::byte, kSha256Size> _gaHash;
	std::array<gsl::byte, kRandomPowerSize> _randomPower;
	MTP::AuthKey::Data _authKey;
	MTPPhoneCallProtocol _protocol;

	quint64 _id = 0;
	quint64 _accessHash = 0;
	quint64 _keyFingerprint = 0;

	std::unique_ptr<tgvoip::VoIPController> _controller;

	std::unique_ptr<Media::Audio::Track> _waitingTrack;
};

void UpdateConfig(const std::map<std::string, std::string> &data);

} // namespace Calls
