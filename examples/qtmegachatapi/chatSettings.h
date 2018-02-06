#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H
#include <QDialog>
#include "MainWindow.h"
#include <ui_settingsDialog.h>

namespace Ui
{
    class SettingsDialog;
    class MainWindow;
}

class ChatSettings : public QDialog
{
    Q_OBJECT
    public:
        ChatSettings(QMainWindow *parent);
        virtual ~ChatSettings();
        void applySettings();

    protected:
       Ui::SettingsDialog *ui;
       //MainWindow & mMainWindow;
       /*
       #ifndef KARERE_DISABLE_WEBRTC
            void selectVideoInput();
            void selectAudioInput();
            int mAudioInIdx;
            int mVideoInIdx;
       #endif
       */
};
#endif // SETTINGSDIALOG_H
