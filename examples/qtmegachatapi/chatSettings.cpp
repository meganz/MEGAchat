#include "chatSettings.h"

#include <vector>

ChatSettingsDialog::ChatSettingsDialog(QMainWindow *parent)
    :QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    mMainWin = (MainWindow *) parent;
    ui->setupUi(this);

#ifndef KARERE_DISABLE_WEBRTC
    std::unique_ptr<::mega::MegaStringList> videoInDevices(mMainWin->mMegaChatApi->getChatVideoInDevices());
    std::unique_ptr<char[]> videoDeviceSelected(mMainWin->mMegaChatApi->getCameraDeviceIdSelected());
    for (int i = 0; i < videoInDevices->size(); i++)
    {
        ui->videoInCombo->addItem(videoInDevices->get(i), videoInDevices->get(i));
    }

    int index = ui->videoInCombo->findData(videoDeviceSelected.get());
    ui->videoInCombo->setCurrentIndex(index);
#endif
}

ChatSettingsDialog::~ChatSettingsDialog()
{
    delete ui;
}

void ChatSettingsDialog::on_buttonBox_clicked(QAbstractButton*)
{
#ifndef KARERE_DISABLE_WEBRTC
    setDevices(ui->videoInCombo->itemText(ui->videoInCombo->currentIndex()).toLatin1().data());
#endif
}

#ifndef KARERE_DISABLE_WEBRTC
void ChatSettingsDialog::setDevices(const std::string &videoDevice)
{
    std::unique_ptr<char[]> videoDeviceSelected(mMainWin->mMegaChatApi->getCameraDeviceIdSelected());
    if (videoDevice != videoDeviceSelected.get())
    {
        mMainWin->mMegaChatApi->setCameraInDevice(videoDevice.c_str());
    }
}
#endif
