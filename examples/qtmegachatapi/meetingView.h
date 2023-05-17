#ifndef KARERE_DISABLE_WEBRTC
#ifndef MEETINGVIEW_H
#define MEETINGVIEW_H

#include "peerWidget.h"
#include <megachatapi.h>

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <map>
#include "meetingSession.h"

class MeetingSession;
class MeetingView : public QDialog
{
    Q_OBJECT
public:
    MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget* parent);
    ~MeetingView();
    void addLocalVideo(PeerWidget* widget);
    void joinedToCall(const megachat::MegaChatCall& call);
    bool hasSession(megachat::MegaChatHandle h);
    void addSession(const megachat::MegaChatSession& session);
    void removeSession(const megachat::MegaChatSession& session);
    size_t getNumSessions( ) const;
    void updateSession(const megachat::MegaChatSession& session);
    void updateAudioButtonText(const megachat::MegaChatCall &call);
    void updateVideoButtonText(const megachat::MegaChatCall &call);
    void setOnHold(bool mIsOnHold, megachat::MegaChatHandle cid);
    std::string sessionToString(const megachat::MegaChatSession& session);
    void updateAudioMonitor(bool enabled);
    void updateLabel(megachat::MegaChatCall *call);
    void setNotParticipating();
    void setConnecting();
    static std::string callStateToString(const megachat::MegaChatCall& call);

    // methods to add/remove video widgets
    bool hasLowResByCid(uint32_t cid);
    bool hasHiResByCid(uint32_t cid);
    void addLowResByCid(megachat::MegaChatHandle chatid, uint32_t cid);
    void addHiResByCid(megachat::MegaChatHandle chatid, uint32_t cid);
    void removeLowResByCid(uint32_t cid);
    void removeHiResByCid(uint32_t cid);
    void createRingingWindow(megachat::MegaChatHandle callid);
    void destroyRingingWindow();

protected:
    megachat::MegaChatApi &mMegaChatApi;
    mega::MegaHandle mChatid;
    int mNetworkQuality = ::megachat::MegaChatCall::NETWORK_QUALITY_GOOD;

    QGridLayout* mGridLayout;
    QHBoxLayout* mThumbLayout;
    QHBoxLayout* mHiResLayout;
    QHBoxLayout* mLocalLayout;
    QScrollArea* mThumbView;
    QScrollArea* mHiResView;
    QVBoxLayout* mButtonsLayout;

    QPushButton* mHangup;
    QPushButton* mEndCall;
    QPushButton* mRequestSpeaker;
    QPushButton* mRequestSpeakerCancel;
    QPushButton* mEnableAudio;
    QPushButton* mEnableVideo;
    QPushButton* mAudioMonitor;
    QPushButton* mRemOwnSpeaker;
    QPushButton* mSetOnHold;
    QPushButton* mJoinCallWithVideo;
    QPushButton* mJoinCallWithoutVideo;
    QPushButton* mWaitingRoomShow;
    QLabel* mOnHoldLabel;
    QLabel* mLabel;

    QListWidget* mListWidget;

    std::map<uint32_t, PeerWidget*> mThumbsWidget;
    std::map<uint32_t, PeerWidget*> mHiResWidget;
    PeerWidget* mLocalWidget = nullptr;
    std::map<uint32_t, MeetingSession*> mSessionWidgets;

    std::unique_ptr<QMessageBox> mRingingWindow;

public slots:
    void onHangUp();
    void onEndCall();
    void onOnHold();
    void onSessionContextMenu(const QPoint &);
    void onRequestSpeak(bool request);
    void onEnableAudio();
    void onEnableVideo();
    void onRemoveSpeaker(uint32_t cid);
    void onEnableAudioMonitor(bool audioMonitorEnable);
    void onJoinCallWithVideo();
    void onJoinCallWithoutVideo();
    void onWrShow();
};

#endif // MEETINGVIEW_H
#endif
