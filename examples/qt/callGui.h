#ifndef CALLGUI_H
#define CALLGUI_H

#include <QMessageBox>
#include <QWidget>
#include <ui_callGui.h>
#include <webrtc.h>
#include <chatClient.h>

class ChatWindow;
class MainWindow;

class CallAnswerGui: public QObject
{
    Q_OBJECT
public:
    MainWindow& mParent;
    QAbstractButton* answerBtn;
    QAbstractButton* rejectBtn;
    rtcModule::ICall mCall;
    karere::Contact* mContact;
    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(MainWindow& parent, rtcModule::ICall& call);
    //ICallHandler
    virtual void onDestroy(rtcModule::TermCode termcode, const std::string& text)
    {
        KR_LOG_DEBUG("Call destroyed: %s, %s\n", rtcModule::termCodeToStr(termcode), text.c_str());
        delete this;
    }
public slots:
    void onBtnClick(QAbstractButton* btn)
    {
        msg->close();
        if (btn == answerBtn)
        {
            mCall.answer(karere::AvFlags(true, true));
        }
        else //decline button
        {
            mCall.hangup();
        }
    }
};
class CallGui: public QWidget, public karere::IApp::ICallHandler
{
Q_OBJECT
protected:
    ChatWindow& mChatWindow;
    rtcModule::ICall& mCall;
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
    virtual void onDestroy(rtcModule::TermCode code, const std::string& text);
    virtual void onLocalMediaFail(const std::string& err, bool* cont)
    {
        KR_LOG_ERROR("=============LocalMediaFail: %s", err.c_str());
    }
    virtual void onPeerMute(karere::AvFlags what);
    virtual void onPeerUnmute(karere::AvFlags what);
};

#endif // MAINWINDOW_H
