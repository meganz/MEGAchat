#ifndef CALLGUI_H
#define CALLGUI_H

#include <QMessageBox>
#include <QWidget>
#include <ui_callGui.h>
#include <webrtc.h>
#include <chatClient.h>
#include <trackDelete.h>
class ChatWindow;
class MainWindow;

class CallAnswerGui: public QObject, public rtcModule::ICallHandler, public karere::DeleteTrackable
{
    Q_OBJECT
public:
    MainWindow& mParent;
    QAbstractButton* answerBtn;
    QAbstractButton* rejectBtn;
    rtcModule::ICall& mCall;
    karere::Contact* mContact;
    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(MainWindow& parent, rtcModule::ICall& call);
    //ICallHandler minimal implementation
    virtual void onDestroy(rtcModule::TermCode termcode, bool byPeer, const std::string& text)
    {
        KR_LOG_DEBUG("Call destroyed: %s, %s\n", rtcModule::termCodeToStr(termcode), text.c_str());
        delete this;
    }
    virtual void setCall(rtcModule::ICall* call) { assert(false); /*called only for outgoing calls*/ }
    virtual void onCallStarting();
public slots:
    void onBtnClick(QAbstractButton* btn)
    {
        msg->close();
        if (btn == answerBtn)
        {
            mCall.answer(karere::AvFlags(true, true));
            //Call handler will be switched upon receipt of onCallStarting
        }
        else //decline button
        {
            mCall.hangup();
        }
    }
};
class CallGui: public QWidget, public rtcModule::ICallHandler, public rtcModule::ISessionHandler
{
Q_OBJECT
protected:
    ChatWindow& mChatWindow;
    rtcModule::ICall* mCall;
    rtcModule::ISession* mSess = nullptr;
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
    CallGui(ChatWindow& parent, rtcModule::ICall* call);
    void hangup() { mCall->hangup(); }
    virtual void setCall(rtcModule::ICall* call) { mCall = call; }
    virtual void onLocalStreamObtained(rtcModule::IVideoRenderer*& renderer)
    {
        renderer = ui.localRenderer;
    }
    virtual void onRemoteStreamAdded(rtcModule::IVideoRenderer*& rendererRet)
    {
        rendererRet = ui.remoteRenderer;
    }
    virtual void onRemoteStreamRemoved() {}
    virtual void onDestroy(rtcModule::TermCode reason, bool byPeer, const std::string& msg);
    virtual void onStateChange(uint8_t newState) {}
    virtual rtcModule::ISessionHandler* onNewSession(rtcModule::ISession& sess)
    {
        mSess = &sess;
        return this;
    }
    virtual void onLocalMediaError(const std::string err)
    {
        KR_LOG_ERROR("=============LocalMediaFail: %s", err.c_str());
    }
    virtual void onRingOut(karere::Id peer) {}
    virtual void onCallStarting() {}
    virtual void onCallStarted() {}
    virtual void onPeerMute(karere::AvFlags state, karere::AvFlags oldState);
    //ISession
    virtual void onSessDestroy(rtcModule::TermCode reason, bool byPeer, const std::string& msg);
    virtual void onSessStateChange(uint8_t newState) {}

};

#endif // MAINWINDOW_H
