#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include "../strophe.disco.h"
#include <ui_mainwindow.h>
#include "karereCommon.h"
#include "IJingleSession.h"
#include "ChatClient.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    Ui::MainWindow *ui;
public slots:
    void buttonPushed();
    void onAudioInSelected();
    void onVideoInSelected();
//    void megaMessageSlot(void* msg);
};

extern bool inCall;
extern std::unique_ptr<karere::Client> gClient;
class RtcEventHandler: public rtcModule::IEventHandler
{
protected:
    MainWindow* mMainWindow;
public:
    RtcEventHandler(MainWindow* mainWindow)
        :mMainWindow(mainWindow){}
    virtual void onLocalStreamObtained(IVideoRenderer** renderer)
    {
        *renderer = mMainWindow->ui->localRenderer;
    }
    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, IVideoRenderer** rendererRet)
    {
        *rendererRet = mMainWindow->ui->remoteRenderer;
    }
    virtual void onCallIncomingRequest(rtcModule::IAnswerCall* ctrl)
    {
        int ret = ctrl->answer(true, rtcModule::AvFlags(true, true), nullptr, nullptr);
        if (ret == 0)
        {
            inCall = true;
            mMainWindow->ui->callBtn->setText("Hangup");
        }
    }
    virtual void onCallEnded(rtcModule::IJingleSession *sess, rtcModule::IRtcStats *stats)
    {
        inCall = false;
        mMainWindow->ui->callBtn->setText("Call");
    }

};

#endif // MAINWINDOW_H
