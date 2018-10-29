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
#include "codegen/style/generator.h"

#include "codegen/style/parsed_file.h"
#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <functional>
#include <memory>

using Module = codegen::style::structure::Module;
using Struct = codegen::style::structure::Struct;
using Variable = codegen::style::structure::Variable;
using Tag = codegen::style::structure::TypeTag;

namespace codegen {
namespace style {
namespace {

constexpr int kErrorBadIconSize = 861;
constexpr int kErrorBadIconFormat = 862;

// crc32 hash, taken somewhere from the internet

class Crc32Table {
public:
	Crc32Table() {
		quint32 poly = 0x04c11db7;
		for (auto i = 0; i != 256; ++i) {
			_data[i] = reflect(i, 8) << 24;
			for (auto j = 0; j != 8; ++j) {
				_data[i] = (_data[i] << 1) ^ (_data[i] & (1 << 31) ? poly : 0);
			}
			_data[i] = reflect(_data[i], 32);
		}
	}

	inline quint32 operator[](int index) const {
		return _data[index];
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

	quint32 _data[256];
};

qint32 hashCrc32(const void *data, int len) {
	static Crc32Table table;

	const uchar *buffer = static_cast<const uchar *>(data);

	quint32 crc = 0xffffffff;
	for (int i = 0; i != len; ++i) {
		crc = (crc >> 8) ^ table[(crc & 0xFF) ^ buffer[i]];
	}

	return static_cast<qint32>(crc ^ 0xffffffff);
}

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

QString pxValueName(int value) {
	QString result = "px";
	if (value < 0) {
		value = -value;
		result += 'm';
	}
	return result + QString::number(value);
}

QString moduleBaseName(const structure::Module &module) {
	auto moduleInfo = QFileInfo(module.filepath());
	auto moduleIsPalette = (moduleInfo.suffix() == "palette");
	return moduleIsPalette ? "palette" : "style_" + moduleInfo.baseName();
}

QString colorFallbackName(structure::Value value) {
	auto copy = value.copyOf();
	if (!copy.isEmpty()) {
		return copy.back();
	}
	return value.Color().fallback;
}

QChar paletteColorPart(uchar part) {
	part = (part & 0x0F);
	if (part >= 10) {
		return 'a' + (part - 10);
	}
	return '0' + part;
}

QString paletteColorComponent(uchar value) {
	return QString() + paletteColorPart(value >> 4) + paletteColorPart(value);
}

QString paletteColorValue(const structure::data::color &value) {
	auto result =
	    paletteColorComponent(value.red) + paletteColorComponent(value.green) + paletteColorComponent(value.blue);
	if (value.alpha != 255) result += paletteColorComponent(value.alpha);
	return result;
}

} // namespace

Generator::Generator(const structure::Module &module, const QString &destBasePath, const common::ProjectInfo &project,
                     bool isPalette)
    : module_(module)
    , basePath_(destBasePath)
    , baseName_(QFileInfo(basePath_).baseName())
    , project_(project)
    , isPalette_(isPalette) {}

bool Generator::writeHeader() {
	header_ = std::make_unique<common::CppFile>(basePath_ + ".h", project_);

	header_->include("styles/style_basic.h").newline();
	header_->include("ui/style/style_core.h").newline();

	if (!writeHeaderStyleNamespace()) {
		return false;
	}
	if (!writeRefsDeclarations()) {
		return false;
	}

	return header_->finalize();
}

bool Generator::writeSource() {
	source_ = std::make_unique<common::CppFile>(basePath_ + ".cpp", project_);

	writeIncludesInSource();

	if (module_.hasVariables()) {
		source_->pushNamespace().newline();
		source_->stream() << R"code(bool inited = false;

class Module_)code" << baseName_
		                  << R"code( : public style::internal::ModuleBase {
public:
	Module_)code" << baseName_
		                  << R"code(() { style::internal::registerModule(this); }
	~Module_)code" << baseName_
		                  << R"code(() { style::internal::unregisterModule(this); }

	void start() override {
		style::internal::init_)code"
		                  << baseName_ << R"code(();
	}
	void stop() override {
	}
};
Module_)code" << baseName_ << " registrator;\n";
		if (isPalette_) {
			source_->newline();
			source_->stream() << "style::palette _palette;\n";
		} else {
			if (!writeVariableDefinitions()) {
				return false;
			}
		}
		source_->newline().popNamespace();

		source_->newline().pushNamespace("st");
		if (!writeRefsDefinition()) {
			return false;
		}

		source_->popNamespace().newline().pushNamespace("style");
		if (isPalette_) {
			writeSetPaletteColor();
		}
		source_->pushNamespace("internal").newline();
		if (!writeVariableInit()) {
			return false;
		}
	}

	return source_->finalize();
}

// Empty result means an error.
QString Generator::typeToString(structure::Type type) const {
	switch (type.tag) {
	case Tag::Invalid: return QString();
	case Tag::Int: return "int";
	case Tag::Double: return "double";
	case Tag::Pixels: return "int";
	case Tag::String: return "QString";
	case Tag::Color: return "style::color";
	case Tag::Point: return "style::point";
	case Tag::Size: return "style::size";
	case Tag::Align: return "style::align";
	case Tag::Margins: return "style::margins";
	case Tag::Font: return "style::font";
	case Tag::Icon: return "style::icon";
	case Tag::Struct: return "style::" + type.name.back();
	}
	return QString();
}

// Empty result means an error.
QString Generator::typeToDefaultValue(structure::Type type) const {
	switch (type.tag) {
	case Tag::Invalid: return QString();
	case Tag::Int: return "0";
	case Tag::Double: return "0.";
	case Tag::Pixels: return "0";
	case Tag::String: return "QString()";
	case Tag::Color: return "{ Qt::Uninitialized }";
	case Tag::Point: return "{ 0, 0 }";
	case Tag::Size: return "{ 0, 0 }";
	case Tag::Align: return "style::al_topleft";
	case Tag::Margins: return "{ 0, 0, 0, 0 }";
	case Tag::Font: return "{ Qt::Uninitialized }";
	case Tag::Icon: return "{ Qt::Uninitialized }";
	case Tag::Struct: {
		if (auto realType = module_.findStruct(type.name)) {
			QStringList fields;
			for (auto field : realType->fields) {
				fields.push_back(typeToDefaultValue(field.type));
			}
			return "{ " + fields.join(", ") + " }";
		}
		return QString();
	}
	}
	return QString();
}

// Empty result means an error.
QString Generator::valueAssignmentCode(structure::Value value) const {
	auto copy = value.copyOf();
	if (!copy.isEmpty()) {
		return "st::" + copy.back();
	}

	switch (value.type().tag) {
	case Tag::Invalid: return QString();
	case Tag::Int: return QString("%1").arg(value.Int());
	case Tag::Double: return QString("%1").arg(value.Double());
	case Tag::Pixels: return pxValueName(value.Int());
	case Tag::String: return QString("QString::fromUtf8(%1)").arg(stringToEncodedString(value.String()));
	case Tag::Color: {
		auto v(value.Color());
		if (v.red == v.green && v.red == v.blue && v.red == 0 && v.alpha == 255) {
			return QString("st::windowFg");
		} else if (v.red == v.green && v.red == v.blue && v.red == 255 && v.alpha == 0) {
			return QString("st::transparent");
		} else {
			common::logError(common::kErrorInternal, "") << "bad color value";
			return QString();
		}
	} break;
	case Tag::Point: {
		auto v(value.Point());
		return QString("{ %1, %2 }").arg(pxValueName(v.x)).arg(pxValueName(v.y));
	} break;
	case Tag::Size: {
		auto v(value.Size());
		return QString("{ %1, %2 }").arg(pxValueName(v.width)).arg(pxValueName(v.height));
	} break;
	case Tag::Align: return QString("style::al_%1").arg(value.String().c_str());
	case Tag::Margins: {
		auto v(value.Margins());
		return QString("{ %1, %2, %3, %4 }")
		    .arg(pxValueName(v.left))
		    .arg(pxValueName(v.top))
		    .arg(pxValueName(v.right))
		    .arg(pxValueName(v.bottom));
	} break;
	case Tag::Font: {
		auto v(value.Font());
		QString family = "0";
		if (!v.family.empty()) {
			auto familyIndex = fontFamilies_.value(v.family, -1);
			if (familyIndex < 0) {
				return QString();
			}
			family = QString("font%1index").arg(familyIndex);
		}
		return QString("{ %1, %2, %3 }").arg(pxValueName(v.size)).arg(v.flags).arg(family);
	} break;
	case Tag::Icon: {
		auto v(value.Icon());
		if (v.parts.empty()) return QString("{}");

		QStringList parts;
		for (const auto &part : v.parts) {
			auto maskIndex = iconMasks_.value(part.filename, -1);
			if (maskIndex < 0) {
				return QString();
			}
			auto color = valueAssignmentCode(part.color);
			auto offset = valueAssignmentCode(part.offset);
			parts.push_back(QString("MonoIcon{ &iconMask%1, %2, %3 }").arg(maskIndex).arg(color).arg(offset));
		}
		return QString("{ %1 }").arg(parts.join(", "));
	} break;
	case Tag::Struct: {
		if (!value.Fields()) return QString();

		QStringList fields;
		for (auto field : *value.Fields()) {
			fields.push_back(valueAssignmentCode(field.variable.value));
		}
		return "{ " + fields.join(", ") + " }";
	}
	}
	return QString();
}

bool Generator::writeHeaderStyleNamespace() {
	if (!module_.hasStructs() && !module_.hasVariables()) {
		return true;
	}
	header_->pushNamespace("style");

	if (module_.hasVariables()) {
		header_->pushNamespace("internal").newline();
		header_->stream() << "void init_" << baseName_ << "();\n\n";
		header_->popNamespace();
	}
	bool wroteForwardDeclarations = writeStructsForwardDeclarations();
	if (module_.hasStructs()) {
		if (!wroteForwardDeclarations) {
			header_->newline();
		}
		if (!writeStructsDefinitions()) {
			return false;
		}
	} else if (isPalette_) {
		if (!wroteForwardDeclarations) {
			header_->newline();
		}
		if (!writePaletteDefinition()) {
			return false;
		}
	}

	header_->popNamespace().newline();
	return true;
}

bool Generator::writePaletteDefinition() {
	header_->stream() << R"code(class palette {
public:
	palette() = default;
	palette(const palette &other) = delete;

	QByteArray save() const;
	bool load(const QByteArray &cache);

	enum class SetResult {
		Ok,
		KeyNotFound,
		ValueNotFound,
		Duplicate,
	};
	SetResult setColor(QLatin1String name, uchar r, uchar g, uchar b, uchar a);
	SetResult setColor(QLatin1String name, QLatin1String from);
	void reset() {
		clear();
		finalize();
	}

	// Created not inited, should be finalized before usage.
	void finalize();

	int indexOfColor(color c) const;
	color colorAtIndex(int index) const;

	inline const color &get_transparent() const { return _colors[0]; }; // special color
)code";

	int indexInPalette = 1;
	if (!module_.enumVariables([this, &indexInPalette](const Variable &variable) -> bool {
		    auto name = variable.name.back();
		    if (variable.value.type().tag != structure::TypeTag::Color) {
			    return false;
		    }

		    auto index = (indexInPalette++);
		    header_->stream() << "\tinline const color &get_" << name << "() const { return _colors[" << index
		                      << "]; };\n";
		    return true;
	    }))
		return false;

	auto count = indexInPalette;
	header_->stream() << R"code(
	palette &operator=(const palette &other) {
		auto wasReady = _ready;
		for (int i = 0; i != kCount; ++i) {
			if (other._status[i] == Status::Loaded) {
				if (_status[i] == Status::Initial) {
					new (data(i)) internal::ColorData(*other.data(i));
				} else {
					*data(i) = *other.data(i);
				}
			} else if (_status[i] != Status::Initial) {
				data(i)->~ColorData();
				_status[i] = Status::Initial;
				_ready = false;
			}
		}
		if (wasReady && !_ready) {
			finalize();
		}
		return *this;
	}

	static qint32 Checksum();

	~palette() {
		clear();
	}

private:
	static constexpr auto kCount = )code"
	                  << count << R"code(;

	void clear() {
		for (int i = 0; i != kCount; ++i) {
			if (_status[i] != Status::Initial) {
				data(i)->~ColorData();
				_status[i] = Status::Initial;
				_ready = false;
			}
		}
	}

	struct TempColorData { uchar r, g, b, a; };
	void compute(int index, int fallbackIndex, TempColorData value) {
		if (_status[index] == Status::Initial) {
			if (fallbackIndex >= 0 && _status[fallbackIndex] == Status::Loaded) {
				_status[index] = Status::Loaded;
				new (data(index)) internal::ColorData(*data(fallbackIndex));
			} else {
				_status[index] = Status::Created;
				new (data(index)) internal::ColorData(value.r, value.g, value.b, value.a);
			}
		}
	}

	internal::ColorData *data(int index) {
		return reinterpret_cast<internal::ColorData*>(_data) + index;
	}

	const internal::ColorData *data(int index) const {
		return reinterpret_cast<const internal::ColorData*>(_data) + index;
	}

	void setData(int index, const internal::ColorData &value) {
		if (_status[index] == Status::Initial) {
			new (data(index)) internal::ColorData(value);
		} else {
			*data(index) = value;
		}
		_status[index] = Status::Loaded;
	}

	enum class Status {
		Initial,
		Created,
		Loaded,
	};

	alignas(alignof(internal::ColorData)) char _data[sizeof(internal::ColorData) * kCount];

	color _colors[kCount] = {
)code";
	for (int i = 0; i != count; ++i) {
		header_->stream() << "\t\tdata(" << i << "),\n";
	}
	header_->stream() << R"code(	};
	Status _status[kCount] = { Status::Initial };
	bool _ready = false;

};

namespace main_palette {

QByteArray save();
bool load(const QByteArray &cache);
palette::SetResult setColor(QLatin1String name, uchar r, uchar g, uchar b, uchar a);
palette::SetResult setColor(QLatin1String name, QLatin1String from);
void apply(const palette &other);
void reset();
int indexOfColor(color c);

struct row {
	QLatin1String name;
	QLatin1String value;
	QLatin1String fallback;
	QLatin1String description;
};
QList<row> data();

} // namespace main_palette
)code";

	return true;
}

bool Generator::writeStructsForwardDeclarations() {
	bool hasNoExternalStructs = module_.enumVariables([this](const Variable &value) -> bool {
		if (value.value.type().tag == structure::TypeTag::Struct) {
			if (!module_.findStructInModule(value.value.type().name, module_)) {
				return false;
			}
		}
		return true;
	});
	if (hasNoExternalStructs) {
		return false;
	}

	header_->newline();
	bool result = module_.enumVariables([this](const Variable &value) -> bool {
		if (value.value.type().tag == structure::TypeTag::Struct) {
			if (!module_.findStructInModule(value.value.type().name, module_)) {
				header_->stream() << "struct " << value.value.type().name.back() << ";\n";
			}
		}
		return true;
	});
	header_->newline();
	return result;
}

bool Generator::writeStructsDefinitions() {
	if (!module_.hasStructs()) {
		return true;
	}

	bool result = module_.enumStructs([this](const Struct &value) -> bool {
		header_->stream() << "\
struct " << value.name.back()
		                  << " {\n";
		for (auto &field : value.fields) {
			auto type = typeToString(field.type);
			if (type.isEmpty()) {
				return false;
			}
			header_->stream() << "\t" << type << " " << field.name.back() << ";\n";
		}
		header_->stream() << "\
};\n\n";
		return true;
	});

	return result;
}

bool Generator::writeRefsDeclarations() {
	if (!module_.hasVariables()) {
		return true;
	}

	header_->pushNamespace("st");

	if (isPalette_) {
		header_->stream() << "extern const style::color &transparent; // special color\n";
	}
	bool result = module_.enumVariables([this](const Variable &value) -> bool {
		auto name = value.name.back();
		auto type = typeToString(value.value.type());
		if (type.isEmpty()) {
			return false;
		}

		header_->stream() << "extern const " << type << " &" << name << ";\n";
		return true;
	});

	header_->popNamespace();

	return result;
}

bool Generator::writeIncludesInSource() {
	if (!module_.hasIncludes()) {
		return true;
	}

	auto includes = QStringList();
	std::function<bool(const Module &)> collector = [&collector, &includes](const Module &module) {
		module.enumIncludes(collector);
		auto base = moduleBaseName(module);
		if (!includes.contains(base)) {
			includes.push_back(base);
		}
		return true;
	};
	auto result = module_.enumIncludes(collector);
	for (auto base : includes) {
		source_->include(base + ".h");
	}
	source_->newline();
	return result;
}

bool Generator::writeVariableDefinitions() {
	if (!module_.hasVariables()) {
		return true;
	}

	source_->newline();
	bool result = module_.enumVariables([this](const Variable &variable) -> bool {
		auto name = variable.name.back();
		auto type = typeToString(variable.value.type());
		if (type.isEmpty()) {
			return false;
		}
		source_->stream() << type << " _" << name << " = " << typeToDefaultValue(variable.value.type()) << ";\n";
		return true;
	});
	return result;
}

bool Generator::writeRefsDefinition() {
	if (!module_.hasVariables()) {
		return true;
	}

	if (isPalette_) {
		source_->stream() << "const style::color &transparent(_palette.get_transparent()); // special color\n";
	}
	bool result = module_.enumVariables([this](const Variable &variable) -> bool {
		auto name = variable.name.back();
		auto type = typeToString(variable.value.type());
		if (type.isEmpty()) {
			return false;
		}
		source_->stream() << "const " << type << " &" << name << "(";
		if (isPalette_) {
			source_->stream() << "_palette.get_" << name << "()";
		} else {
			source_->stream() << "_" << name;
		}
		source_->stream() << ");\n";
		return true;
	});
	return result;
}

bool Generator::writeSetPaletteColor() {
	source_->newline();
	source_->stream() << R"code(
int palette::indexOfColor(style::color c) const {
	auto start = data(0);
	if (c._data >= start && c._data < start + kCount) {
		return static_cast<int>(c._data - start);
	}
	return -1;
}

color palette::colorAtIndex(int index) const {
	Assert(_ready);
	Assert(index >= 0 && index < kCount);
	return _colors[index];
}

void palette::finalize() {
	if (_ready) return;
	_ready = true;

	compute(0, -1, { 255, 255, 255, 0}); // special color
)code";

	QList<structure::FullName> names;
	module_.enumVariables([&names](const Variable &variable) -> bool {
		names.push_back(variable.name);
		return true;
	});

	QString dataRows;
	int indexInPalette = 1;
	QByteArray checksumString;
	checksumString.append("&transparent:{ 255, 255, 255, 0 }");
	auto result = module_.enumVariables(
	    [this, &indexInPalette, &checksumString, &dataRows, &names](const Variable &variable) -> bool {
		    auto name = variable.name.back();
		    auto index = indexInPalette++;
		    paletteIndices_.emplace(name, index);
		    if (variable.value.type().tag != structure::TypeTag::Color) {
			    return false;
		    }
		    auto color = variable.value.Color();
		    auto fallbackIterator = paletteIndices_.find(colorFallbackName(variable.value));
		    auto fallbackIndex = (fallbackIterator == paletteIndices_.end()) ? -1 : fallbackIterator->second;
		    auto assignment =
		        QString("{ %1, %2, %3, %4 }").arg(color.red).arg(color.green).arg(color.blue).arg(color.alpha);
		    source_->stream() << "\tcompute(" << index << ", " << fallbackIndex << ", " << assignment << ");\n";
		    checksumString.append('&' + name + ':' + assignment);

		    auto isCopy = !variable.value.copyOf().isEmpty();
		    auto colorString = paletteColorValue(color);
		    auto fallbackName = QString();
		    if (fallbackIndex > 0) {
			    auto fallbackVariable = module_.findVariableInModule(names[fallbackIndex - 1], module_);
			    if (fallbackVariable && fallbackVariable->value.type().tag == structure::TypeTag::Color) {
				    fallbackName = fallbackVariable->name.back();
			    }
		    }
		    auto value = isCopy ? fallbackName : '#' + colorString;
		    if (value.isEmpty()) {
			    return false;
		    }

		    dataRows.append("\tresult.push_back({ qstr(\"" + name + "\"), qstr(\"" + value + "\"), qstr(\"" +
		                    (isCopy ? QString() : fallbackName) + "\"), qstr(" +
		                    stringToEncodedString(variable.description.toStdString()) + ") });\n");
		    return true;
	    });
	if (!result) {
		return false;
	}
	auto count = indexInPalette;
	auto checksum = hashCrc32(checksumString.constData(), checksumString.size());

	source_->stream() << R"code(}

qint32 palette::Checksum() {
	return )code" << checksum
	                  << ";\n\
}\n";

	source_->newline().pushNamespace().newline();

	source_->stream() << R"code(const std::map<std::string, int> PaletteMap = {
)code";
	for (auto &ind : paletteIndices_) {
		source_->stream() << "{\"" << ind.first << "\"," << ind.second << "},\n";
	}
	source_->stream() << "\
\n\
};";

	source_->stream() << R"code(

int getPaletteIndex(QLatin1String name) {
	auto data = name.data();
    return PaletteMap.find(data) != PaletteMap.end() ? PaletteMap.at(data) : -1;
}
)code";

	source_->newline().popNamespace().newline();
	source_->stream() << R"code(QByteArray palette::save() const {
	if (!_ready) const_cast<palette*>(this)->finalize();

	auto result = QByteArray()code"
	                  << (count * 4) << R"code(, Qt::Uninitialized);
	for (auto i = 0, index = 0; i != )code"
	                  << count << R"code(; ++i) {
		result[index++] = static_cast<uchar>(data(i)->c.red());
		result[index++] = static_cast<uchar>(data(i)->c.green());
		result[index++] = static_cast<uchar>(data(i)->c.blue());
		result[index++] = static_cast<uchar>(data(i)->c.alpha());
	}
	return result;
}

bool palette::load(const QByteArray &cache) {
	if (cache.size() != )code"
	                  << (count * 4) << R"code() return false;

	auto p = reinterpret_cast<const uchar*>(cache.constData());
	for (auto i = 0; i != )code"
	                  << count << R"code(; ++i) {
		setData(i, { p[i * 4 + 0], p[i * 4 + 1], p[i * 4 + 2], p[i * 4 + 3] });
	}
	return true;
}

palette::SetResult palette::setColor(QLatin1String name, uchar r, uchar g, uchar b, uchar a) {
	auto nameIndex = getPaletteIndex(name);
	if (nameIndex < 0) return SetResult::KeyNotFound;
	auto duplicate = (_status[nameIndex] != Status::Initial);

	setData(nameIndex, { r, g, b, a });
	return duplicate ? SetResult::Duplicate : SetResult::Ok;
}

palette::SetResult palette::setColor(QLatin1String name, QLatin1String from) {
	auto nameIndex = getPaletteIndex(name);
	if (nameIndex < 0) return SetResult::KeyNotFound;
	auto duplicate = (_status[nameIndex] != Status::Initial);

	auto fromIndex = getPaletteIndex(from);
	if (fromIndex < 0 || _status[fromIndex] != Status::Loaded) return SetResult::ValueNotFound;

	setData(nameIndex, *data(fromIndex));
	return duplicate ? SetResult::Duplicate : SetResult::Ok;
}

namespace main_palette {

QByteArray save() {
	return _palette.save();
}

bool load(const QByteArray &cache) {
	if (_palette.load(cache)) {
		style::internal::resetIcons();
		return true;
	}
	return false;
}

palette::SetResult setColor(QLatin1String name, uchar r, uchar g, uchar b, uchar a) {
	return _palette.setColor(name, r, g, b, a);
}

palette::SetResult setColor(QLatin1String name, QLatin1String from) {
	return _palette.setColor(name, from);
}

void apply(const palette &other) {
	_palette = other;
	style::internal::resetIcons();
}

void reset() {
	_palette.reset();
	style::internal::resetIcons();
}

int indexOfColor(color c) {
	return _palette.indexOfColor(c);
}

QList<row> data() {
	auto result = QList<row>();
	result.reserve()code"
	                  << count // TODO(Randl)
	                  << R"code();

)code" << dataRows << R"code(
	return result;
}

} // namespace main_palette

)code";

	return result;
}

bool Generator::writeVariableInit() {
	if (!module_.hasVariables()) {
		return true;
	}

	if (!collectUniqueValues()) {
		return false;
	}
	bool hasUniqueValues = (!pxValues_.isEmpty() || !fontFamilies_.isEmpty() || !iconMasks_.isEmpty());
	if (hasUniqueValues) {
		source_->pushNamespace();
		if (!writePxValuesInit()) {
			return false;
		}
		if (!writeFontFamiliesInit()) {
			return false;
		}
		if (!writeIconValues()) {
			return false;
		}
		source_->popNamespace().newline();
	}

	source_->stream() << "\
void init_" << baseName_
	                  << R"code(() {
	if (inited) return;
	inited = true;

)code";

	if (module_.hasIncludes()) {
		bool writtenAtLeastOne = false;
		bool result = module_.enumIncludes([this, &writtenAtLeastOne](const Module &module) -> bool {
			if (module.hasVariables()) {
				source_->stream() << "\tinit_" + moduleBaseName(module) + "();\n";
				writtenAtLeastOne = true;
			}
			return true;
		});
		if (!result) {
			return false;
		}
		if (writtenAtLeastOne) {
			source_->newline();
		}
	}

	if (!pxValues_.isEmpty() || !fontFamilies_.isEmpty()) {
		if (!pxValues_.isEmpty()) {
			source_->stream() << "\tinitPxValues();\n";
		}
		if (!fontFamilies_.isEmpty()) {
			source_->stream() << "\tinitFontFamilies();\n";
		}
		source_->newline();
	}

	if (isPalette_) {
		source_->stream() << "\t_palette.finalize();\n";
	} else if (!module_.enumVariables([this](const Variable &variable) -> bool {
		           auto name = variable.name.back();
		           auto value = valueAssignmentCode(variable.value);
		           if (value.isEmpty()) {
			           return false;
		           }
		           source_->stream() << "\t_" << name << " = " << value << ";\n";
		           return true;
	           })) {
		return false;
	}
	source_->stream() << "\
}\n\n";
	return true;
}

bool Generator::writePxValuesInit() {
	if (pxValues_.isEmpty()) {
		return true;
	}

	for (auto i = pxValues_.cbegin(), e = pxValues_.cend(); i != e; ++i) {
		source_->stream() << "int " << pxValueName(i.key()) << " = " << i.key() << ";\n";
	}
	source_->stream() << R"code(void initPxValues() {
	if (cRetina()) return;

	switch (cScale()) {
)code";
	for (int i = 1, scalesCount = _scales.size(); i < scalesCount; ++i) {
		source_->stream() << "\tcase " << _scaleNames.at(i) << ":\n";
		for (auto it = pxValues_.cbegin(), e = pxValues_.cend(); it != e; ++it) {
			auto value = it.key();
			int adjusted = structure::data::pxAdjust(value, _scales.at(i));
			if (adjusted != value) {
				source_->stream() << "\t\t" << pxValueName(value) << " = " << adjusted << ";\n";
			}
		}
		source_->stream() << "\tbreak;\n";
	}
	source_->stream() << "\
	}\n\
}\n\n";
	return true;
}

bool Generator::writeFontFamiliesInit() {
	if (fontFamilies_.isEmpty()) {
		return true;
	}

	for (auto familyIndex : fontFamilies_) {
		source_->stream() << "int font" << familyIndex << "index;\n";
	}
	source_->stream() << "void initFontFamilies() {\n";
	for (auto i = fontFamilies_.cbegin(), e = fontFamilies_.cend(); i != e; ++i) {
		auto family = stringToEncodedString(i.key());
		source_->stream() << "\tfont" << i.value() << "index = style::internal::registerFontFamily(" << family
		                  << ");\n";
	}
	source_->stream() << "}\n\n";
	return true;
}

namespace {

QByteArray iconMaskValueSize(int width, int height) {
	QByteArray result;
	QLatin1String generateTag("GENERATE:");
	result.append(generateTag.data(), generateTag.size());
	QLatin1String sizeTag("SIZE:");
	result.append(sizeTag.data(), sizeTag.size());
	{
		QDataStream stream(&result, QIODevice::Append);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << qint32(width) << qint32(height);
	}
	return result;
}

QByteArray iconMaskValuePng(QString filepath) {
	QByteArray result;

	QFileInfo fileInfo(filepath);
	auto directory = fileInfo.dir();
	auto nameAndModifiers = fileInfo.fileName().split('-');
	filepath = directory.filePath(nameAndModifiers[0]);
	auto modifiers = nameAndModifiers.mid(1);

	QImage png100x(filepath + ".png");
	QImage png200x(filepath + "@2x.png");
	png100x.setDevicePixelRatio(1.);
	png200x.setDevicePixelRatio(1.);
	if (png100x.isNull()) {
		common::logError(common::kErrorFileNotOpened, filepath + ".png") << "could not open icon file";
		return result;
	}
	if (png200x.isNull()) {
		common::logError(common::kErrorFileNotOpened, filepath + "@2x.png") << "could not open icon file";
		return result;
	}
	if (png100x.format() != png200x.format()) {
		common::logError(kErrorBadIconFormat, filepath + ".png") << "1x and 2x icons have different format";
		return result;
	}
	if (png100x.width() * 2 != png200x.width() || png100x.height() * 2 != png200x.height()) {
		common::logError(kErrorBadIconSize, filepath + ".png")
		    << "bad icons size, 1x: " << png100x.width() << "x" << png100x.height() << ", 2x: " << png200x.width()
		    << "x" << png200x.height();
		return result;
	}
	for (auto modifierName : modifiers) {
		if (auto modifier = GetModifier(modifierName)) {
			modifier(png100x, png200x);
		} else {
			common::logError(common::kErrorInternal, filepath)
			    << "modifier should be valid here, name: " << modifierName.toStdString();
			return result;
		}
	}
	QImage png125x =
	    png200x.scaled(structure::data::pxAdjust(png100x.width(), 5), structure::data::pxAdjust(png100x.height(), 5),
	                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QImage png150x =
	    png200x.scaled(structure::data::pxAdjust(png100x.width(), 6), structure::data::pxAdjust(png100x.height(), 6),
	                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	QImage composed(png200x.width() + png100x.width(), png200x.height() + png150x.height(), png100x.format());
	{
		QPainter p(&composed);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.fillRect(0, 0, composed.width(), composed.height(), QColor(0, 0, 0, 255));
		p.drawImage(0, 0, png200x);
		p.drawImage(png200x.width(), 0, png100x);
		p.drawImage(0, png200x.height(), png150x);
		p.drawImage(png150x.width(), png200x.height(), png125x);
	}
	{
		QBuffer buffer(&result);
		composed.save(&buffer, "PNG");
	}
	return result;
}

} // namespace

bool Generator::writeIconValues() {
	if (iconMasks_.isEmpty()) {
		return true;
	}

	for (auto i = iconMasks_.cbegin(), e = iconMasks_.cend(); i != e; ++i) {
		QString filePath = i.key();
		QByteArray maskData;
		QImage png100x, png200x;
		if (filePath.startsWith("size://")) {
			QStringList dimensions = filePath.mid(7).split(',');
			if (dimensions.size() < 2 || dimensions.at(0).toInt() <= 0 || dimensions.at(1).toInt() <= 0) {
				common::logError(common::kErrorFileNotOpened, filePath) << "bad dimensions";
				return false;
			}
			maskData = iconMaskValueSize(dimensions.at(0).toInt(), dimensions.at(1).toInt());
		} else {
			maskData = iconMaskValuePng(filePath);
		}
		if (maskData.isEmpty()) {
			return false;
		}
		source_->stream() << "const uchar iconMask" << i.value()
		                  << "Data[] = " << stringToBinaryArray(std::string(maskData.constData(), maskData.size()))
		                  << ";\n";
		source_->stream() << "IconMask iconMask" << i.value() << "(iconMask" << i.value() << "Data);\n\n";
	}
	return true;
}

bool Generator::collectUniqueValues() {
	int fontFamilyIndex = 0;
	int iconMaskIndex = 0;
	std::function<bool(const Variable &)> collector = [this, &collector, &fontFamilyIndex,
	                                                   &iconMaskIndex](const Variable &variable) {
		auto value = variable.value;
		if (!value.copyOf().isEmpty()) {
			return true;
		}

		switch (value.type().tag) {
		case Tag::Invalid:
		case Tag::Int:
		case Tag::Double:
		case Tag::String:
		case Tag::Color:
		case Tag::Align: break;
		case Tag::Pixels: pxValues_.insert(value.Int(), true); break;
		case Tag::Point: {
			auto v(value.Point());
			pxValues_.insert(v.x, true);
			pxValues_.insert(v.y, true);
		} break;
		case Tag::Size: {
			auto v(value.Size());
			pxValues_.insert(v.width, true);
			pxValues_.insert(v.height, true);
		} break;
		case Tag::Margins: {
			auto v(value.Margins());
			pxValues_.insert(v.left, true);
			pxValues_.insert(v.top, true);
			pxValues_.insert(v.right, true);
			pxValues_.insert(v.bottom, true);
		} break;
		case Tag::Font: {
			auto v(value.Font());
			pxValues_.insert(v.size, true);
			if (!v.family.empty() && !fontFamilies_.contains(v.family)) {
				fontFamilies_.insert(v.family, ++fontFamilyIndex);
			}
		} break;
		case Tag::Icon: {
			auto v(value.Icon());
			for (auto &part : v.parts) {
				pxValues_.insert(part.offset.Point().x, true);
				pxValues_.insert(part.offset.Point().y, true);
				if (!iconMasks_.contains(part.filename)) {
					iconMasks_.insert(part.filename, ++iconMaskIndex);
				}
			}
		} break;
		case Tag::Struct: {
			auto fields = variable.value.Fields();
			if (!fields) {
				return false;
			}

			for (auto field : *fields) {
				if (!collector(field.variable)) {
					return false;
				}
			}
		} break;
		}
		return true;
	};
	return module_.enumVariables(collector);
}

} // namespace style
} // namespace codegen
