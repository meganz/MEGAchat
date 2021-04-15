#ifndef MEETINGVIEW_H
#define MEETINGVIEW_H

#include "peerWidget.h"
#include <megachatapi.h>

#include <QPushButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <map>
#include "meetingSession.h"

class MeetingSession;
class MeetingView : public QWidget
{
    Q_OBJECT
public:
    MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget* parent, unsigned numParticipants = 0);
    ~MeetingView();
    void addLocalVideo(PeerWidget* widget);
    void addSession(const megachat::MegaChatSession& session);
    void removeSession(const megachat::MegaChatSession& session);
    void updateSession(const megachat::MegaChatSession& session);
    void updateAudioButtonText(MegaChatCall *call);
    void updateVideoButtonText(MegaChatCall *call);
    void onRequestSpeakFinish();
    void setOnHold(bool mIsOnHold, MegaChatHandle cid);
    std::string sessionToString(const megachat::MegaChatSession& session);
    void updateAudioMonitor(bool enabled);
    void updateNumParticipants(unsigned participants);

    // methods to add/remove video widgets
    void addLowResByCid(MegaChatHandle chatid, uint32_t cid);
    void addHiResByCid(MegaChatHandle chatid, uint32_t cid);
    void removeLowResByCid(uint32_t cid);
    void removeHiResByCid(uint32_t cid);

protected:
    megachat::MegaChatApi &mMegaChatApi;
    mega::MegaHandle mChatid;

    QGridLayout* mGridLayout;
    QHBoxLayout* mThumbLayout;
    QHBoxLayout* mHiResLayout;
    QHBoxLayout* mLocalLayout;
    QScrollArea* mThumbView;
    QScrollArea* mHiResView;
    QVBoxLayout* mButtonsLayout;

    QPushButton* mHangup;
    QPushButton* mRequestSpeaker;
    QPushButton* mRequestSpeakerCancel;
    QPushButton* mEnableAudio;
    QPushButton* mEnableVideo;
    QPushButton* mAudioMonitor;
    QPushButton* mRemOwnSpeaker;
    QPushButton* mSetOnHold;
    QLabel* mOnHoldLabel;
    QLabel* mParticipantsLabel;

    QListWidget* mListWidget;

    std::map<uint32_t, PeerWidget*> mThumbsWidget;
    std::map<uint32_t, PeerWidget*> mHiResWidget;
    PeerWidget* mLocalWidget = nullptr;
    std::map<uint32_t, MeetingSession*> mSessionWidgets;

public slots:
    void onHangUp();
    void onOnHold();
    void onSessionContextMenu(const QPoint &);
    void onRequestSpeak(bool request);
    void onEnableAudio();
    void onEnableVideo();
    void onRemoveSpeaker(uint32_t cid = MEGACHAT_INVALID_HANDLE);
    void onEnableAudioMonitor(bool audioMonitorEnable);
};

#endif // MEETINGVIEW_H
