#ifndef CHATWINDOW_H
#define CHATWINDOW_H
#include "ui_chatWindow.h"
#include "QTMegaChatRoomListener.h"
#include "QTMegaChatNodeHistoryListener.h"
#include "megachatapi.h"
#include "chatItemWidget.h"
#include "chatMessage.h"
#include "megaLoggerApplication.h"
#include "MainWindow.h"
#include <QMessageBox>
#include "QTMegaTransferListener.h"

#ifndef KARERE_DISABLE_WEBRTC
#include "callGui.h"
/*
namespace rtcmodule
{
    class ICallHandler{};
}
class CallGui: public rtcModule::ICallHandler {};*/
#endif

const int callMaxParticipants = 9;
const int widgetsFixed = 3;

#define NMESSAGES_LOAD 16   // number of messages to load at every fetch
class ChatMessage;
class MainWindow;

namespace Ui{
class ChatWindowUi;
}
class ChatItemWidget;

class ChatWindow : public QDialog,
        public megachat::MegaChatRoomListener,
        public ::mega::MegaTransferListener,
        public megachat::MegaChatNodeHistoryListener
{
    Q_OBJECT
    public:
        ChatWindow(QWidget *parent, megachat::MegaChatApi *mMegaChatApi, megachat::MegaChatRoom *cRoom, const char *title);
        virtual ~ChatWindow();
        void openChatRoom();
        void onChatRoomUpdate(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat);
        void onMessageReceived(megachat::MegaChatApi *api, megachat::MegaChatMessage *msg);
        void onMessageUpdate(megachat::MegaChatApi *api, megachat::MegaChatMessage *msg);
        void onMessageLoaded(megachat::MegaChatApi *api, megachat::MegaChatMessage *msg);
        void onHistoryReloaded(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat);
        void onAttachmentLoaded(MegaChatApi *api, MegaChatMessage *msg);
        void onAttachmentReceived(MegaChatApi *api, MegaChatMessage *msg);
        void onAttachmentDeleted(MegaChatApi *api, MegaChatHandle msgid);
        void onTruncate(MegaChatApi *api, MegaChatHandle msgid);
        void deleteChatMessage(megachat::MegaChatMessage *msg);
        void createMembersMenu(QMenu& menu);
        void createSettingsMenu(QMenu& menu);
        void createAttachMenu(QMenu& menu);
        void truncateChatUI();
        void connectPeerCallGui(MegaChatHandle peerid);
        void destroyCallGui(MegaChatHandle mPeerid);
        void hangCall();
        void setChatTittle(const char *title);
        bool eraseChatMessage(megachat::MegaChatMessage *msg, bool temporal);
        void moveManualSendingToSending(megachat::MegaChatMessage *msg);
        void updateMessageFirstname(megachat::MegaChatHandle contactHandle, const char *firstname);
        void setMessageHeight(megachat::MegaChatMessage *msg, QListWidgetItem *item);
        QListWidgetItem *addMsgWidget (megachat::MegaChatMessage *msg, int index);
        ChatMessage *findChatMessage(megachat::MegaChatHandle msgId);
        megachat::MegaChatHandle getMessageId(megachat::MegaChatMessage *msg);
        void onTransferFinish(::mega::MegaApi *api, ::mega::MegaTransfer *transfer, ::mega::MegaError *e);
        void onAttachLocation();

#ifndef KARERE_DISABLE_WEBRTC
        std::set<CallGui *> *getCallGui();
        void setCallGui(CallGui *callGui);
#endif
    protected:
        Ui::ChatWindowUi *ui;
#ifndef KARERE_DISABLE_WEBRTC
        CallGui *mCallGui;
        std::set<CallGui *> callParticipantsGui;
#endif
        MainWindow *mMainWin;
        megachat::MegaChatApi *mMegaChatApi;
        ::mega::MegaApi *mMegaApi;
        megachat::MegaChatRoom *mChatRoom;
        ChatItemWidget *mChatItemWidget;
        MegaLoggerApplication *mLogger;
        megachat::QTMegaChatRoomListener *megaChatRoomListenerDelegate;
        ::mega::QTMegaTransferListener *megaTransferListenerDelegate;
        std::map<megachat::MegaChatHandle, ChatMessage *> mMsgsWidgetsMap;
        std::string mChatTitle;
        int nSending;
        int loadedMessages;
        int nManualSending;
        int mPendingLoad;
        MegaChatHandle mFreeCallGui [callMaxParticipants];
        int loadedAttachments;
        bool mScrollToBottomAttachments;
        megachat::QTMegaChatNodeHistoryListener *megaChatNodeHistoryListenerDelegate = NULL;
        MyMessageList *mAttachmentList = NULL;
        QWidget *mFrameAttachments = NULL;
        QMessageBox *mUploadDlg;

    private slots:
        void onMsgListRequestHistory();
        void onMemberSetPriv();
        void onMemberRemove();
        void onMsgSendBtn();
        void onMemberAdd();
        void onTruncateChat();
        void onMembersBtn(bool);
        void onShowAttachments(bool active);
        void onAttachmentRequestHistory();
        void on_mAttachBtn_clicked();
        void on_mCancelTransfer(QAbstractButton *);
        void onArchiveClicked(bool);
        void onAttachmentsClosed(QObject*);
        void onAttachNode(bool isVoiceClip);

#ifndef KARERE_DISABLE_WEBRTC
        void onCallBtn(bool video);
        void closeEvent(QCloseEvent *event);
        void createCallGui(bool, MegaChatHandle peerid);
        void getCallPos(int index, int &row, int &col);
        void onVideoCallBtn(bool);
        void onAudioCallBtn(bool);
        void deleteCallGui();
#endif
        void on_mSettingsBtn_clicked();

    friend class CallGui;
    friend class ChatMessage;
    friend class CallAnswerGui;
    friend class MainWindow;
};
#endif // CHATWINDOW_H
