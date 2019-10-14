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


namespace Data {

template <
        typename FlagsType,
        typename FlagsType::Type kEssential = -1>
class Flags {
public:
        struct Change {
                Change(FlagsType diff, FlagsType value)
                : diff(diff)
                , value(value) {
                }
                FlagsType diff = 0;
                FlagsType value = 0;
        };

        Flags() = default;
        Flags(FlagsType value) : _value(value) {
        }

        void set(FlagsType which) {
                if (auto diff = which ^ _value) {
                        _value = which;
                        //updated(diff);
                }
        }
        void add(FlagsType which) {
                if (auto diff = which & ~_value) {
                        _value |= which;
                        //updated(diff);
                }
        }
        void remove(FlagsType which) {
                if (auto diff = which & _value) {
                        _value &= ~which;
                        //updated(diff);
                }
        }
        FlagsType current() const {
                return _value;
        }
//        rpl::producer<Change> changes() const {
//                return _changes.events();
//        }
//        rpl::producer<Change> value() const {
//                return _changes.events_starting_with({
//                        FlagsType::from_raw(kEssential),
//                        _value });
//        }

private:
//        void updated(FlagsType diff) {
//                if ((diff &= FlagsType::from_raw(kEssential))) {
//                        _changes.fire({ diff, _value });
//                }
//        }

        FlagsType _value = 0;
        //rpl::event_stream<Change> _changes;

};

} // namespace Data
