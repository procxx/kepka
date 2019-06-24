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
#include "codegen/emoji/generator.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QtPlugin>
#include <QtGui/QFontDatabase>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#ifdef SUPPORT_IMAGE_GENERATION
Q_IMPORT_PLUGIN(QWebpPlugin)
#ifdef Q_OS_MAC
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#elif defined Q_OS_WIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#else // !Q_OS_MAC && !Q_OS_WIN
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif // !Q_OS_MAC && !Q_OS_WIN
#endif // SUPPORT_IMAGE_GENERATION

namespace codegen {
namespace emoji {
namespace {

constexpr auto kErrorCantWritePath = 851;

constexpr auto kOriginalBits = 12;
constexpr auto kIdSizeBits = 6;
constexpr auto kColumnBits = 6;
constexpr auto kRowBits = 6;

common::ProjectInfo Project = {
    "codegen_emoji", "empty",
    false, // forceReGenerate
};

QRect computeSourceRect(const QImage &image) {
	auto size = image.width();
	auto result = QRect(2, 2, size - 4, size - 4);
	auto top = 1, bottom = 1, left = 1, right = 1;
	auto rgbBits = reinterpret_cast<const QRgb *>(image.constBits());
	for (auto i = 0; i != size; ++i) {
		if (rgbBits[i] > 0 || rgbBits[(size - 1) * size + i] > 0 || rgbBits[i * size] > 0 ||
		    rgbBits[i * size + (size - 1)] > 0) {
			logDataError() << "Bad border.";
			return QRect();
		}
		if (rgbBits[1 * size + i] > 0) {
			top = -1;
		} else if (top > 0 && rgbBits[2 * size + i] > 0) {
			top = 0;
		}
		if (rgbBits[(size - 2) * size + i] > 0) {
			bottom = -1;
		} else if (bottom > 0 && rgbBits[(size - 3) * size + i] > 0) {
			bottom = 0;
		}
		if (rgbBits[i * size + 1] > 0) {
			left = -1;
		} else if (left > 0 && rgbBits[i * size + 2] > 0) {
			left = 0;
		}
		if (rgbBits[i * size + (size - 2)] > 0) {
			right = -1;
		} else if (right > 0 && rgbBits[i * size + (size - 3)] > 0) {
			right = 0;
		}
	}
	if (top < 0) {
		if (bottom <= 0) {
			logDataError() << "Bad vertical :(";
			return QRect();
		} else {
			result.setY(result.y() + 1);
		}
	} else if (bottom < 0) {
		if (top <= 0) {
			logDataError() << "Bad vertical :(";
			return QRect();
		} else {
			result.setY(result.y() - 1);
		}
	}
	if (left < 0) {
		if (right <= 0) {
			logDataError() << "Bad horizontal :(";
			return QRect();
		} else {
			result.setX(result.x() + 1);
		}
	} else if (right < 0) {
		if (left <= 0) {
			logDataError() << "Bad horizontal :(";
			return QRect();
		} else {
			result.setX(result.x() - 1);
		}
	}
	return result;
}

quint32 Crc32Table[256];
class Crc32Initializer {
public:
	Crc32Initializer() {
		quint32 poly = 0x04C11DB7U;
		for (auto i = 0; i != 256; ++i) {
			Crc32Table[i] = reflect(i, 8) << 24;
			for (auto j = 0; j != 8; ++j) {
				Crc32Table[i] = (Crc32Table[i] << 1) ^ (Crc32Table[i] & (1 << 31) ? poly : 0);
			}
			Crc32Table[i] = reflect(Crc32Table[i], 32);
		}
	}

private:
	quint32 reflect(quint32 val, char ch) {
		quint32 result = 0;
		for (int i = 1; i < (ch + 1); ++i) {
			if (val & 1) {
				result |= 1 << (ch - i);
			}
			val >>= 1;
		}
		return result;
	}
};

quint32 countCrc32(const void *data, std::size_t size) {
	static Crc32Initializer InitTable;

	auto buffer = static_cast<const unsigned char *>(data);
	auto result = quint32(0xFFFFFFFFU);
	for (auto i = std::size_t(0); i != size; ++i) {
		result = (result >> 8) ^ Crc32Table[(result & 0xFFU) ^ buffer[i]];
	}
	return (result ^ 0xFFFFFFFFU);
}

} // namespace

Generator::Generator(const Options &options)
    : project_(Project)
#ifdef SUPPORT_IMAGE_GENERATION
    , writeImages_(options.writeImages)
#endif // SUPPORT_IMAGE_GENERATION
    , data_(PrepareData())
    , replaces_(PrepareReplaces(options.replacesPath)) {
	QDir dir(options.outputPath);
	if (!dir.mkpath(".")) {
		common::logError(kErrorCantWritePath, "Command Line")
		    << "can not open path for writing: " << dir.absolutePath().toStdString();
		data_ = Data();
	}
	if (!CheckAndConvertReplaces(replaces_, data_)) {
		replaces_ = Replaces(replaces_.filename);
	}

	outputPath_ = dir.absolutePath() + "/emoji";
	spritePath_ = dir.absolutePath() + "/emoji";
	suggestionsPath_ = dir.absolutePath() + "/emoji_suggestions_data";
}

int Generator::generate() {
	if (data_.list.empty() || replaces_.list.isEmpty()) {
		return -1;
	}

#ifdef SUPPORT_IMAGE_GENERATION
	if (writeImages_) {
		return writeImages() ? 0 : -1;
	}
#endif // SUPPORT_IMAGE_GENERATION

	if (!writeSource()) {
		return -1;
	}
	if (!writeHeader()) {
		return -1;
	}
	if (!writeSuggestionsSource()) {
		return -1;
	}
	if (!writeSuggestionsHeader()) {
		return -1;
	}

	return 0;
}

constexpr auto kVariantsCount = 5;
constexpr auto kEmojiInRow = 40;

#ifdef SUPPORT_IMAGE_GENERATION
QImage Generator::generateImage(int variantIndex) {
	constexpr int kEmojiSizes[kVariantsCount + 1] = {18, 22, 27, 36, 45, 180};
	constexpr bool kBadSizes[kVariantsCount] = {true, true, false, false, false};
	constexpr int kEmojiFontSizes[kVariantsCount + 1] = {14, 20, 27, 36, 45, 180};
	constexpr int kEmojiDeltas[kVariantsCount + 1] = {15, 20, 25, 34, 42, 167};

	auto emojiCount = data_.list.size();
	auto columnsCount = kEmojiInRow;
	auto rowsCount = (emojiCount / columnsCount) + ((emojiCount % columnsCount) ? 1 : 0);

	auto emojiSize = kEmojiSizes[variantIndex];
	auto isBad = kBadSizes[variantIndex];
	auto sourceSize = (isBad ? kEmojiSizes[kVariantsCount] : emojiSize);

	auto font = QGuiApplication::font();
	font.setFamily(QStringLiteral("Apple Color Emoji"));
	font.setPixelSize(kEmojiFontSizes[isBad ? kVariantsCount : variantIndex]);

	auto singleSize = 4 + sourceSize;
	auto emojiImage = QImage(columnsCount * emojiSize, rowsCount * emojiSize, QImage::Format_ARGB32);
	emojiImage.fill(Qt::transparent);
	auto singleImage = QImage(singleSize, singleSize, QImage::Format_ARGB32);
	{
		QPainter p(&emojiImage);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		auto column = 0;
		auto row = 0;
		for (auto &emoji : data_.list) {
			{
				singleImage.fill(Qt::transparent);

				QPainter q(&singleImage);
				q.setPen(QColor(0, 0, 0, 255));
				q.setFont(font);
				q.drawText(2, 2 + kEmojiDeltas[isBad ? kVariantsCount : variantIndex], emoji.id);
			}
			auto sourceRect = computeSourceRect(singleImage);
			if (sourceRect.isEmpty()) {
				return QImage();
			}
			auto targetRect = QRect(column * emojiSize, row * emojiSize, emojiSize, emojiSize);
			if (isBad) {
				p.drawImage(targetRect,
				            singleImage.copy(sourceRect)
				                .scaled(emojiSize, emojiSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			} else {
				p.drawImage(targetRect, singleImage, sourceRect);
			}
			++column;
			if (column == columnsCount) {
				column = 0;
				++row;
			}
		}
	}
	return emojiImage;
}

bool Generator::writeImages() {
	constexpr const char *variantPostfix[] = {"", "_125x", "_150x", "_200x", "_250x"};
	for (auto variantIndex = 0; variantIndex != kVariantsCount; variantIndex++) {
		auto image = generateImage(variantIndex);
		auto postfix = variantPostfix[variantIndex];
		auto filename = spritePath_ + postfix + ".webp";
		auto bytes = QByteArray();
		{
			QBuffer buffer(&bytes);
			if (!image.save(&buffer, "WEBP", (variantIndex < 3) ? 100 : 99)) {
				logDataError() << "Could not save 'emoji" << postfix << ".webp'.";
				return false;
			}
		}
		auto needResave = !QFileInfo(filename).exists();
		if (!needResave) {
			QFile file(filename);
			if (!file.open(QIODevice::ReadOnly)) {
				needResave = true;
			} else {
				auto already = file.readAll();
				if (already.size() != bytes.size() || memcmp(already.constData(), bytes.constData(), already.size())) {
					needResave = true;
				}
			}
		}
		if (needResave) {
			QFile file(filename);
			if (!file.open(QIODevice::WriteOnly)) {
				logDataError() << "Could not open 'emoji" << postfix << ".png'.";
				return false;
			} else {
				if (file.write(bytes) != bytes.size()) {
					logDataError() << "Could not write 'emoji" << postfix << ".png'.";
					return false;
				}
			}
		}
	}
	return true;
}
#endif // SUPPORT_IMAGE_GENERATION

bool Generator::writeSource() {
	source_ = std::make_unique<common::CppFile>(outputPath_ + ".cpp", project_);

	source_->include("ui/emoji_config.h").newline();
	source_->include("emoji_suggestions_data.h").newline();
	source_->pushNamespace("Ui").pushNamespace("Emoji").pushNamespace();
	source_->stream() << "\
\n\
std::vector<One> Items;\n\
\n";
	if (!writeInitCode()) {
		return false;
	}
	if (!writeSections()) {
		return false;
	}
	if (!writeFindReplace()) {
		return false;
	}
	if (!writeFind()) {
		return false;
	}
	source_->popNamespace().newline().pushNamespace("internal");
	source_->stream() << "\
\n\
EmojiPtr ByIndex(size_t index) {\n\
	return index < Items.size() ? &Items[index] : nullptr;\n\
}\n\
\n\
EmojiPtr FindReplace(const QChar *start, const QChar *end, int *outLength) {\n\
	auto index = FindReplaceIndex(start, end, outLength);\n\
	return index ? &Items[index - 1] : nullptr;\n\
}\n\
\n\
EmojiPtr Find(const QChar *start, const QChar *end, int *outLength) {\n\
	auto index = FindIndex(start, end, outLength);\n\
	return index ? &Items[index - 1] : nullptr;\n\
}\n\
\n\
void Init() {\n\
	auto id = IdData;\n\
	auto takeString = [&id](int size) {\n\
		auto result = QString::fromRawData(reinterpret_cast<const QChar*>(id), size);\n\
		id += size;\n\
		return result;\n\
	};\n\
\n\
	Items.reserve(base::array_size(Data));\n\
	for (auto &data : Data) {\n\
		Items.emplace_back(takeString(data.idSize), quint16(data.column), quint16(data.row), bool(data.postfixed), bool(data.variated), data.original ? &Items[data.original - 1] : nullptr, One::CreationTag());\n\
	}\n\
	InitReplacements();\n\
}\n\
\n";
	source_->popNamespace();

	if (!writeGetSections()) {
		return false;
	}

	return source_->finalize();
}

bool Generator::writeHeader() {
	auto header = std::make_unique<common::CppFile>(outputPath_ + ".h", project_);

	header->include("QChar", true);
	header->include("settings.h");
	header->pushNamespace("Ui").pushNamespace("Emoji").pushNamespace("internal");
	header->stream() << "\
\n\
void Init();\n\
\n\
EmojiPtr ByIndex(size_t index);\n\
\n\
EmojiPtr Find(const QChar *ch, const QChar *end, int *outLength = nullptr);\n\
\n\
inline bool IsReplaceEdge(const QChar *ch) {\n\
	return true;\n\
\n\
//	switch (ch->unicode()) {\n\
//	case '.': case ',': case ':': case ';': case '!': case '?': case '#': case '@':\n\
//	case '(': case ')': case '[': case ']': case '{': case '}': case '<': case '>':\n\
//	case '+': case '=': case '-': case '_': case '*': case '/': case '\\\\': case '^': case '$':\n\
//	case '\"': case '\\'':\n\
//	case 8212: case 171: case 187: // --, <<, >>\n\
//		return true;\n\
//	}\n\
//	return false;\n\
}\n\
\n\
EmojiPtr FindReplace(const QChar *ch, const QChar *end, int *outLength = nullptr);\n\
\n";
	header->popNamespace().stream() << "\
\n\
enum class Section {\n\
	Recent,\n\
	People,\n\
	Nature,\n\
	Food,\n\
	Activity,\n\
	Travel,\n\
	Objects,\n\
	Symbols,\n\
};\n\
\n\
int Index();\n\
\n\
int GetSectionCount(Section section);\n\
EmojiPack GetSection(Section section);\n\
\n";
	return header->finalize();
}

template <typename Callback> bool Generator::enumerateWholeList(Callback callback) {
	auto column = 0;
	auto row = 0;
	auto index = 0;
	auto variated = -1;
	auto coloredCount = 0;
	for (auto &item : data_.list) {
		if (!callback(item.id, column, row, item.postfixed, item.variated, item.colored, variated)) {
			return false;
		}
		if (coloredCount > 0 && (item.variated || !item.colored)) {
			if (!colorsCount_) {
				colorsCount_ = coloredCount;
			} else if (colorsCount_ != coloredCount) {
				logDataError() << "different colored emoji count exist.";
				return false;
			}
			coloredCount = 0;
		}
		if (item.variated) {
			variated = index;
		} else if (item.colored) {
			if (variated <= 0) {
				logDataError() << "wrong order of colored items.";
				return false;
			}
			++coloredCount;
		} else if (variated >= 0) {
			variated = -1;
		}
		if (++column == kEmojiInRow) {
			column = 0;
			++row;
		}
		++index;
	}
	return true;
}

bool Generator::writeInitCode() {
	source_->stream() << "\
struct DataStruct {\n\
	ushort original : "
	                  << kOriginalBits << ";\n\
	uchar idSize : " << kIdSizeBits
	                  << ";\n\
	uchar column : " << kColumnBits
	                  << ";\n\
	uchar row : " << kRowBits
	                  << ";\n\
	bool postfixed : 1;\n\
	bool variated : 1;\n\
};\n\
\n\
const ushort IdData[] = {";
	startBinary();
	if (!enumerateWholeList([this](Id id, int column, int row, bool isPostfixed, bool isVariated, bool isColored,
	                               int original) { return writeStringBinary(source_.get(), id); })) {
		return false;
	}
	if (_binaryFullLength >= std::numeric_limits<ushort>::max()) {
		logDataError() << "Too many IdData elements.";
		return false;
	}
	source_->stream() << " };\n\
\n\
const DataStruct Data[] = {\n";
	if (!enumerateWholeList([this](Id id, int column, int row, bool isPostfixed, bool isVariated, bool isColored,
	                               int original) {
		    if (original + 1 >= (1 << kOriginalBits)) {
			    logDataError() << "Too many entries.";
			    return false;
		    }
		    if (id.size() >= (1 << kIdSizeBits)) {
			    logDataError() << "Too large id.";
			    return false;
		    }
		    if (column >= (1 << kColumnBits) || row >= (1 << kRowBits)) {
			    logDataError() << "Bad row-column.";
			    return false;
		    }
		    source_->stream() << "\
	{ ushort(" << (isColored ? (original + 1) : 0)
		                      << "), uchar(" << id.size() << "), uchar(" << column << "), uchar(" << row << "), "
		                      << (isPostfixed ? "true" : "false") << ", " << (isVariated ? "true" : "false") << " },\n";
		    return true;
	    })) {
		return false;
	}

	source_->stream() << "\
};\n";

	return true;
}

bool Generator::writeSections() {
	source_->stream() << "\
const ushort SectionData[] = {";
	startBinary();
	for (auto &category : data_.categories) {
		for (auto index : category) {
			writeIntBinary(source_.get(), index);
		}
	}
	source_->stream() << " };\n\
\n\
EmojiPack FillSection(int offset, int size) {\n\
	auto result = EmojiPack();\n\
	result.reserve(size);\n\
	for (auto index : gsl::make_span(SectionData + offset, size)) {\n\
		result.push_back(&Items[index]);\n\
	}\n\
	return result;\n\
}\n\n";
	return true;
}

bool Generator::writeGetSections() {
	constexpr const char *sectionNames[] = {
	    "Section::People", "Section::Nature",  "Section::Food",    "Section::Activity",
	    "Section::Travel", "Section::Objects", "Section::Symbols",
	};
	source_->stream() << "\
\n\
int GetSectionCount(Section section) {\n\
	switch (section) {\n\
	case Section::Recent: return GetRecent().size();\n";
	auto countIndex = 0;
	for (auto name : sectionNames) {
		if (countIndex >= int(data_.categories.size())) {
			logDataError() << "category " << countIndex << " not found.";
			return false;
		}
		source_->stream() << "\
	case " << name << ": return "
		                  << data_.categories[countIndex++].size() << ";\n";
	}
	source_->stream() << "\
	}\n\
	return 0;\n\
}\n\
\n\
EmojiPack GetSection(Section section) {\n\
	switch (section) {\n\
	case Section::Recent: {\n\
		auto result = EmojiPack();\n\
		result.reserve(GetRecent().size());\n\
		for (auto &item : GetRecent()) {\n\
			result.push_back(item.first);\n\
		}\n\
		return result;\n\
	} break;\n";
	auto index = 0;
	auto offset = 0;
	for (auto name : sectionNames) {
		if (index >= int(data_.categories.size())) {
			logDataError() << "category " << index << " not found.";
			return false;
		}
		auto &category = data_.categories[index++];
		source_->stream() << "\
\n\
	case " << name << ": {\n\
		static auto result = FillSection("
		                  << offset << ", " << category.size() << ");\n\
		return result;\n\
	} break;\n";
		offset += category.size();
	}
	source_->stream() << "\
	}\n\
	return EmojiPack();\n\
}\n\
\n";
	return true;
}

bool Generator::writeFindReplace() {
	source_->stream() << "\
const std::map<QString, int> ReplaceMap = {\n\
\n";

	for (auto &item : data_.replaces) {
		source_->stream() << "{\"" << item.first << "\"," << item.second << "},\n";
	}
	source_->stream() << "\
\n\
};\n";

	source_->stream() << "\
\n\
int FindReplaceIndex(const QChar *start, const QChar *end, int *outLength) {\n\
	auto ch = start;\n\
\n";

	if (!writeFindFromDictionary(data_.replaces, "ReplaceMap")) {
		return false;
	}

	source_->stream() << "\
}\n";

	return true;
}

bool Generator::writeFind() {
	source_->stream() << "\
const std::map<QString, int> EmojiMap = {\n\
\n";

	for (auto &item : data_.map) {
		source_->stream() << "{QString(\"" << item.first << "\")," << item.second << "},\n";
	}
	source_->stream() << "\
\n\
};\n";
	source_->stream() << "\
\n\
int FindIndex(const QChar *start, const QChar *end, int *outLength) {\n\
	auto ch = start;\n\
\n";

	if (!writeFindFromDictionary(data_.map, "EmojiMap", true)) {
		return false;
	}

	source_->stream() << "\
}\n\
\n";

	return true;
}
bool Generator::writeFindFromDictionary(const std::map<QString, int, std::greater<QString>> &dictionary,
                                        QString dict_name, bool skipPostfixes) {
	source_->stream() << R"code(	QString full_str(start, end-start), str="";
)code";

	auto max_el = std::max_element(dictionary.begin(), dictionary.end(),
	                               [](const auto &a, const auto &b) { return a.first.size() < b.first.size(); });
	auto max_length = max_el->first.size();
	source_->stream() << "\tstd::array<size_t, " << max_length + 1 << "> sizes;\n";
	if (skipPostfixes) {
		source_->stream() << "\tfor (int i=0,l=0; i<full_str.size() && l < " << max_length << R"code(; ++i) {
		if (full_str[i] == kPostfix) {continue;}
		++l;
		sizes[l]=i+1;
		str+=full_str[i];
	}
)code";
	} else {
		source_->stream() << "\tstd::iota(sizes.begin(), sizes.end(), 0);\n";
		source_->stream() << "\tstr = full_str.mid(0," << max_length << ");\n";
	}
	source_->stream() << R"code(	for (int curr_len = str.size(); curr_len>0; --curr_len) {
		QString substr = str.mid(0,curr_len);
		if ()code" << dict_name
	                  << ".find(substr) !=" << dict_name << R"code(.end()) {
			if (outLength) *outLength = sizes[curr_len];
			return )code"
	                  << dict_name << R"code(.at(substr);
		}
	}
	return 0;
)code";
	return true;
}


bool Generator::writeSuggestionsSource() {
	suggestionsSource_ = std::make_unique<common::CppFile>(suggestionsPath_ + ".cpp", project_);
	suggestionsSource_->stream() << "\
#include <map>\n\
\n";
	suggestionsSource_->pushNamespace("Ui").pushNamespace("Emoji").pushNamespace("internal").pushNamespace();
	suggestionsSource_->stream() << "\
\n";
	if (!writeReplacements()) {
		return false;
	}
	suggestionsSource_->popNamespace().newline();
	if (!writeGetReplacements()) {
		return false;
	}

	return suggestionsSource_->finalize();
}

bool Generator::writeSuggestionsHeader() {
	auto maxLength = 0;
	for (auto &replace : replaces_.list) {
		if (maxLength < replace.replacement.size()) {
			maxLength = replace.replacement.size();
		}
	}
	auto header = std::make_unique<common::CppFile>(suggestionsPath_ + ".h", project_);
	header->include("emoji_suggestions.h").newline();
	header->pushNamespace("Ui").pushNamespace("Emoji").pushNamespace("internal");
	header->stream() << "\
\n\
struct Replacement {\n\
	utf16string emoji;\n\
	utf16string replacement;\n\
	std::vector<utf16string> words;\n\
};\n\
\n\
constexpr auto kReplacementMaxLength = "
	                 << maxLength << ";\n\
\n\
void InitReplacements();\n\
const std::vector<const Replacement*> *GetReplacements(char16_t first);\n\
utf16string GetReplacementEmoji(utf16string replacement);\n\
\n";
	return header->finalize();
}

bool Generator::writeReplacements() {
	QMap<QChar, QVector<int>> byCharIndices;
	suggestionsSource_->stream() << "\
struct ReplacementStruct {\n\
	uint8_t emojiSize;\n\
	uint8_t replacementSize;\n\
	uint8_t wordsCount;\n\
};\n\
\n\
const char16_t ReplacementData[] = {";
	startBinary();
	for (auto i = 0, size = replaces_.list.size(); i != size; ++i) {
		auto &replace = replaces_.list[i];
		if (!writeStringBinary(suggestionsSource_.get(), replace.id)) {
			return false;
		}
		if (!writeStringBinary(suggestionsSource_.get(), replace.replacement)) {
			return false;
		}
		for (auto &word : replace.words) {
			if (!writeStringBinary(suggestionsSource_.get(), word)) {
				return false;
			}
			auto &index = byCharIndices[word[0]];
			if (index.isEmpty() || index.back() != i) {
				index.push_back(i);
			}
		}
	}
	suggestionsSource_->stream() << " };\n\
\n\
const uint8_t ReplacementWordLengths[] = {";
	startBinary();
	for (auto &replace : replaces_.list) {
		auto wordLengths = QStringList();
		for (auto &word : replace.words) {
			writeIntBinary(suggestionsSource_.get(), word.size());
		}
	}
	suggestionsSource_->stream() << " };\n\
\n\
const ReplacementStruct ReplacementInitData[] = {\n";
	for (auto &replace : replaces_.list) {
		suggestionsSource_->stream() << "\
	{ uint8_t(" << replace.id.size() << "), uint8_t("
		                             << replace.replacement.size() << "), uint8_t(" << replace.words.size() << ") },\n";
	}
	suggestionsSource_->stream() << "};\n\
\n\
const uint16_t ReplacementIndices[] = {";
	startBinary();
	for (auto &byCharIndex : byCharIndices) {
		for (auto index : byCharIndex) {
			writeIntBinary(suggestionsSource_.get(), index);
		}
	}
	suggestionsSource_->stream() << " };\n\
\n\
struct ReplacementIndexStruct {\n\
	char16_t ch;\n\
	uint16_t count;\n\
};\n\
\n\
const internal::checksum ReplacementChecksums[] = {\n";
	startBinary();
	for (auto &replace : replaces_.list) {
		writeUintBinary(suggestionsSource_.get(),
		                countCrc32(replace.replacement.constData(), replace.replacement.size() * sizeof(QChar)));
	}
	suggestionsSource_->stream() << " };\n\
\n\
const ReplacementIndexStruct ReplacementIndexData[] = {\n";
	startBinary();
	for (auto i = byCharIndices.cbegin(), e = byCharIndices.cend(); i != e; ++i) {
		suggestionsSource_->stream() << "\
	{ char16_t(" << i.key().unicode() << "), uint16_t("
		                             << i.value().size() << ") },\n";
	}
	suggestionsSource_->stream() << "};\n\
\n\
std::vector<Replacement> Replacements;\n\
std::map<char16_t, std::vector<const Replacement*>> ReplacementsMap;\n\
std::map<internal::checksum, const Replacement*> ReplacementsHash;\n\
\n";
	return true;
}

bool Generator::writeGetReplacements() {
	suggestionsSource_->stream() << "\
void InitReplacements() {\n\
	if (!Replacements.empty()) {\n\
		return;\n\
	}\n\
	auto data = ReplacementData;\n\
	auto takeString = [&data](int size) {\n\
		auto result = utf16string(data, size);\n\
		data += size;\n\
		return result;\n\
	};\n\
	auto wordSize = ReplacementWordLengths;\n\
\n\
	Replacements.reserve(" << replaces_.list.size()
	                             << ");\n\
	for (auto item : ReplacementInitData) {\n\
		auto emoji = takeString(item.emojiSize);\n\
		auto replacement = takeString(item.replacementSize);\n\
		auto words = std::vector<utf16string>();\n\
		words.reserve(item.wordsCount);\n\
		for (auto i = 0; i != item.wordsCount; ++i) {\n\
			words.push_back(takeString(*wordSize++));\n\
		}\n\
		Replacements.push_back({ std::move(emoji), std::move(replacement), std::move(words) });\n\
	}\n\
\n\
	auto indices = ReplacementIndices;\n\
	auto items = &Replacements[0];\n\
	for (auto item : ReplacementIndexData) {\n\
		auto index = std::vector<const Replacement*>();\n\
		index.reserve(item.count);\n\
		for (auto i = 0; i != item.count; ++i) {\n\
			index.push_back(items + (*indices++));\n\
		}\n\
		ReplacementsMap.emplace(item.ch, std::move(index));\n\
	}\n\
\n\
	for (auto checksum : ReplacementChecksums) {\n\
		ReplacementsHash.emplace(checksum, items++);\n\
	}\n\
}\n\
\n\
const std::vector<const Replacement*> *GetReplacements(char16_t first) {\n\
	if (ReplacementsMap.empty()) {\n\
		InitReplacements();\n\
	}\n\
	auto it = ReplacementsMap.find(first);\n\
	return (it == ReplacementsMap.cend()) ? nullptr : &it->second;\n\
}\n\
\n\
utf16string GetReplacementEmoji(utf16string replacement) {\n\
	auto code = internal::countChecksum(replacement.data(), replacement.size() * sizeof(char16_t));\n\
	auto it = ReplacementsHash.find(code);\n\
	return (it == ReplacementsHash.cend()) ? utf16string() : it->second->emoji;\n\
}\n\
\n";
	return true;
}

void Generator::startBinary() {
	_binaryFullLength = _binaryCount = 0;
}

bool Generator::writeStringBinary(common::CppFile *source, const QString &string) {
	if (string.size() >= 256) {
		logDataError() << "Too long string: " << string.toStdString();
		return false;
	}
	for (auto ch : string) {
		if (_binaryFullLength > 0) source->stream() << ",";
		if (!_binaryCount++) {
			source->stream() << "\n";
		} else {
			if (_binaryCount == 12) {
				_binaryCount = 0;
			}
			source->stream() << " ";
		}
		source->stream() << "0x" << QString::number(ch.unicode(), 16);
		++_binaryFullLength;
	}
	return true;
}

void Generator::writeIntBinary(common::CppFile *source, int data) {
	if (_binaryFullLength > 0) source->stream() << ",";
	if (!_binaryCount++) {
		source->stream() << "\n";
	} else {
		if (_binaryCount == 12) {
			_binaryCount = 0;
		}
		source->stream() << " ";
	}
	source->stream() << data;
	++_binaryFullLength;
}

void Generator::writeUintBinary(common::CppFile *source, quint32 data) {
	if (_binaryFullLength > 0) source->stream() << ",";
	if (!_binaryCount++) {
		source->stream() << "\n";
	} else {
		if (_binaryCount == 12) {
			_binaryCount = 0;
		}
		source->stream() << " ";
	}
	source->stream() << "0x" << QString::number(data, 16).toUpper() << "U";
	++_binaryFullLength;
}

} // namespace emoji
} // namespace codegen
