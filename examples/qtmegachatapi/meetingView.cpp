#include "meetingView.h"
#include <QMenu>
#include <QApplication>

MeetingView::MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget *parent, unsigned numParticipants)
    : QWidget(parent)
    , mMegaChatApi(megaChatApi)
    , mChatid(chatid)
{
    mGridLayout = new QGridLayout(this);
    mThumbView = new QScrollArea();
    mHiResView = new QScrollArea();
    QWidget* widgetThumbs = new QWidget(mThumbView);
    QWidget* widgetHiRes = new QWidget(mHiResView);
    mThumbLayout = new QHBoxLayout(widgetThumbs);
    mHiResLayout = new QHBoxLayout(widgetHiRes);
    mLocalLayout = new QHBoxLayout();
    mButtonsLayout = new QVBoxLayout();

    mListWidget = new QListWidget();
    mListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mListWidget, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(onSessionContextMenu(const QPoint &)));

    mHangup = new QPushButton("Hang up", this);
    connect(mHangup, SIGNAL(released()), this, SLOT(onHangUp()));
    mRequestSpeaker = new QPushButton("ReqSpeaker", this);
    connect(mRequestSpeaker, &QAbstractButton::clicked, this, [=](){onRequestSpeak(true);});
    mRequestSpeakerCancel = new QPushButton("Cancel ReqSpeaker", this);
    connect(mRequestSpeakerCancel, &QAbstractButton::clicked, this, [=](){onRequestSpeak(false);});
    mEnableAudio = new QPushButton("Audio-disable", this);
    connect(mEnableAudio, SIGNAL(released()), this, SLOT(onEnableAudio()));
    mEnableVideo = new QPushButton("Video-disable", this);
    connect(mEnableVideo, SIGNAL(released()), this, SLOT(onEnableVideo()));

    QString audioMonTex = mMegaChatApi.isAudioLevelMonitorEnabled(mChatid) ? "Audio monitor (is enabled)" : "Audio monitor (is disabled)";
    mAudioMonitor = new QPushButton(audioMonTex.toStdString().c_str(), this);
    connect(mAudioMonitor, SIGNAL(clicked(bool)), this, SLOT(onEnableAudioMonitor(bool)));

    mRemOwnSpeaker = new QPushButton("Remove own speaker", this);
    connect(mRemOwnSpeaker, SIGNAL(clicked()), this, SLOT(onRemoveSpeaker()));
    mSetOnHold = new QPushButton("onHold", this);
    connect(mSetOnHold, SIGNAL(released()), this, SLOT(onOnHold()));
    mOnHoldLabel = new QLabel("CALL ONHOLD", this);
    mOnHoldLabel->setStyleSheet("background-color:#876300 ;color:#FFFFFF; font-weight:bold;");
    mOnHoldLabel->setAlignment(Qt::AlignCenter);
    mOnHoldLabel->setContentsMargins(0, 0, 0, 0);
    mOnHoldLabel->setVisible(false);

    setLayout(mGridLayout);

    mThumbView->setWidget(widgetThumbs);
    mThumbView->setWidgetResizable(true);
    widgetThumbs->setMaximumHeight(mThumbView->height());
    mThumbLayout->setGeometry(widgetThumbs->geometry());
    mThumbView->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    mHiResView->setWidget(widgetHiRes);
    mHiResView->setWidgetResizable(true);
    mHiResView->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    mParticipantsLabel = new QLabel("");
    mParticipantsLabel->setStyleSheet("border: 1px solid #C5C5C5; color:#000000; font-weight:bold;");
    mParticipantsLabel->setAlignment(Qt::AlignCenter);
    updateNumParticipants(numParticipants);
    mGridLayout->addWidget(mParticipantsLabel, 0, 0, 1, 1);
    mGridLayout->addWidget(mListWidget, 1, 0, 3, 1);
    mGridLayout->addWidget(mThumbView, 0, 1, 1, 1);
    mGridLayout->addWidget(mHiResView, 1, 1, 1, 1);

    mButtonsLayout->addWidget(mHangup);
    mButtonsLayout->addWidget(mRequestSpeaker);
    mButtonsLayout->addWidget(mRequestSpeakerCancel);
    mButtonsLayout->addWidget(mRemOwnSpeaker);
    mButtonsLayout->addWidget(mEnableAudio);
    mButtonsLayout->addWidget(mEnableVideo);
    mButtonsLayout->addWidget(mAudioMonitor);
    mButtonsLayout->addWidget(mSetOnHold);
    mButtonsLayout->addWidget(mOnHoldLabel);
    mGridLayout->addLayout(mLocalLayout, 2, 1, 1, 1);
    mGridLayout->setRowStretch(0, 1);
    mGridLayout->setRowStretch(1, 3);
    mGridLayout->setRowStretch(2, 3);
    mLocalLayout->addLayout(mButtonsLayout);
}

MeetingView::~MeetingView()
{
}

void MeetingView::updateAudioMonitor(bool enabled)
{
    QString audioMonTex = enabled ? "Audio monitor (is enabled)" : "Audio monitor (is disabled)";
    mAudioMonitor->setText(audioMonTex.toStdString().c_str());
}

void MeetingView::updateNumParticipants(unsigned participants)
{
    std::string txt = "Participants: ";
    txt.append(std::to_string(participants));
    mParticipantsLabel->setText(txt.c_str());
}

void MeetingView::addLowResByCid(MegaChatHandle chatid, uint32_t cid)
{
    auto it = mThumbsWidget.find(cid);
    if (it == mThumbsWidget.end())
    {
        PeerWidget *peerWidget = new PeerWidget(mMegaChatApi, chatid, cid, false);
        mThumbLayout->addWidget(peerWidget);
        peerWidget->show();
        mThumbsWidget[peerWidget->getCid()] = peerWidget;
    }
}

void MeetingView::addHiResByCid(MegaChatHandle chatid, uint32_t cid)
{
    auto it = mHiResWidget.find(cid);
    if (it == mHiResWidget.end())
    {
        PeerWidget *peerWidget = new PeerWidget(mMegaChatApi, chatid, cid, true);
        mHiResLayout->addWidget(peerWidget);
        peerWidget->show();
        mHiResWidget[peerWidget->getCid()] = peerWidget;
    }
}

void MeetingView::removeLowResByCid(uint32_t cid)
{
    auto it = mThumbsWidget.find(cid);
    if (it != mThumbsWidget.end())
    {
        PeerWidget* widget = it->second;
        mThumbLayout->removeWidget(widget);
        mThumbsWidget.erase(it);
        delete widget;
    }
}

void MeetingView::removeHiResByCid(uint32_t cid)
{
    auto it = mHiResWidget.find(cid);
    if (it != mHiResWidget.end())
    {
        PeerWidget* widget = it->second;
        mHiResLayout->removeWidget(widget);
        mHiResWidget.erase(it);
        delete widget;
    }
}

void MeetingView::addLocalVideo(PeerWidget *widget)
{
    assert(!mLocalWidget);
    mLocalWidget = widget;
    QHBoxLayout *localLayout = new QHBoxLayout();
    localLayout->addWidget(widget);
    mLocalLayout->addLayout(localLayout);
}

void MeetingView::addSession(const megachat::MegaChatSession &session)
{
    QString cid(std::to_string(session.getClientid()).c_str());
    QVariant data(cid);
    MeetingSession *widget = new MeetingSession(this, session);
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(Qt::UserRole, data);
    item->setSizeHint(QSize(item->sizeHint().height(), 35));
    widget->setWidgetItem(item);
    mListWidget->insertItem(mSessionWidgets.size(), item);
    mListWidget->setItemWidget(item, widget);
    assert(mSessionWidgets.find(session.getClientid()) == mSessionWidgets.end());
    mSessionWidgets[session.getClientid()] = widget;
}

void MeetingView::removeSession(const megachat::MegaChatSession& session)
{
    auto it = mSessionWidgets.find(session.getClientid());
    if (it != mSessionWidgets.end())
    {
        MeetingSession *meetingSession = it->second;
        QListWidgetItem *item = it->second->getWidgetItem();
        mListWidget->removeItemWidget(item);
        mSessionWidgets.erase(it);
        delete item;
        delete meetingSession;
    }
}

void MeetingView::updateSession(const megachat::MegaChatSession &session)
{
    auto it = mSessionWidgets.find(session.getClientid());
    if (it != mSessionWidgets.end())
    {
        it->second->updateWidget(session);
    }
}

void MeetingView::updateAudioButtonText(MegaChatCall *call)
{
    std::string text;
    if (call->hasLocalAudio())
    {
        text = "Disable Audio";
    }
    else
    {
        text = "Enable Audio";
    }

    mEnableAudio->setText(text.c_str());
}

void MeetingView::updateVideoButtonText(MegaChatCall *call)
{
    std::string text;
    if (call->hasLocalVideo())
    {
        text = "Disable Video";
    }
    else
    {
        text = "Enable Video";
    }

    mEnableVideo->setText(text.c_str());
}

void MeetingView::setOnHold(bool isOnHold, MegaChatHandle cid)
{
    if (cid == MEGACHAT_INVALID_HANDLE)
    {
        mLocalWidget->setOnHold(isOnHold);
        mOnHoldLabel->setVisible(isOnHold);
    }
    else
    {
        // update session item
        auto sessIt = mSessionWidgets.find(cid);
        if (sessIt != mSessionWidgets.end())
        {
            sessIt->second->setOnHold(isOnHold);
        }

        // set low-res widget onHold
        auto it = mThumbsWidget.find(cid);
        if (it != mThumbsWidget.end())
        {
            it->second->setOnHold(isOnHold);
        }

        // set hi-res widget onHold
        auto auxit = mHiResWidget.find(cid);
        if (auxit != mHiResWidget.end())
        {
            auxit->second->setOnHold(isOnHold);
        }
    }
}

std::string MeetingView::sessionToString(const megachat::MegaChatSession &session)
{
    std::string returnedString;
    const char* name = mMegaChatApi.getUserFirstnameFromCache(session.getPeerid());
    if (name)
    {
        returnedString.append(name);
        delete [] name;
    }

    returnedString.append(" [ClientId: ");
    returnedString.append(std::to_string(session.getClientid())).append("]");
    return returnedString;
}

void MeetingView::onHangUp()
{
    mLocalWidget->removeVideoListener();
    std::unique_ptr<MegaChatCall> call = std::unique_ptr<MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (call)
    {
        mMegaChatApi.hangChatCall(call->getCallId());
    }
}

void MeetingView::onOnHold()
{
    std::unique_ptr<megachat::MegaChatCall> call(mMegaChatApi.getChatCall(mChatid));
    if (call)
    {
        mMegaChatApi.setCallOnHold(mChatid, !call->isOnHold());
    }
}

void MeetingView::onSessionContextMenu(const QPoint &pos)
{
    QPoint globalPoint = mListWidget->mapToGlobal(pos);
    QListWidgetItem* item = mListWidget->itemAt(pos);
    if (!item)
    {
        return;
    }

    uint32_t cid = static_cast<uint32_t>(atoi(item->data(Qt::UserRole).toString().toStdString().c_str()));
    if (mSessionWidgets.find(cid) == mSessionWidgets.end())
    {
        return;
    }

    QMenu submenu;
    std::string requestDelSpeaker("Remove speaker");
    std::string requestThumb("Request vThumb");
    std::string requestHiRes("Request hiRes");
    std::string approveSpeak("Approve Speak");
    std::string rejectSpeak("Reject Speak");
    submenu.addAction(requestThumb.c_str());
    submenu.addAction(requestHiRes.c_str());

    std::unique_ptr<megachat::MegaChatCall> call(mMegaChatApi.getChatCall(mChatid));
    std::unique_ptr<MegaChatRoom> chatRoom = std::unique_ptr<MegaChatRoom>(mMegaChatApi. getChatRoom(mChatid));
    bool moderator = (chatRoom->getOwnPrivilege() == MegaChatRoom::PRIV_MODERATOR);
    if (call && moderator)
    {
       submenu.addAction(requestDelSpeaker.c_str());
       megachat::MegaChatSession* session = call->getMegaChatSession(cid);
       if (session->hasRequestSpeak())
       {
           submenu.addAction(approveSpeak.c_str());
           submenu.addAction(rejectSpeak.c_str());
       }
    }

    QAction* rightClickItem = submenu.exec(globalPoint);
    if (rightClickItem)
    {
        if (rightClickItem->text().contains(requestThumb.c_str()))
        {
            std::unique_ptr<mega::MegaHandleList> handleList = std::unique_ptr<mega::MegaHandleList>(mega::MegaHandleList::createInstance());
            handleList->addMegaHandle(cid);
            mMegaChatApi.requestLowResVideo(mChatid, handleList.get());
        }
        else if (rightClickItem->text().contains(requestHiRes.c_str()))
        {
            mMegaChatApi.requestHiResVideo(mChatid, cid);
        }
        else if (rightClickItem->text().contains(approveSpeak.c_str()))
        {
            mMegaChatApi.approveSpeakRequest(mChatid, cid);
        }
        else if (rightClickItem->text().contains(rejectSpeak.c_str()))
        {
            mMegaChatApi.rejectSpeakRequest(mChatid, cid);
        }
        else if (rightClickItem->text().contains(requestDelSpeaker.c_str()))
        {
            onRemoveSpeaker(cid);
        }
    }
}

void MeetingView::onRequestSpeak(bool request)
{
    std::unique_ptr<MegaChatCall> call = std::unique_ptr<MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (!call)
    {
        assert(false);
        return;
    }

    request
            ? mMegaChatApi.requestSpeak(mChatid)
            : mMegaChatApi.removeRequestSpeak(mChatid);

    mRequestSpeaker->setEnabled(false);
    mRequestSpeakerCancel->setEnabled(false);
}

void MeetingView::onRequestSpeakFinish()
{
    mRequestSpeaker->setEnabled(true);
    mRequestSpeakerCancel->setEnabled(true);
}

void MeetingView::onEnableAudio()
{
    std::unique_ptr<MegaChatCall> call = std::unique_ptr<MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (!call)
    {
        assert(false);
        return;
    }

    if (call->hasLocalAudio())
    {
        mMegaChatApi.disableAudio(mChatid);
    }
    else
    {
        mMegaChatApi.enableAudio(mChatid);
    }
}

void MeetingView::onEnableVideo()
{
    std::unique_ptr<MegaChatCall> call = std::unique_ptr<MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (!call)
    {
        assert(false);
        return;
    }

    if (call->hasLocalVideo())
    {
        mMegaChatApi.disableVideo(mChatid);
    }
    else
    {
        mMegaChatApi.enableVideo(mChatid);
    }
}

void MeetingView::onRemoveSpeaker(uint32_t cid)
{
    mMegaChatApi.removeSpeaker(mChatid, MEGACHAT_INVALID_HANDLE);
}

void MeetingView::onEnableAudioMonitor(bool audioMonitorEnable)
{
    mMegaChatApi.isAudioLevelMonitorEnabled(mChatid)
           ? mMegaChatApi.enableAudioLevelMonitor(false, mChatid)
           : mMegaChatApi.enableAudioLevelMonitor(true, mChatid);
}

