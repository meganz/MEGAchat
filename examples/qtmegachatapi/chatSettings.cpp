#include "chatSettings.h"
#include "ui_settingsDialog.h"
#include <vector>

//ChatSettings::ChatSettings(MainWindow &parent) :QDialog(&parent), mMainWindow(parent)
ChatSettings::ChatSettings(QMainWindow *parent)
    :QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
}

    /*
    #ifndef KARERE_DISABLE_WEBRTC
    vector<string> audio;
    //mMainWindow.client().rtc->getAudioInDevices(audio);
    for (auto& name: audio)
        ui.audioInCombo->addItem(name.c_str());
    mAudioInIdx = 0;

    vector<string> video;
    //mMainWindow.client().rtc->getVideoInDevices(video);
    for (auto& name: video)
        ui.videoInCombo->addItem(name.c_str());
    mVideoInIdx = 0;
    #endif
    */


ChatSettings::~ChatSettings()
{
    delete ui;
}
