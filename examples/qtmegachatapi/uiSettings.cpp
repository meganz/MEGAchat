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
    QString::fromUtf8("\xF0\x9F\x98\x89")
};
