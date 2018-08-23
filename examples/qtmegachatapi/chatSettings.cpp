#include "chatSettings.h"

#include <vector>

WebRTCSettingsDialog::WebRTCSettingsDialog(QMainWindow *parent, WebRTCSettings *chatSettings)
    :QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    mMainWin = (MainWindow *) parent;
    mChatSettings = chatSettings;
    ui->setupUi(this);

#ifndef KARERE_DISABLE_WEBRTC
    mega::MegaStringList *audioInDevices = mMainWin->mMegaChatApi->getChatAudioInDevices();
    for (int i = 0; i < audioInDevices->size(); i++)
    {
        ui->audioInCombo->addItem(audioInDevices->get(i));
    }

    if(mChatSettings->getAudioInIdx() == deviceListInvalidIndex)
    {
        ui->audioInCombo->setCurrentIndex(0);
    }
    else
    {
        ui->audioInCombo->setCurrentIndex(mChatSettings->getAudioInIdx());
    }
    delete audioInDevices;

    mega::MegaStringList *videoInDevices = mMainWin->mMegaChatApi->getChatVideoInDevices();
    for (int i=0; i<videoInDevices->size(); i++)
    {
        ui->videoInCombo->addItem(videoInDevices->get(i));
    }
    if(mChatSettings->getAudioInIdx() == deviceListInvalidIndex)
    {
        ui->videoInCombo->setCurrentIndex(0);
    }
    else
    {
        ui->videoInCombo->setCurrentIndex(mChatSettings->getVideoInIdx());
    }

    delete videoInDevices;
    setDevices();
#endif
}

WebRTCSettingsDialog::~WebRTCSettingsDialog()
{
    delete ui;
}

void WebRTCSettingsDialog::on_buttonBox_clicked(QAbstractButton *button)
{
#ifndef KARERE_DISABLE_WEBRTC
    if (ui->audioInCombo->currentIndex() != mChatSettings->getAudioInIdx()
            || ui->videoInCombo->currentIndex() != mChatSettings->getVideoInIdx())
    {
        mChatSettings->setAudioInIdx(ui->audioInCombo->currentIndex());
        mChatSettings->setVideoInIdx(ui->videoInCombo->currentIndex());
        setDevices();
    }
#endif
}

#ifndef KARERE_DISABLE_WEBRTC
void WebRTCSettingsDialog::setDevices()
{
    int audioInIdx = ui->audioInCombo->currentIndex();
    if (audioInIdx != deviceListInvalidIndex)
    {
        std::string device =  ui->audioInCombo->itemText(ui->audioInCombo->currentIndex()).toLatin1().data();
        bool result = mMainWin->mMegaChatApi->setChatAudioInDevice(device.c_str());
        if (!result)
        {
            QMessageBox::critical(this, "Call settings", "The audio device could not be set");
        }
    }

    int videoInIdx =  ui->videoInCombo->currentIndex();;
    if (videoInIdx != deviceListInvalidIndex)
    {
        std::string device =  ui->videoInCombo->itemText(ui->videoInCombo->currentIndex()).toLatin1().data();
        bool result = mMainWin->mMegaChatApi->setChatVideoInDevice(device.c_str());
        if (!result)
        {
            QMessageBox::critical(this, "Call settings", "The video device could not be set");
        }
    }
}
#endif

WebRTCSettings::WebRTCSettings()
{
    mAudioInIdx = deviceListInvalidIndex;
    mVideoInIdx = deviceListInvalidIndex;
}

WebRTCSettings::~WebRTCSettings()
{

}

int WebRTCSettings::getAudioInIdx() const
{
    return mAudioInIdx;
}

void WebRTCSettings::setAudioInIdx(int audioInIdx)
{
    mAudioInIdx = audioInIdx;
}

int WebRTCSettings::getVideoInIdx() const
{
    return mVideoInIdx;
}

void WebRTCSettings::setVideoInIdx(int videoInIdx)
{
    mVideoInIdx = videoInIdx;
}
