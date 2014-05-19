#include "webrtcAdapter.h" //must be before any Qt headers because Qt defines an 'emit' macro which conicides with a method name in webrtc, resulting in compile error

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>
#include "videoRenderer_Qt.h"
#include <streamPlayer.h>

extern MainWindow* mainWin;
talk_base::scoped_refptr<webrtc::MediaStreamInterface> localStream;
std::unique_ptr<rtcModule::StreamPlayer> localPlayer;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    rtcModule::init(NULL);
}

void MainWindow::buttonPushed()
{
    rtcModule::DeviceManager devMgr;
    auto devices = rtcModule::getInputDevices(devMgr);
    for (auto& dev: devices->audio)
        printf("Audio: %s\n", dev.name.c_str());
    for (auto& dev: devices->video)
        printf("Video: %s\n", dev.name.c_str());
    rtcModule::GetUserMediaOptions options;
    options.audio = &(devices->audio[0]);
    options.video = &(devices->video[0]);
    localStream = rtcModule::getUserMedia(options, devMgr, "localStream");
    printf("video track count: %lu\n", localStream->GetVideoTracks().size());
    localPlayer.reset(new rtcModule::StreamPlayer(NULL, localStream->GetVideoTracks()[0], ui->localRenderer));
    localPlayer->start();
}

MainWindow::~MainWindow()
{
    rtcModule::cleanup();
    delete ui;
}

void MainWindow::slotKarereEvent(void* msg)
{
    QMessageBox::information(this, "", ("signal received, thread = "+std::to_string(QThread::currentThreadId())+"\nvalue = "+(const char*)msg).c_str());
}
struct MyThread: public QThread
{
    virtual void run()
    {
        QMetaObject::invokeMethod(mainWin,
          "slotKarereEvent", Qt::QueuedConnection,
          Q_ARG(void*, (void*)"this is a test message"));
    }
};
/*
void MainWindow::buttonPushed()
{
    QMessageBox::information(this, "",
        ("button pushed, thread = "+std::to_string(QThread::currentThreadId())).c_str());
    MyThread thread;
    thread.run();
}
*/
