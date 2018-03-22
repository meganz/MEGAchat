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

class ChatWindow;
class MainWindow;

class CallGui: public QWidget
{
    Q_OBJECT
    protected:
        ChatWindow *mChatWindow;
        megachat::MegaChatCall *mCall;
        RemoteCallListener *remoteCallListener;
        LocalCallListener *localCallListener;
        void setAvatarOnRemote();
        void setAvatarOnLocal();
        void drawAvatar(QImage &image, QChar letter, uint64_t userid);
        void drawOwnAvatar(QImage &image);
        void drawPeerAvatar(QImage &image);
    public slots:
        void onHangCall(bool);
        void onChatBtn(bool);
        void onMuteCam(bool);
        void onMuteMic(bool);
        void onAnswerCallBtn(bool);
    public:
        Ui::CallGui *ui;
        CallGui(ChatWindow *parent);
        virtual ~ CallGui();
        void hangCall();
        void connectCall();
        virtual void onLocalStreamObtained(rtcModule::IVideoRenderer *&renderer);
        virtual void onRemoteStreamAdded(rtcModule::IVideoRenderer *&rendererRet);
        virtual void onDestroy(rtcModule::TermCode reason, bool byPeer, const std::string &msg);
        virtual void onPeerMute(karere::AvFlags state, karere::AvFlags oldState);
        virtual void onLocalMediaError(const std::string err);
        virtual void onVideoRecv();

    friend class RemoteCallListener;
    friend class LocalCallListener;
    friend class MainWindow;
};

#endif // MAINWINDOW_H
