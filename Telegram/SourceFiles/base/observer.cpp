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
#include "base/observer.h"
#include "facades.h"

namespace base {
namespace internal {
namespace {

bool CantUseObservables = false;

struct ObservableListWrap {
	~ObservableListWrap() {
		CantUseObservables = true;
	}
	std::set<ObservableCallHandlers *> list;
};

ObservableListWrap &PendingObservables() {
	static ObservableListWrap result;
	return result;
}

ObservableListWrap &ActiveObservables() {
	static ObservableListWrap result;
	return result;
}

} // namespace

void RegisterPendingObservable(ObservableCallHandlers *handlers) {
	if (CantUseObservables) return;
	PendingObservables().list.insert(handlers);
	Global::RefHandleObservables().call();
}

void UnregisterActiveObservable(ObservableCallHandlers *handlers) {
	if (CantUseObservables) return;
	ActiveObservables().list.erase(handlers);
}

void UnregisterObservable(ObservableCallHandlers *handlers) {
	if (CantUseObservables) return;
	PendingObservables().list.erase(handlers);
	ActiveObservables().list.erase(handlers);
}

} // namespace internal

void HandleObservables() {
	if (internal::CantUseObservables) return;
	auto &active = internal::ActiveObservables().list;
	qSwap(active, internal::PendingObservables().list);
	while (!active.empty()) {
		auto first = *active.begin();
		(*first)();
		if (!active.empty() && *active.begin() == first) {
			active.erase(active.begin());
		}
	}
}

} // namespace base
