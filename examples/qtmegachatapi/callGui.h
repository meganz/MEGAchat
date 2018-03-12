#ifndef CALLGUI_H
#define CALLGUI_H

#include <QMessageBox>
#include <QWidget>
#include <ui_callGui.h>
#include <webrtc.h>
#include <chatClient.h>
#include <trackDelete.h>
#include "uiSettings.h"
#include "megachatapi.h"
#include "remoteCallListener.h"
#include "localCallListener.h"
#include "QTMegaChatCallListener.h"

class ChatWindow;
class MainWindow;

class CallAnswerGui: public QObject, public rtcModule::ICallHandler, public karere::DeleteTrackable
{
    Q_OBJECT
public:
    MainWindow *mMainWin;
    QAbstractButton *answerBtn;
    QAbstractButton *rejectBtn;
    rtcModule::ICall *mCall;
    megachat::MegaChatRoom *mChatRoom;
    std::unique_ptr<QMessageBox> msg;
    CallAnswerGui(MainWindow *parent, rtcModule::ICall *call);
    void onDestroy(rtcModule::TermCode termcode, bool byPeer, const std::string &text);
    void setCall(rtcModule::ICall *call);
    void onCallStarting();
public slots:
    void onBtnClick(QAbstractButton *btn);
};

class CallGui: public QWidget, public MegaChatCallListener, public rtcModule::ICallHandler, public rtcModule::ISessionHandler
{
Q_OBJECT
protected:
    ChatWindow *mChatWindow;
    rtcModule::ICall *mICall;
    megachat::MegaChatCall * mCall;
    rtcModule::ISession *mSess = nullptr;
    RemoteCallListener * remoteCallListener;
    LocalCallListener * localCallListener;
    QTMegaChatCallListener *megaChatCallListenerDelegate;
    void setAvatarOnRemote();
    void setAvatarOnLocal();
    void drawAvatar(QImage &image, QChar letter, uint64_t userid);
    void drawOwnAvatar(QImage &image);
    void drawPeerAvatar(QImage &image);
public slots:
    void onHupBtn(bool);
    void onChatBtn(bool);
    void onMuteCam(bool);
    void onMuteMic(bool);
public:
    Ui::CallGui *ui;
    CallGui(ChatWindow *parent, rtcModule::ICall *call);
    void hangup();
    void connectCall();
    virtual rtcModule::ISessionHandler* onNewSession(rtcModule::ISession &sess);
    virtual void setCall(rtcModule::ICall *call);
    virtual void onLocalStreamObtained(rtcModule::IVideoRenderer *&renderer);
    virtual void onRemoteStreamAdded(rtcModule::IVideoRenderer *&rendererRet);
    virtual void onRemoteStreamRemoved();
    virtual void onDestroy(rtcModule::TermCode reason, bool byPeer, const std::string &msg);
    virtual void onStateChange(uint8_t newState);
    virtual void onLocalMediaError(const std::string err);
    virtual void onRingOut(karere::Id peer);
    virtual void onCallStarting();
    virtual void onCallStarted();
    virtual void onPeerMute(karere::AvFlags state, karere::AvFlags oldState);
    virtual void onSessDestroy(rtcModule::TermCode reason, bool byPeer, const std::string &msg);
    virtual void onSessStateChange(uint8_t newState);
    virtual void onVideoRecv();

    friend class RemoteCallListener;
    friend class LocalCallListener;
};

#endif // MAINWINDOW_H
