#include "uiSettings.h"

QChar kOnlineSymbol_InProgress(0x267a);
QChar kOnlineSymbol_Set(0x25cf);

QString gOnlineIndColors[NINDCOLORS] =
{ "black", "lightgray", "orange", "lightgreen", "red" };

QString kOnlineStatusBtnStyle = QStringLiteral(
    u"color: %1;"
    "border: 0px;"
    "border-radius: 2px;"
    "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1,"
        "stop:0 rgba(100,100,100,255),"
        "stop:1 rgba(160,160,160,255));"
);

QColor gAvatarColors[16] = {
    "aliceblue", "gold", "darkseagreen", "crimson",
    "firebrick", "lightsteelblue", "#70a1ff", "maroon",
    "cadetblue", "#db00d3", "darkturquoise", "lightblue",
    "honeydew", "lightyellow", "violet", "turquoise"
};

QStringList utf8reactionsList = {
    QString::fromUtf8("\xF0\x9F\x98\x80"),
    QString::fromUtf8("\xF0\x9F\x98\x81"),
    QString::fromUtf8("\xF0\x9F\x98\x82"),
    QString::fromUtf8("\xF0\x9F\x98\x83"),
    QString::fromUtf8("\xF0\x9F\x98\x84"),
    QString::fromUtf8("\xF0\x9F\x98\x85"),
    QString::fromUtf8("\xF0\x9F\x98\x86"),
    QString::fromUtf8("\xF0\x9F\x98\x87"),
    QString::fromUtf8("\xF0\x9F\x98\x88"),
    QString::fromUtf8("\xF0\x9F\x98\x89"),
    QString::fromUtf8("\xF0\x9F\x98\x90"),
    QString::fromUtf8("\xF0\x9F\x98\x91"),
    QString::fromUtf8("\xF0\x9F\x98\x92"),
    QString::fromUtf8("\xF0\x9F\x98\x93"),
    QString::fromUtf8("\xF0\x9F\x98\x94"),
    QString::fromUtf8("\xF0\x9F\x98\x95"),
    QString::fromUtf8("\xF0\x9F\x98\x96"),
    QString::fromUtf8("\xF0\x9F\x98\x97"),
    QString::fromUtf8("\xF0\x9F\x98\x98"),
    QString::fromUtf8("\xF0\x9F\x98\x99"),
    QString::fromUtf8("\xF0\x9F\x98\xA0"),
    QString::fromUtf8("\xF0\x9F\x98\xA1"),
    QString::fromUtf8("\xF0\x9F\x98\xA2"),
    QString::fromUtf8("\xF0\x9F\x98\xA3"),
    QString::fromUtf8("\xF0\x9F\x98\xA4"),
    QString::fromUtf8("\xF0\x9F\x98\xA5"),
    QString::fromUtf8("\xF0\x9F\x98\xA6"),
    QString::fromUtf8("\xF0\x9F\x98\xA7"),
    QString::fromUtf8("\xF0\x9F\x98\xA8"),
    QString::fromUtf8("\xF0\x9F\x98\xA9"),
    QString::fromUtf8("\xF0\x9F\x98\xB0"),
    QString::fromUtf8("\xF0\x9F\x98\xB1"),
    QString::fromUtf8("\xF0\x9F\x98\xB2"),
    QString::fromUtf8("\xF0\x9F\x98\xB3"),
    QString::fromUtf8("\xF0\x9F\x98\xB4"),
    QString::fromUtf8("\xF0\x9F\x98\xB5"),
    QString::fromUtf8("\xF0\x9F\x98\xB6"),
    QString::fromUtf8("\xF0\x9F\x98\xB7"),
    QString::fromUtf8("\xF0\x9F\x98\xB8"),
    QString::fromUtf8("\xF0\x9F\x98\xB9"),
    QString::fromUtf8("\xF0\x9F\x99\x8F"),
    QString::fromUtf8("\xF0\x9F\x91\x8F"),
    QString::fromUtf8("\xF0\x9F\xA4\xB2"),
    QString::fromUtf8("\xF0\x9F\x91\x8C"),
    QString::fromUtf8("\xF0\x9F\xA4\x9F"),
    QString::fromUtf8("\x31\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x32\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x33\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x34\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x35\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x36\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x37\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x38\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\x39\xEF\xB8\x8F\xE2\x83\xA3"),
    QString::fromUtf8("\xF0\x9F\x94\x9F"),
};
