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
