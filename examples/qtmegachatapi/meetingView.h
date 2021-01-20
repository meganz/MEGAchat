#ifndef MEETINGVIEW_H
#define MEETINGVIEW_H

#include "peerWidget.h"
#include <megachatapi.h>

#include <QPushButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <webrtc.h>
#include <map>

class MeetingView : public QWidget
{
    Q_OBJECT
public:
    MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget* parent);
    void addVthumb(PeerWidget* widget);
    void addHiRes(PeerWidget* widget);
    void addLocalVideo(PeerWidget* widget);
    void removeThumb(Cid_t cid);
    void removeHiRes(Cid_t cid);

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
    QPushButton* mRequestModerator;
    QPushButton* mEnableAudio;
    QPushButton* mEnableVideo;

    std::map<Cid_t, PeerWidget*> mThumbsWidget;
    std::map<Cid_t, PeerWidget*> mHiResWidget;

    void removeThumb(PeerWidget* widget);
    void removeHiRes(PeerWidget* widget);

public slots:
    void onHangUp();
};

#endif // MEETINGVIEW_H
