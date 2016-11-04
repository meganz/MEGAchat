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
class MainWindow;

class CallAnswerGui: public QObject, public rtcModule::IEventHandler
{
    Q_OBJECT
public:
    MainWindow& mParent;
    QAbstractButton* answerBtn;
    QAbstractButton* rejectBtn;
    std::shared_ptr<rtcModule::ICallAnswer> mAns;
    karere::Contact* mContact;
    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(MainWindow& parent, const std::shared_ptr<rtcModule::ICallAnswer>& ans);
    //IEventHandler
    void onCallEnded(rtcModule::TermCode termcode, const std::string& text,
                     const std::shared_ptr<rtcModule::stats::IRtcStats> &stats)
    {
        KR_LOG_DEBUG("Call ended: %s, %s\n", rtcModule::ICall::termcodeToMsg(termcode), text.c_str());
        delete this;
    }
    void onLocalStreamObtained(rtcModule::IVideoRenderer*& renderer);
    void onSession();
public slots:
    void onBtnClick(QAbstractButton* btn)
    {
        msg->close();
        if (btn == answerBtn)
        {
            bool ret = mAns->answer(true, karere::AvFlags(true, true));
            if (!ret)
                return;
        }
        else //decline button
        {
            mAns->answer(false, rtcModule::AvFlags());
        }
    }
};
class CallGui: public QWidget, public karere::IApp::ICallHandler
{
Q_OBJECT
protected:
    ChatWindow& mChatWindow;
    std::shared_ptr<rtcModule::ICall> mCall;
    void setAvatarOnRemote();
    void setAvatarOnLocal();
    static void drawAvatar(QImage& image, QChar letter, uint64_t userid);
    void drawOwnAvatar(QImage& image);
    void drawPeerAvatar(QImage& image);
public slots:
    void onHupBtn(bool);
    void onChatBtn(bool);
    void onMuteCam(bool);
    void onMuteMic(bool);
public:
    Ui::CallGui ui;
    CallGui(ChatWindow& parent, const std::shared_ptr<rtcModule::ICall>& call=nullptr);
    void hangup(const std::string& msg="") { mCall->hangup(msg); }
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
    virtual void onMediaRecv(rtcModule::stats::Options& statOptions);
    virtual void onCallEnded(rtcModule::TermCode code, const std::string& text,
        const std::shared_ptr<rtcModule::stats::IRtcStats>& statsObj);
    virtual void onLocalMediaFail(const std::string& err, bool* cont)
    {
        KR_LOG_ERROR("=============LocalMediaFail: %s", err.c_str());
    }
    virtual void onPeerMute(karere::AvFlags what);
    virtual void onPeerUnmute(karere::AvFlags what);
};

#endif // MAINWINDOW_H
