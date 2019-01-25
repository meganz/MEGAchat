#ifndef SETTINGWINDOW_H
#define SETTINGWINDOW_H

#include <megaapi.h>
#include <mega/types.h>
#include <QDialog>
#include <qstandarditemmodel.h>
#include <QTMegaRequestListener.h>

namespace Ui {
class SettingWindow;
}

class SettingWindow : public QDialog, public mega::MegaRequestListener
{
    Q_OBJECT

public:
    explicit SettingWindow(mega::MegaApi *megaApi, QWidget *parent = 0);
    ~SettingWindow();
    void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e);
    void show();

private:
    Ui::SettingWindow *ui;
    mega::MegaApi *mMegaApi;
    mega::MegaPushNotificationSettings *mPushNotificationSettings = NULL;
    mega::MegaTimeZoneDetails *mTimeZoneDetails = NULL;

    mega::m_time_t mGlobalDifference = -1;
    QStandardItemModel mModel;
    mega::QTMegaRequestListener *mMegaRequestDelegate;

    void fillWidget();

private slots:
    void accepted();
    void rejected();
};

#endif // SETTINGWINDOW_H
