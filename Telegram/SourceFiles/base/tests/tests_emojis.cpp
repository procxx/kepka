/// @file tests_emojis.cpp Tests several emojis for making sure that it
/// actually find required emoji.
#include "emoji_suggestions.h"
#include "emoji_suggestions_data.h"
#include <cassert>
#include <iostream>
#include <QString>
#include <QVector>
#include <QPair>
#include <QMap>
#include <string>
#include "ui/emoji_config.h"

int main()
{
    auto str = std::u16string(u":cat:");
    auto vec = Ui::Emoji::internal::GetReplacements(str[1]); // needs for init internal emoji table.
    assert(vec != nullptr);
    auto emoji = Ui::Emoji::internal::GetReplacementEmoji(str);
    auto cat_u16 = std::u16string(u"🐱");
    auto cat_w = std::wstring(L"🐱");
    auto cat_u8 = std::string(u8"🐱"); // u8string in c++20.
    assert(cat_u16[0] == 0xd83d); // assert that compiler correctly sets charset.
    assert(cat_w[0] == 0xd83d);   // ...in all available charsets.
    assert(emoji == cat_u16);     // simple test that :cat: is really 🐱
    assert(u"☠️" == Ui::Emoji::internal::GetReplacementEmoji(u":skull_and_crossbones:")); // ditto for ☠️
    return 0;
}

// stubs for linking (because we do not want to include whole tdesktop)

DBIScale gRealScale;
DBIScale gScreenScale;
bool gRetina;
QVector <QPair<const Ui::Emoji::One *, unsigned short>> gRecentEmoji;
QVector <QPair<QString, unsigned short>> gRecentEmojiPreload;
QMap<QString, int> gEmojiVariants;

namespace SignalHandlers {
void setCrashAnnotation(std::string const &, QString const &) {}
}

namespace Logs {
void writeMain(QString const &message) {
    std::cout << message.toStdString();
}
}
