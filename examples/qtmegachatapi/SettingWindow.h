#ifndef SETTINGWINDOW_H
#define SETTINGWINDOW_H

#include <megaapi.h>
#include <megachatapi.h>
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
    void onPresenceConfigUpdate();

private:
    Ui::SettingWindow *ui;
    MegaChatApplication *mApp;

    // notification settings
    ::mega::m_time_t mGlobalDifference = -1;
    QStandardItemModel mNotificationSettingsPerChat;
    void savePushNotificationSettings();

    // presence config
    ::megachat::MegaChatPresenceConfig *mPresenceConfig = nullptr;
    void savePresenceConfig();

private slots:
    void onClicked(QAbstractButton*);
    void onGlobalClicked(bool value);
    void onScheduleEnabled(bool value);
    void on_autoAwayCheckBox_clicked(bool checked);
};

#endif // SETTINGWINDOW_H
