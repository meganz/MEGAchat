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

class ChatWindow;

class CallAnswerGui: QObject, rtcModule::IEventHandler
{
    Q_OBJECT
public:
    QAbstractButton* answerBtn;
    QAbstractButton* rejectBtn;
    std::shared_ptr<rtcModule::CallAnswerFunc> ansFunc;
    std::shared_ptr<rtcModule::ICall> call;
    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(QWidget* parent, const std::shared_ptr<rtcModule::ICall>& aCall,
            const std::shared_ptr<rtcModule::CallAnswerFunc>& ans,
            const std::shared_ptr<std::function<bool()> >& reqStillValid,
            karere::AvFlags peerMedia, const std::shared_ptr<std::set<std::string> >& files)
    :QObject(parent), ansFunc(ans), call(aCall), msg(new QMessageBox(QMessageBox::Information,
        "Incoming call", QString::fromStdString(call->peerJid()+" is calling you"),
        QMessageBox::NoButton, parent))
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
        msg->close();
        if (btn == answerBtn)
        {
            bool ret = (*ansFunc)(true, karere::AvFlags(true, true));
            if (!ret)
                return;
        }
        else //decline button
        {
            (*ansFunc)(false, rtcModule::AvFlags());
        }
    }
};
class CallGui: public QWidget, public rtcModule::IEventHandler, public karere::IGui::ICallGui
{
Q_OBJECT
protected:
    std::shared_ptr<rtcModule::ICall> mCall;
    ChatWindow& mChatWindow;
public slots:
    void onHupBtn(bool);
    void onChatBtn(bool);
    void onMuteCam(int);
    void onMuteMic(int);
public:
    Ui::CallGui ui;
    CallGui(ChatWindow& parent);
    void call();
    virtual void onOutgoingCallCreated(const std::shared_ptr<rtcModule::ICall> &aCall)
    {mCall = aCall;}
    virtual void onLocalStreamObtained(rtcModule::IVideoRenderer*& renderer)
    {
        renderer = ui.localRenderer;
    }
    virtual void onRemoteSdpRecv(rtcModule::IVideoRenderer*& rendererRet)
    {
        rendererRet = ui.remoteRenderer;
    }
    virtual void onCallEnded(rtcModule::TermCode code, const char* text,
        const std::shared_ptr<rtcModule::stats::IRtcStats>& statsObj);
    virtual void onLocalMediaFail(const std::string& err, bool* cont)
    {
        KR_LOG_ERROR("=============LocalMediaFail: %s", err.c_str());
    }
};

#endif // MAINWINDOW_H
