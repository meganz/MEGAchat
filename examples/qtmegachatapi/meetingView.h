#ifndef MEETINGVIEW_H
#define MEETINGVIEW_H

#include "peerWidget.h"
#include <megachatapi.h>

#include <QPushButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QListWidget>
#include <map>

class MeetingView : public QWidget
{
    Q_OBJECT
public:
    MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget* parent);
    void addVthumb(PeerWidget* widget);
    void addHiRes(PeerWidget* widget);
    void addLocalVideo(PeerWidget* widget);
    void removeThumb(uint32_t cid);
    void removeHiRes(uint32_t cid);
    void addSession(const megachat::MegaChatSession& session);
    void removeSession(const megachat::MegaChatSession& session);
    void updateSession(const megachat::MegaChatSession& session);

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

    QListWidget* mListWidget;


    std::map<uint32_t, PeerWidget*> mThumbsWidget;
    std::map<uint32_t, PeerWidget*> mHiResWidget;

    std::map<uint32_t, QListWidgetItem*> mSessionItems;

    void removeThumb(PeerWidget* widget);
    void removeHiRes(PeerWidget* widget);
    std::string sessionToString(const megachat::MegaChatSession& session);

public slots:
    void onHangUp();
    void onSessionContextMenu(const QPoint &);
};

#endif // MEETINGVIEW_H
