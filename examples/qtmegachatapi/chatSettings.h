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
class ChatSettings
{
    public:
        ChatSettings();
        virtual ~ChatSettings();
        int getAudioInIdx() const;
        void setAudioInIdx(int audioInIdx);
        int getVideoInIdx() const;
        void setVideoInIdx(int videoInIdx);

    private:
        int mAudioInIdx;
        int mVideoInIdx;
};

class ChatSettingsDialog : public QDialog
{
    Q_OBJECT
    public:
        ChatSettingsDialog(QMainWindow *parent, ChatSettings *chatSettings);
        virtual ~ChatSettingsDialog();
        void applySettings();

    protected:
        MainWindow *mMainWin;
        Ui::SettingsDialog *ui;
        ChatSettings *mChatSettings;
        #ifndef KARERE_DISABLE_WEBRTC
            void setDevices();
        #endif
    private slots:
            void on_buttonBox_clicked(QAbstractButton *button);
};
#endif // SETTINGSDIALOG_H
