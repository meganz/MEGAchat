#include "chatSettings.h"

#include <vector>

ChatSettings::ChatSettings(QMainWindow *parent)
    :QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    mMainWin = (MainWindow *) parent;
    ui->setupUi(this);
    #ifndef KARERE_DISABLE_WEBRTC
        mega::MegaStringList *audioInDevices = mMainWin->mMegaChatApi->getChatAudioInDevices();
        for (int i=0; i<audioInDevices->size(); i++)
        {
            ui->audioInCombo->addItem(audioInDevices->get(i));
        }
        delete audioInDevices;
        mega::MegaStringList *videoInDevices = mMainWin->mMegaChatApi->getChatVideoInDevices();
        for (int i=0; i<videoInDevices->size(); i++)
        {
            ui->videoInCombo->addItem(videoInDevices->get(i));
        }
        delete videoInDevices;
    #endif
}

ChatSettings::~ChatSettings()
{
    delete ui;
}
