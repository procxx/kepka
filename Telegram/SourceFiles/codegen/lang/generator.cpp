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
#include "codegen/lang/generator.h"

#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <functional>
#include <memory>

namespace codegen {
namespace lang {
namespace {

char hexChar(uchar ch) {
	if (ch < 10) {
		return '0' + ch;
	} else if (ch < 16) {
		return 'a' + (ch - 10);
	}
	return '0';
}

char hexSecondChar(char ch) {
	return hexChar((*reinterpret_cast<uchar *>(&ch)) & 0x0F);
}

char hexFirstChar(char ch) {
	return hexChar((*reinterpret_cast<uchar *>(&ch)) >> 4);
}

QString stringToEncodedString(const QString &str) {
	QString result, lineBreak = "\\\n";
	result.reserve(str.size() * 8);
	bool writingHexEscapedCharacters = false, startOnNewLine = false;
	int lastCutSize = 0;
	auto utf = str.toUtf8();
	for (auto ch : utf) {
		if (result.size() - lastCutSize > 80) {
			startOnNewLine = true;
			result.append(lineBreak);
			lastCutSize = result.size();
		}
		if (ch == '\n') {
			writingHexEscapedCharacters = false;
			result.append("\\n");
		} else if (ch == '\t') {
			writingHexEscapedCharacters = false;
			result.append("\\t");
		} else if (ch == '"' || ch == '\\') {
			writingHexEscapedCharacters = false;
			result.append('\\').append(ch);
		} else if (ch < 32 || static_cast<uchar>(ch) > 127) {
			writingHexEscapedCharacters = true;
			result.append("\\x").append(hexFirstChar(ch)).append(hexSecondChar(ch));
		} else {
			if (writingHexEscapedCharacters) {
				writingHexEscapedCharacters = false;
				result.append("\"\"");
			}
			result.append(ch);
		}
	}
	return '"' + (startOnNewLine ? lineBreak : QString()) + result + '"';
}

QString stringToEncodedString(const std::string &str) {
	return stringToEncodedString(QString::fromStdString(str));
}

QString stringToBinaryArray(const std::string &str) {
	QStringList rows, chars;
	chars.reserve(13);
	rows.reserve(1 + (str.size() / 13));
	for (uchar ch : str) {
		if (chars.size() > 12) {
			rows.push_back(chars.join(", "));
			chars.clear();
		}
		chars.push_back(QString("0x") + hexFirstChar(ch) + hexSecondChar(ch));
	}
	if (!chars.isEmpty()) {
		rows.push_back(chars.join(", "));
	}
	return QString("{") + ((rows.size() > 1) ? '\n' : ' ') + rows.join(",\n") + " }";
}

} // namespace

Generator::Generator(const LangPack &langpack, const QString &destBasePath, const common::ProjectInfo &project)
    : langpack_(langpack)
    , basePath_(destBasePath)
    , baseName_(QFileInfo(basePath_).baseName())
    , project_(project) {}

bool Generator::writeHeader() {
	header_ = std::make_unique<common::CppFile>(basePath_ + ".h", project_);

	header_->include("utility", true);
	header_->include("QString", true);
	header_->include("QLatin1String", true);

	header_->include("lang/lang_tag.h").newline().pushNamespace("Lang").stream() << "\
\n\
constexpr auto kTagsCount = " << langpack_.tags.size() << ";\n\
\n";

	header_->popNamespace().newline();
	auto index = 0;
	for (auto &tag : langpack_.tags) {
		header_->stream() << "enum lngtag_" << tag.tag << " { lt_" << tag.tag << " = " << index++ << " };\n";
	}
	header_->stream() << "\
\n\
enum LangKey {\n";
	for (auto &entry : langpack_.entries) {
		header_->stream() << "\t" << getFullKey(entry) << ",\n";
	}
	header_->stream() << "\
\n\
	kLangKeysCount,\n\
};\n\
\n\
QString lang(LangKey key);\n\
\n";

	for (auto &entry : langpack_.entries) {
		auto isPlural = !entry.keyBase.isEmpty();
		auto &key = entry.key;
		auto genericParams = QStringList();
		auto params = QStringList();
		auto applyTags = QStringList();
		auto plural = QString();
		auto nonPluralTagFound = false;
		for (auto &tagData : entry.tags) {
			auto &tag = tagData.tag;
			auto isPluralTag = isPlural && (tag == kPluralTag);
			genericParams.push_back("lngtag_" + tag + ", " + (isPluralTag ? "double " : "const ResultString &") + tag +
			                        "__val");
			params.push_back("lngtag_" + tag + ", " + (isPluralTag ? "double " : "const QString &") + tag + "__val");
			if (isPluralTag) {
				plural = "\tauto plural = Lang::Plural(" + key + ", " + kPluralTag + "__val);\n";
				applyTags.push_back("\tresult = Lang::ReplaceTag<ResultString>::Call(std::move(result), lt_" + tag +
				                    ", Lang::StartReplacements<ResultString>::Call(std::move(plural.replacement)));\n");
			} else {
				nonPluralTagFound = true;
				applyTags.push_back("\tresult = Lang::ReplaceTag<ResultString>::Call(std::move(result), lt_" + tag +
				                    ", " + tag + "__val);\n");
			}
		}
		if (!entry.tags.empty() && (!isPlural || key == ComputePluralKey(entry.keyBase, 0))) {
			auto initialString = isPlural ? ("std::move(plural.string)") : ("lang(" + getFullKey(entry) + ")");
			header_->stream() << "\
template <typename ResultString>\n\
inline ResultString " << (isPlural ? entry.keyBase : key)
			                  << "__generic(" << genericParams.join(QString(", ")) << ") {\n\
" << plural << "\
	auto result = Lang::StartReplacements<ResultString>::Call("
			                  << initialString << ");\n\
" << applyTags.join(QString()) << "\
	return result;\n\
}\n\
constexpr auto " << (isPlural ? entry.keyBase : key)
			                  << " = &" << (isPlural ? entry.keyBase : key) << "__generic<QString>;\n\
\n";
		}
	}

	header_->pushNamespace("Lang").stream() << "\
\n\
const char *GetKeyName(LangKey key);\n\
ushort GetTagIndex(QLatin1String tag);\n\
LangKey GetKeyIndex(QLatin1String key);\n\
bool IsTagReplaced(LangKey key, ushort tag);\n\
QString GetOriginalValue(LangKey key);\n\
\n";

	return header_->finalize();
}

bool Generator::writeSource() {
	source_ = std::make_unique<common::CppFile>(basePath_ + ".cpp", project_);

	source_->include("map", true);
	source_->include("string", true);
	source_->include("lang/lang_keys.h").pushNamespace("Lang").pushNamespace().stream() << "\
const std::map<std::string, LangKey> KeyMap = {\n\
\n";
	for (auto &entry : langpack_.entries) {
		source_->stream() << "{\"" << entry.key << "\"," << getFullKey(entry) << "},\n";
	}
	source_->stream() << "\
\n\
};\n\
\n\
const std::array<std::string, "
	                  << langpack_.entries.size() << "> KeyNames = {\n\
\n";
	for (auto &entry : langpack_.entries) {
		source_->stream() << "\"" << entry.key << "\",\n";
	}
	source_->stream() << "\
\n\
};\n\
\n\
QChar DefaultData[] = {";
	auto count = 0;
	auto fulllength = 0;
	for (auto &entry : langpack_.entries) {
		for (auto ch : entry.value) {
			if (fulllength > 0) source_->stream() << ",";
			if (!count++) {
				source_->stream() << "\n";
			} else {
				if (count == 12) {
					count = 0;
				}
				source_->stream() << " ";
			}
			source_->stream() << "0x" << QString::number(ch.unicode(), 16);
			++fulllength;
		}
	}
	source_->stream() << " };\n\
\n\
int Offsets[] = {";
	count = 0;
	auto offset = 0;
	auto writeOffset = [this, &count, &offset] {
		if (offset > 0) source_->stream() << ",";
		if (!count++) {
			source_->stream() << "\n";
		} else {
			if (count == 12) {
				count = 0;
			}
			source_->stream() << " ";
		}
		source_->stream() << offset;
	};
	for (auto &entry : langpack_.entries) {
		writeOffset();
		offset += entry.value.size();
	}
	writeOffset();
	source_->stream() << " };\n";

	source_->stream() << "\
const std::map<std::string, ushort> TagMap = {\n\
\n";

	for (auto &tag : langpack_.tags) {
		source_->stream() << "{\"" << tag.tag << "\","
		                  << "lt_" << tag.tag << "},\n";
	}
	source_->stream() << "\
\n\
};\n";
	source_->popNamespace().stream() << "\
\n\
const char *GetKeyName(LangKey key) {\n\
	return (key < 0 || key >= kLangKeysCount) ? \"\" : KeyNames[key].c_str();\n\
}\n\
\n\
ushort GetTagIndex(QLatin1String tag) {\n\
	auto data = tag.data();\n\
    return TagMap.find(data) != TagMap.end() ? TagMap.at(data) : kTagsCount;\n";

	source_->stream() << "\
}\n\
\n\
LangKey GetKeyIndex(QLatin1String key) {\n\
	auto data = key.data();\n\
    return KeyMap.find(data) != KeyMap.end() ? KeyMap.at(data) : kLangKeysCount;\n";

	source_->stream() << "\
}\n\
\n\
bool IsTagReplaced(LangKey key, ushort tag) {\n\
	switch (key) {\n";

	auto lastWrittenPluralEntry = QString();
	for (auto &entry : langpack_.entries) {
		if (entry.tags.empty()) {
			continue;
		}
		if (!entry.keyBase.isEmpty()) {
			if (entry.keyBase == lastWrittenPluralEntry) {
				continue;
			}
			lastWrittenPluralEntry = entry.keyBase;
			for (auto i = 0; i != kPluralPartCount; ++i) {
				source_->stream() << "\
	case " << ComputePluralKey(entry.keyBase, i)
				                  << ":" << ((i + 1 == kPluralPartCount) ? " {" : "") << "\n";
			}
		} else {
			source_->stream() << "\
	case " << getFullKey(entry)
			                  << ": {\n";
		}
		source_->stream() << "\
		switch (tag) {\n";
		for (auto &tag : entry.tags) {
			source_->stream() << "\
		case lt_" << tag.tag << ":\n";
		}
		source_->stream() << "\
			return true;\n\
		}\n\
	} break;\n";
	}

	source_->stream() << "\
	}\
\n\
	return false;\n\
}\n\
\n\
QString GetOriginalValue(LangKey key) {\n\
	Expects(key >= 0 && key < kLangKeysCount);\n\
	auto offset = Offsets[key];\n\
	return QString::fromRawData(DefaultData + offset, Offsets[key + 1] - offset);\n\
}\n\
\n";

	return source_->finalize();
}

QString Generator::getFullKey(const LangPack::Entry &entry) {
	if (!entry.keyBase.isEmpty() || entry.tags.empty()) {
		return entry.key;
	}
	return entry.key + "__tagged";
}

} // namespace lang
} // namespace codegen
