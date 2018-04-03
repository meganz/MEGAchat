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
        for (int i = 0; i < audioInDevices->size(); i++)
        {
            ui->audioInCombo->addItem(audioInDevices->get(i));
        }
        mAudioInIdx = ui->audioInCombo->currentIndex();
        delete audioInDevices;

        mega::MegaStringList *videoInDevices = mMainWin->mMegaChatApi->getChatVideoInDevices();
        for (int i=0; i<videoInDevices->size(); i++)
        {
            ui->videoInCombo->addItem(videoInDevices->get(i));
        }
        mVideoInIdx = ui->videoInCombo->currentIndex();
        delete videoInDevices;
    #endif
}

ChatSettings::~ChatSettings()
{
    delete ui;
}

void ChatSettings::on_buttonBox_clicked(QAbstractButton *button)
{
#ifndef KARERE_DISABLE_WEBRTC
    if (ui->audioInCombo->currentIndex() != mAudioInIdx)
    {
        mAudioInIdx = ui->audioInCombo->currentIndex();
        std::string device =  ui->audioInCombo->itemText(ui->audioInCombo->currentIndex()).toLatin1().data();

        bool result = mMainWin->mMegaChatApi->setChatAudioInDevice(device.c_str());
        if (!result)
        {
            QMessageBox::critical(this, "Call settings", "The audio device could not be set");
        }
    }

    if (ui->videoInCombo->currentIndex() != mVideoInIdx)
    {
        mVideoInIdx = ui->videoInCombo->currentIndex();
        std::string device =  ui->videoInCombo->itemText(ui->videoInCombo->currentIndex()).toLatin1().data();
        bool result = mMainWin->mMegaChatApi->setChatVideoInDevice(device.c_str());
        if (!result)
        {
            QMessageBox::critical(this, "Call settings", "The video device could not be set");
        }
    }
#endif
}
