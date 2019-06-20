#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H
#include <QDialog>
#include "MainWindow.h"
#include "ui_settingsDialog.h"
const int deviceListInvalidIndex = -1;

namespace Ui
{
    class SettingsDialog;
    class MainWindow;
}
class MainWindow;

class ChatSettingsDialog : public QDialog
{
    Q_OBJECT
    public:
        ChatSettingsDialog(QMainWindow *parent);
        virtual ~ChatSettingsDialog();
        void applySettings();

    protected:
        MainWindow *mMainWin;
        Ui::SettingsDialog *ui;
#ifndef KARERE_DISABLE_WEBRTC
        void setDevices(const std::string &audio, const std::string &video);
#endif
    private slots:
            void on_buttonBox_clicked(QAbstractButton *button);
};
#endif // SETTINGSDIALOG_H
