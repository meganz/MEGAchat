#include "meetingView.h"
#include <QMenu>
#include <QApplication>

MeetingView::MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget *parent)
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
    mRequestSpeaker = new QPushButton("Request Speak", this);
    connect(mRequestSpeaker, SIGNAL(released()), this, SLOT(onRequestSpeak()));
    mRequestModerator = new QPushButton("Moderator", this);
    mEnableAudio = new QPushButton("Audio-disable", this);
    connect(mEnableAudio, SIGNAL(released()), this, SLOT(onEnableAudio()));
    mEnableVideo = new QPushButton("Video-disable", this);
    connect(mEnableVideo, SIGNAL(released()), this, SLOT(onEnableVideo()));

    mAudioMonitor = new QPushButton("Audio monitor", this);
    connect(mAudioMonitor, SIGNAL(clicked(bool)), this, SLOT(onEnableAudioMonitor(bool)));
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

    mGridLayout->addWidget(mListWidget, 0, 0, 3, 1);
    mGridLayout->addWidget(mThumbView, 0, 1, 1, 1);
    mGridLayout->addWidget(mHiResView, 1, 1, 1, 1);

    mButtonsLayout->addWidget(mHangup);
    mButtonsLayout->addWidget(mRequestSpeaker);
    mButtonsLayout->addWidget(mRequestModerator);
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

    // enable ReqSpeak btn
    enableReqSpeaker = true;
    mRequestSpeaker->setEnabled(true);
    mRequestSpeaker->setText("ReqSpeak (enable)");
}

MeetingView::~MeetingView()
{
}

void MeetingView::addVthumb(PeerWidget *widget)
{
    mThumbLayout->addWidget(widget);
    widget->show();
    mThumbsWidget[widget->getCid()] = widget;
}

void MeetingView::addHiRes(PeerWidget *widget)
{
    mHiResLayout->addWidget(widget, 1);
    widget->show();
    mHiResWidget[widget->getCid()] = widget;
}

void MeetingView::addLocalVideo(PeerWidget *widget)
{
    assert(!mLocalWidget);
    mLocalWidget = widget;
    QHBoxLayout * localLayout = new QHBoxLayout();
    localLayout->addWidget(widget);
    mLocalLayout->addLayout(localLayout);
}

void MeetingView::removeThumb(uint32_t cid)
{
    auto it = mThumbsWidget.find(cid);
    if (it != mThumbsWidget.end())
    {
        PeerWidget* widget = it->second;
        removeThumb(it->second);
        mThumbsWidget.erase(it);
        delete widget;
    }
}

void MeetingView::removeHiRes(uint32_t cid)
{
    auto it = mHiResWidget.find(cid);
    if (it != mHiResWidget.end())
    {
        PeerWidget* widget = it->second;
        removeHiRes(it->second);
        mHiResWidget.erase(it);
        delete widget;
    }
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

void MeetingView::removeThumb(PeerWidget *widget)
{
    mThumbLayout->removeWidget(widget);
}

void MeetingView::removeHiRes(PeerWidget *widget)
{
    mHiResLayout->removeWidget(widget);
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
    mMegaChatApi.hangChatCall(mChatid);
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
    std::string requestThumb("Request vThumb");
    std::string requestHiRes("Request hiRes");
    std::string approveSpeak("Approve Speak");
    std::string rejectSpeak("Reject Speak");
    submenu.addAction(requestThumb.c_str());
    submenu.addAction(requestHiRes.c_str());
    std::unique_ptr<megachat::MegaChatCall> call(mMegaChatApi.getChatCall(mChatid));
    if (call && call->isModerator())
    {
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
    }
}

void MeetingView::onRequestSpeak()
{
    std::unique_ptr<MegaChatCall> call = std::unique_ptr<MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (!call)
    {
        assert(false);
        return;
    }

    mRequestSpeaker->setEnabled(false); //disable button while request is being processed
    enableReqSpeaker
            ? mMegaChatApi.requestSpeak(mChatid)
            : mMegaChatApi.removeRequestSpeak(mChatid);

    enableReqSpeaker = !enableReqSpeaker;
}

void MeetingView::onRequestSpeakFinish()
{
    enableReqSpeaker
        ? mRequestSpeaker->setText("ReqSpeak (enable)")
        : mRequestSpeaker->setText("ReqSpeak (cancel)");

    mRequestSpeaker->setEnabled(true);
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

void MeetingView::onEnableAudioMonitor(bool audioMonitorEnable)
{
    mMegaChatApi.enableAudioLevelMonitor(true, mChatid);
}

