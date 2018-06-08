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
        MegaChatHandle mPeerid;
        ChatWindow *mChatWindow;        
        megachat::MegaChatCall *mCall;
        RemoteCallListener *remoteCallListener;
        LocalCallListener *localCallListener;
        bool mVideo;
        bool mLocal;
        int mIndex;
        void setAvatar();
        void drawAvatar(QImage &image, QChar letter, uint64_t userid);
        void drawPeerAvatar(QImage &image);
    public slots:
        void onHangCall(bool);
        void onChatBtn(bool);
        void onMuteCam(bool);
        void onMuteMic(bool);
        void onAnswerCallBtn(bool);
    public:
        Ui::CallGui *ui;
        CallGui(ChatWindow *parent, bool video, MegaChatHandle peerid, bool local);
        virtual ~ CallGui();
        void hangCall();
        void connectPeerCallGui();
        virtual void onLocalStreamObtained(rtcModule::IVideoRenderer *&renderer);
        virtual void onRemoteStreamAdded(rtcModule::IVideoRenderer *&rendererRet);
        virtual void onDestroy(rtcModule::TermCode reason, bool byPeer, const std::string &msg);
        virtual void onPeerMute(karere::AvFlags state, karere::AvFlags oldState);
        virtual void onLocalMediaError(const std::string err);
        virtual void onVideoRecv();
        MegaChatHandle getPeer();

    friend class RemoteCallListener;
    friend class LocalCallListener;
    friend class MainWindow;
    megachat::MegaChatCall *getCall() const;
    void setCall(megachat::MegaChatCall *call);
    int getIndex() const;
    void setIndex(int index);
};

#endif // MAINWINDOW_H
