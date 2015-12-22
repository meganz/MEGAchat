#ifndef CALLGUI_H
#define CALLGUI_H

#include <QMessageBox>
#include <QWidget>
#include <ui_callGui.h>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include <../strophe.disco.h>
#include <IJingleSession.h>
#include <chatClient.h>

class CallGui : public QWidget
{
    Q_OBJECT    
public:
    Ui::CallGui ui;
    CallGui(QWidget *parent = 0): QWidget(parent)
    {
        ui.setupUi(this);
    }
protected:
public slots:
    void onCallBtnPushed();
};

extern bool inCall;
extern karere::IGui::ICallGui* gCallGui;

class CallAnswerGui: QObject
{
    Q_OBJECT
public:
    rtcModule::IAnswerCall* ctrl;
    QAbstractButton* answerBtn;
    QAbstractButton* rejectBtn;

    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(rtcModule::IAnswerCall* aCtrl, CallGui* win):ctrl(aCtrl),
        msg(new QMessageBox(QMessageBox::Information,
        "Incoming call", QString::fromLatin1(ctrl->callerFullJid())+" is calling you",
        QMessageBox::NoButton, nullptr))
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
                std::string caller(ctrl->callerFullJid());
                auto pos = caller.find('@');
                if (pos == std::string::npos)
                    throw std::runtime_error("Cant get user handle from caller JID");
                chatd::Id handle(caller.c_str(), pos);
                auto chatwin = karere::gClient->gui.contactList().chatWindowForPeer(handle);
                gCallGui = chatwin->callGui();
                inCall = true;
            }
        }
        else //decline button
        {
            gCallGui = nullptr;
            inCall = false;
            ctrl->answer(false, rtcModule::AvFlags(true, true), "hangup", nullptr);
        }
    }
};
class RtcEventHandler: public rtcModule::IEventHandler
{
protected:
    virtual ~RtcEventHandler(){}
public:
    RtcEventHandler(){}
    virtual void onLocalStreamObtained(IVideoRenderer** renderer)
    {
        inCall = true;
        gCallGui->ui->callBtn->setText("Hangup");
        *renderer = mCallGui->ui->localRenderer;
    }
    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, IVideoRenderer** rendererRet)
    {
        *rendererRet = gCallGui->ui->remoteRenderer;
    }
    virtual void onCallIncomingRequest(rtcModule::IAnswerCall* ctrl)
    {
        ctrl->setUserData(new CallAnswerGui(ctrl));
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
        gCallGui->ui->callBtn->setText("Call");
        gCallGui = nullptr;
    }
    virtual void discoAddFeature(const char *feature)
    {
        karere::gClient->conn->plugin<disco::DiscoPlugin>("disco").addFeature(feature);
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
