#include "chatSettings.h"

#include <vector>

ChatSettingsDialog::ChatSettingsDialog(QMainWindow *parent)
    :QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    mMainWin = (MainWindow *) parent;
    ui->setupUi(this);

#ifndef KARERE_DISABLE_WEBRTC
    ::mega::MegaStringList *videoInDevices = mMainWin->mMegaChatApi->getChatVideoInDevices();
    char *videoDeviceSelected = mMainWin->mMegaChatApi->getVideoDeviceSelected();
    for (int i = 0; i < videoInDevices->size(); i++)
    {
        ui->videoInCombo->addItem(videoInDevices->get(i), videoInDevices->get(i));
    }

    int index = ui->videoInCombo->findData(videoDeviceSelected);
    ui->videoInCombo->setCurrentIndex(index);

    delete videoInDevices;
    delete []videoDeviceSelected;
#endif
}

ChatSettingsDialog::~ChatSettingsDialog()
{
    delete ui;
}

void ChatSettingsDialog::on_buttonBox_clicked(QAbstractButton *button)
{
#ifndef KARERE_DISABLE_WEBRTC
    std::string video = ui->videoInCombo->itemText(ui->videoInCombo->currentIndex()).toLatin1().data();
    setDevices(video);

#endif
}

#ifndef KARERE_DISABLE_WEBRTC
void ChatSettingsDialog::setDevices(const std::string &video)
{
    std::string device =  ui->videoInCombo->itemText(ui->videoInCombo->currentIndex()).toLatin1().data();
    char *videoDeviceSelected = mMainWin->mMegaChatApi->getVideoDeviceSelected();
    if (device != videoDeviceSelected)
    {
        mMainWin->mMegaChatApi->setChatVideoInDevice(video.c_str());
    }

    delete []videoDeviceSelected;
}
#endif
