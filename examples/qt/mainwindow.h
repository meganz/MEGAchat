#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include "../strophe.disco.h"
#include <ui_mainwindow.h>
#include "IJingleSession.h"
#include "chatClient.h"

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
    std::string chatRoomJid;
    void handleMessage(std::string &message);
    void roomAdded(std::string &roomJid);
    void contactAdded(std::string &contactJid);
    void contactStateChange(std::string &contactJid, karere::Presence oldState, karere::Presence newState);
    void handleError(std::string &message);
    void handleWarning(std::string &message);
public slots:
    void inviteButtonPushed();
    void sendButtonPushed();
    void leaveButtonPushed();
    void buttonPushed();
    void onAudioInSelected();
    void onVideoInSelected();
//    void megaMessageSlot(void* msg);
};

extern bool inCall;
extern std::unique_ptr<karere::Client> gClient;

class CallAnswerGui: QObject
{
    Q_OBJECT
public:
    rtcModule::IAnswerCall* ctrl;
    MainWindow* mMainWin;
    QAbstractButton* answerBtn;
    QAbstractButton* rejectBtn;

    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(rtcModule::IAnswerCall* aCtrl, MainWindow* win):ctrl(aCtrl), mMainWin(win),
        msg(new QMessageBox(QMessageBox::Information,
        "Incoming call", QString::fromLatin1(ctrl->callerFullJid())+" is calling you",
        QMessageBox::NoButton, mMainWin))
    {
        msg->setAttribute(Qt::WA_DeleteOnClose);
        answerBtn = msg->addButton("Answer", QMessageBox::AcceptRole);
        rejectBtn = msg->addButton("Reject", QMessageBox::RejectRole);
        msg->setWindowModality(Qt::NonModal);
        QObject::connect(msg.get(), SIGNAL(buttonClicked(QAbstractButton*)),
            this, SLOT(onBtnClick(QAbstractButton*)));
        msg->show();
        msg->raise();
    }
public slots:
    void onBtnClick(QAbstractButton* btn)
    {
        ctrl->setUserData(nullptr);
        msg->close();
        if (btn == answerBtn)
        {
            int ret = ctrl->answer(true, rtcModule::AvFlags(true, true), nullptr, nullptr);
            if (ret == 0)
            {
                inCall = true;
                mMainWin->ui->callBtn->setText("Hangup");
            }
        }
        else
        {
            ctrl->answer(false, rtcModule::AvFlags(true, true), "hangup", nullptr);
            inCall = false;
            mMainWin->ui->callBtn->setText("Call");
        }
    }
};

class RtcEventHandler: public rtcModule::IEventHandler
{
protected:
    MainWindow* mMainWindow;
    virtual ~RtcEventHandler(){}
public:
    RtcEventHandler(MainWindow* mainWindow)
        :mMainWindow(mainWindow){}
    virtual void onLocalStreamObtained(IVideoRenderer** renderer)
    {
        inCall = true;
        mMainWindow->ui->callBtn->setText("Hangup");
        *renderer = mMainWindow->ui->localRenderer;
    }
    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, IVideoRenderer** rendererRet)
    {
        *rendererRet = mMainWindow->ui->remoteRenderer;
    }
    virtual void onCallIncomingRequest(rtcModule::IAnswerCall* ctrl)
    {
        ctrl->setUserData(new CallAnswerGui(ctrl, mMainWindow));
    }
    virtual void onIncomingCallCanceled(const char *sid, const char *event, const char *by, int accepted, void **userp)
    {
        if(*userp)
        {
            delete static_cast<CallAnswerGui*>(*userp);
            *userp = nullptr;
        }
        printf("Call canceled for reason: %s\n", event);
    }

    virtual void onCallEnded(rtcModule::IJingleSession *sess,
        const char* reason, const char* text, rtcModule::stats::IRtcStats *statsObj)
    {
        rtcModule::ISharedPtr<rtcModule::stats::IRtcStats> stats(statsObj);
        printf("on call ended\n");
        inCall = false;
        mMainWindow->ui->callBtn->setText("Call");
    }
    virtual void discoAddFeature(const char *feature)
    {
        gClient->conn->plugin<disco::DiscoPlugin>("disco").addFeature(feature);
    }
    virtual void onLocalMediaFail(const char* err, int* cont = nullptr)
    {
        KR_LOG_ERROR("=============LocalMediaFail: %s", err);
    }
    virtual void onError(const char* sid, const char* msg, const char* reason, const char* text, unsigned flags)
    {
        KR_LOG_ERROR("onError even handler: %s", msg);
    }

};

#endif // MAINWINDOW_H
