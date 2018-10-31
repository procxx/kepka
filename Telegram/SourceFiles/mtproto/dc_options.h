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

#include "base/observer.h"
#include "mtproto/rsa_public_key.h"
#include "scheme.h"
#include <map>
#include <string>
#include <vector>

namespace MTP {

enum class DcType {
	Regular,
	Temporary,
	MediaDownload,
	Cdn,
};
class DcOptions {
public:
	// construct methods don't notify "changed" subscribers.
	void constructFromSerialized(const QByteArray &serialized);
	void constructFromBuiltIn();
	void constructAddOne(int id, MTPDdcOption::Flags flags, const std::string &ip, int port);
	QByteArray serialize() const;

	using Ids = std::vector<DcId>;
	base::Observable<Ids> &changed() const {
		return _changed;
	}
	void setFromList(const MTPVector<MTPDcOption> &options);
	void addFromList(const MTPVector<MTPDcOption> &options);
	void addFromOther(DcOptions &&options);

	Ids configEnumDcIds() const;

	struct Endpoint {
		std::string ip;
		int port = 0;
		MTPDdcOption::Flags flags = 0;
	};
	struct Variants {
		enum {
			IPv4 = 0,
			IPv6 = 1,
			AddressTypeCount = 2,
		};
		enum {
			Tcp = 0,
			Http = 1,
			ProtocolCount = 2,
		};
		Endpoint data[AddressTypeCount][ProtocolCount];
	};
	Variants lookup(DcId dcId, DcType type) const;
	DcType dcType(ShiftedDcId shiftedDcId) const;

	void setCDNConfig(const MTPDcdnConfig &config);
	bool hasCDNKeysForDc(DcId dcId) const;
	bool getDcRSAKey(DcId dcId, const QVector<MTPlong> &fingerprints, internal::RSAPublicKey *result) const;

	// Debug feature for now.
	bool loadFromFile(const QString &path);
	bool writeToFile(const QString &path) const;

private:
	struct Option {
		Option(DcId id, MTPDdcOption::Flags flags, const std::string &ip, int port)
		    : id(id)
		    , flags(flags)
		    , ip(ip)
		    , port(port) {}

		DcId id;
		MTPDdcOption::Flags flags;
		std::string ip;
		int port;
	};
	bool applyOneGuarded(DcId dcId, MTPDdcOption::Flags flags, const std::string &ip, int port);

	void processFromList(const QVector<MTPDcOption> &options, bool overwrite);
	void computeCdnDcIds();

	void readBuiltInPublicKeys();

	class WriteLocker;
	friend class WriteLocker;

	class ReadLocker;
	friend class ReadLocker;

	std::map<ShiftedDcId, Option> _data;
	std::set<DcId> _cdnDcIds;
	std::map<quint64, internal::RSAPublicKey> _publicKeys;
	std::map<DcId, std::map<quint64, internal::RSAPublicKey>> _cdnPublicKeys;
	mutable QReadWriteLock _useThroughLockers;

	mutable base::Observable<Ids> _changed;

	// True when we have overriden options from a .tdesktop-endpoints file.
	bool _immutable = false;
};

} // namespace MTP
