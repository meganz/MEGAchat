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
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 80")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 81")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 82")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 83")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 84")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 85")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 86")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 87")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 88")),
    QString::fromUtf8(QByteArray::fromHex("F0 9F 98 89"))
};
