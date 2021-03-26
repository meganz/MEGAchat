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
    MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget* parent);
    ~MeetingView();
    void addVthumb(PeerWidget* widget);
    void addHiRes(PeerWidget* widget);
    void addLocalVideo(PeerWidget* widget);
    void removeThumb(uint32_t cid);
    void removeHiRes(uint32_t cid);
    void addSession(const megachat::MegaChatSession& session);
    void removeSession(const megachat::MegaChatSession& session);
    void updateSession(const megachat::MegaChatSession& session);
    void updateAudioButtonText(MegaChatCall *call);
    void updateVideoButtonText(MegaChatCall *call);
    void onRequestSpeakFinish();
    void setOnHold(bool mIsOnHold, MegaChatHandle cid);
    std::string sessionToString(const megachat::MegaChatSession& session);

protected:
    megachat::MegaChatApi &mMegaChatApi;
    mega::MegaHandle mChatid;
    bool enableReqSpeaker;

    QGridLayout* mGridLayout;
    QHBoxLayout* mThumbLayout;
    QHBoxLayout* mHiResLayout;
    QHBoxLayout* mLocalLayout;
    QScrollArea* mThumbView;
    QScrollArea* mHiResView;
    QVBoxLayout* mButtonsLayout;

    QPushButton* mHangup;
    QPushButton* mRequestSpeaker;
    QPushButton* mRequestModerator;
    QPushButton* mEnableAudio;
    QPushButton* mEnableVideo;
    QPushButton* mAudioMonitor;
    QPushButton* mSetOnHold;
    QLabel* mOnHoldLabel;

    QListWidget* mListWidget;

    std::map<uint32_t, PeerWidget*> mThumbsWidget;
    std::map<uint32_t, PeerWidget*> mHiResWidget;
    PeerWidget* mLocalWidget = nullptr;
    std::map<uint32_t, MeetingSession*> mSessionWidgets;


    void removeThumb(PeerWidget* widget);
    void removeHiRes(PeerWidget* widget);

public slots:
    void onHangUp();
    void onOnHold();
    void onSessionContextMenu(const QPoint &);
    void onRequestSpeak();
    void onEnableAudio();
    void onEnableVideo();
    void onEnableAudioMonitor(bool audioMonitorEnable);
};

#endif // MEETINGVIEW_H
