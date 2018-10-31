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

#include "lang/lang_instance.h"
#include <QDate>

inline QString lang(LangKey key) {
	return Lang::Current().getValue(key);
}

inline Fn<QString()> langFactory(LangKey key) {
	return [key] { return Lang::Current().getValue(key); };
}

template <typename WithYear, typename WithoutYear>
inline QString langDateMaybeWithYear(QDate date, WithYear withYear, WithoutYear withoutYear) {
	auto month = date.month();
	if (month <= 0 || month > 12) {
		return qsl("MONTH_ERR");
	};
	auto year = date.year();
	auto current = QDate::currentDate();
	auto currentYear = current.year();
	auto currentMonth = current.month();
	if (year != currentYear) {
		auto yearIsMuchGreater = [](int year, int otherYear) { return (year > otherYear + 1); };
		auto monthIsMuchGreater = [](int year, int month, int otherYear, int otherMonth) {
			return (year == otherYear + 1) && (month + 12 > otherMonth + 3);
		};
		if (false || yearIsMuchGreater(year, currentYear) || yearIsMuchGreater(currentYear, year) ||
		    monthIsMuchGreater(year, month, currentYear, currentMonth) ||
		    monthIsMuchGreater(currentYear, currentMonth, year, month)) {
			return withYear(month, year);
		}
	}
	return withoutYear(month, year);
}

inline QString langDayOfMonth(const QDate &date) {
	auto day = date.day();
	return langDateMaybeWithYear(
	    date,
	    [day](int month, int year) {
		    return lng_month_day_year(lt_month, lang(LangKey(lng_month1_small + month - 1)), lt_day,
		                              QString::number(day), lt_year, QString::number(year));
	    },
	    [day](int month, int year) {
		    return lng_month_day(lt_month, lang(LangKey(lng_month1_small + month - 1)), lt_day, QString::number(day));
	    });
}

inline QString langDayOfMonthFull(const QDate &date) {
	auto day = date.day();
	return langDateMaybeWithYear(date,
	                             [day](int month, int year) {
		                             return lng_month_day_year(lt_month, lang(LangKey(lng_month1 + month - 1)), lt_day,
		                                                       QString::number(day), lt_year, QString::number(year));
	                             },
	                             [day](int month, int year) {
		                             return lng_month_day(lt_month, lang(LangKey(lng_month1 + month - 1)), lt_day,
		                                                  QString::number(day));
	                             });
}

inline QString langMonthOfYear(int month, int year) {
	return (month > 0 && month <= 12) ?
	           lng_month_year(lt_month, lang(LangKey(lng_month1_small + month - 1)), lt_year, QString::number(year)) :
	           qsl("MONTH_ERR");
}

inline QString langMonth(const QDate &date) {
	return langDateMaybeWithYear(date, [](int month, int year) { return langMonthOfYear(month, year); },
	                             [](int month, int year) { return lang(LangKey(lng_month1_small + month - 1)); });
}

inline QString langMonthOfYearFull(int month, int year) {
	return (month > 0 && month <= 12) ?
	           lng_month_year(lt_month, lang(LangKey(lng_month1 + month - 1)), lt_year, QString::number(year)) :
	           qsl("MONTH_ERR");
}

inline QString langMonthFull(const QDate &date) {
	return langDateMaybeWithYear(date, [](int month, int year) { return langMonthOfYearFull(month, year); },
	                             [](int month, int year) { return lang(LangKey(lng_month1 + month - 1)); });
}

inline QString langDayOfWeek(int index) {
	return (index > 0 && index <= 7) ? lang(LangKey(lng_weekday1 + index - 1)) : qsl("DAY_ERR");
}

inline QString langDayOfWeek(const QDate &date) {
	return langDayOfWeek(date.dayOfWeek());
}

inline QString langDateTime(const QDateTime &date) {
	return lng_mediaview_date_time(lt_date, langDayOfMonth(date.date()), lt_time, date.time().toString(cTimeFormat()));
}

inline QString langDateTimeFull(const QDateTime &date) {
	return lng_mediaview_date_time(lt_date, langDayOfMonthFull(date.date()), lt_time,
	                               date.time().toString(cTimeFormat()));
}

bool langFirstNameGoesSecond();
