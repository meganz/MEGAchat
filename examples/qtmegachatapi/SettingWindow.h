#ifndef SETTINGWINDOW_H
#define SETTINGWINDOW_H

#include <megaapi.h>
#include <mega/types.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <qstandarditemmodel.h>
#include <QTMegaRequestListener.h>

namespace Ui {
class SettingWindow;
}

class MegaChatApplication;

class SettingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingWindow(MegaChatApplication *app);
    ~SettingWindow();
    void show();
    void onPushNotificationSettingsUpdate();    // update the UI with new values

private:
    Ui::SettingWindow *ui;
    MegaChatApplication *mApp;

    // notification settings
    ::mega::m_time_t mGlobalDifference = -1;
    QStandardItemModel mNotificationSettingsPerChat;

    void savePushNotificationSettings();

private slots:
    void onClicked(QAbstractButton*);
    void onGlobalClicked(bool value);
    void onScheduleEnabled(bool value);
};

#endif // SETTINGWINDOW_H
