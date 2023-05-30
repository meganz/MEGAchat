#ifndef KARERE_DISABLE_WEBRTC
#include "meetingView.h"
#include <QMenu>
#include <QApplication>
#include <QInputDialog>

MeetingView::MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget *parent)
    : QDialog(parent)
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
    mHangup->setVisible(false);
    mEndCall = new QPushButton("End call", this);
    connect(mEndCall, SIGNAL(released()), this, SLOT(onEndCall()));
    mEndCall->setVisible(false);
    mRequestSpeaker = new QPushButton("ReqSpeaker", this);
    connect(mRequestSpeaker, &QAbstractButton::clicked, this, [=](){onRequestSpeak(true);});
    mRequestSpeaker->setVisible(false);
    mRequestSpeakerCancel = new QPushButton("Cancel ReqSpeaker", this);
    connect(mRequestSpeakerCancel, &QAbstractButton::clicked, this, [=](){onRequestSpeak(false);});
    mRequestSpeakerCancel->setVisible(false);
    mEnableAudio = new QPushButton("Audio-disable", this);
    connect(mEnableAudio, SIGNAL(released()), this, SLOT(onEnableAudio()));
    mEnableAudio->setVisible(false);
    mEnableVideo = new QPushButton("Video-disable", this);
    connect(mEnableVideo, SIGNAL(released()), this, SLOT(onEnableVideo()));
    mEnableVideo->setVisible(false);

    QString audioMonTex = mMegaChatApi.isAudioLevelMonitorEnabled(mChatid) ? "Audio monitor (is enabled)" : "Audio monitor (is disabled)";
    mAudioMonitor = new QPushButton(audioMonTex.toStdString().c_str(), this);
    connect(mAudioMonitor, SIGNAL(clicked(bool)), this, SLOT(onEnableAudioMonitor(bool)));
    mAudioMonitor->setVisible(false);

    mRemOwnSpeaker = new QPushButton("Remove own speaker", this);
    connect(mRemOwnSpeaker, SIGNAL(clicked()), this, SLOT(onRemoveSpeaker()));
    mRemOwnSpeaker->setVisible(false);

    mJoinCallWithVideo = new QPushButton("Join Call with Video", this);
    connect(mJoinCallWithVideo, SIGNAL(clicked()), this, SLOT(onJoinCallWithVideo()));
    mJoinCallWithVideo->setVisible(false);

    mJoinCallWithoutVideo = new QPushButton("Join Call without Video", this);
    connect(mJoinCallWithoutVideo, SIGNAL(clicked()), this, SLOT(onJoinCallWithoutVideo()));
    mJoinCallWithoutVideo->setVisible(false);

    mSetOnHold = new QPushButton("onHold", this);
    connect(mSetOnHold, SIGNAL(released()), this, SLOT(onOnHold()));
    mOnHoldLabel = new QLabel("CALL ONHOLD", this);
    mOnHoldLabel->setStyleSheet("background-color:#876300 ;color:#FFFFFF; font-weight:bold;");
    mOnHoldLabel->setAlignment(Qt::AlignCenter);
    mOnHoldLabel->setContentsMargins(0, 0, 0, 0);
    mOnHoldLabel->setVisible(false);
    mSetOnHold->setVisible(false);

    mWaitingRoomShow = new QPushButton("Show waiting room", this);
    connect(mWaitingRoomShow, SIGNAL(clicked()), this, SLOT(onWrShow()));
    mWaitingRoomShow->setVisible(true);

    mAllowJoin= new QPushButton("Allow join user", this);
    connect(mAllowJoin, SIGNAL(clicked()), this, SLOT(onAllowJoin()));
    mAllowJoin->setVisible(true);

    mPushWr= new QPushButton("Push user into Wr", this);
    connect(mPushWr, SIGNAL(clicked()), this, SLOT(onPushWr()));
    mPushWr->setVisible(true);

    mKickWr= new QPushButton("Kick user from call", this);
    connect(mKickWr, SIGNAL(clicked()), this, SLOT(onKickWr()));
    mKickWr->setVisible(true);

    setLayout(mGridLayout);

    mThumbView->setWidget(widgetThumbs);
    mThumbView->setWidgetResizable(true);
    widgetThumbs->setMaximumHeight(mThumbView->height());
    mThumbLayout->setGeometry(widgetThumbs->geometry());
    mThumbView->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    mHiResView->setWidget(widgetHiRes);
    mHiResView->setWidgetResizable(true);
    mHiResView->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    mLabel = new QLabel("");
    mLabel->setStyleSheet("border: 1px solid #C5C5C5; color:#000000; font-weight:bold;");
    mLabel->setAlignment(Qt::AlignCenter);
    mGridLayout->addWidget(mLabel, 0, 0, 1, 1);
    mGridLayout->addWidget(mListWidget, 1, 0, 3, 1);
    mGridLayout->addWidget(mThumbView, 0, 1, 1, 1);
    mGridLayout->addWidget(mHiResView, 1, 1, 1, 1);

    mButtonsLayout->addWidget(mHangup);
    mButtonsLayout->addWidget(mEndCall);
    mButtonsLayout->addWidget(mRequestSpeaker);
    mButtonsLayout->addWidget(mRequestSpeakerCancel);
    mButtonsLayout->addWidget(mRemOwnSpeaker);
    mButtonsLayout->addWidget(mEnableAudio);
    mButtonsLayout->addWidget(mEnableVideo);
    mButtonsLayout->addWidget(mAudioMonitor);
    mButtonsLayout->addWidget(mSetOnHold);
    mButtonsLayout->addWidget(mOnHoldLabel);
    mButtonsLayout->addWidget(mJoinCallWithVideo);
    mButtonsLayout->addWidget(mJoinCallWithoutVideo);
    mButtonsLayout->addWidget(mWaitingRoomShow);
    mButtonsLayout->addWidget(mAllowJoin);
    mButtonsLayout->addWidget(mPushWr);
    mButtonsLayout->addWidget(mKickWr);
    mGridLayout->addLayout(mLocalLayout, 2, 1, 1, 1);
    mGridLayout->setRowStretch(0, 1);
    mGridLayout->setRowStretch(1, 3);
    mGridLayout->setRowStretch(2, 3);
    mLocalLayout->addLayout(mButtonsLayout);

    QVBoxLayout *localLayout = new QVBoxLayout();
    mLocalLayout->addLayout(localLayout);
    PeerWidget* widget = new PeerWidget(mMegaChatApi, chatid, 0, 0, true);
    addLocalVideo(widget);

    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint| Qt::WindowMinimizeButtonHint);
    std::unique_ptr<megachat::MegaChatRoom> chatroom = std::unique_ptr<megachat::MegaChatRoom>(mMegaChatApi.getChatRoom(chatid));
    assert(chatroom);
    setWindowTitle(chatroom->getTitle());
}

MeetingView::~MeetingView()
{
}

void MeetingView::updateAudioMonitor(bool enabled)
{
    QString audioMonTex = enabled ? "Audio monitor (is enabled)" : "Audio monitor (is disabled)";
    mAudioMonitor->setText(audioMonTex.toStdString().c_str());
}

void MeetingView::updateLabel(megachat::MegaChatCall *call)
{
    std::string txt = call->isOwnModerator() ? QString::fromUtf8("<span style='font-size:25px'>\xE2\x99\x9A</span>").toStdString() : std::string();
    txt.append (" Participants: ")
            .append(std::to_string(call->getNumParticipants()))
            .append("  State: ")
            .append(callStateToString(*call));

    if (call->getStatus() == megachat::MegaChatCall::CALL_STATUS_WAITING_ROOM)
    {
        txt.append("<br /><span style='color:#A30010'>WAITING ROOM</span>");
    }

    if (call->hasChanged(megachat::MegaChatCall::CHANGE_TYPE_NETWORK_QUALITY))
    {
        // just update mNetworkQuality if CHANGE_TYPE_NETWORK_QUALITY changed
        mNetworkQuality = call->getNetworkQuality();
    }

    if (mNetworkQuality == ::megachat::MegaChatCall::NETWORK_QUALITY_BAD)
    {
        txt.append("<br /><span style='color:#FF0000'>SLOW NETWORK</span>");
    }

    if (!getNumSessions() && call->getStatus() == ::megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS)
    {
        txt.append("<br /><span style='color:#FFA500'>NO PARTICIPANTS</span>");
    }

    mLabel->setText(txt.c_str());
}

void MeetingView::setNotParticipating()
{
    mLocalWidget->setVisible(false);
    mHangup->setVisible(false);
    mEndCall->setVisible(false);
    mRequestSpeaker->setVisible(false);
    mRequestSpeakerCancel->setVisible(false);
    mEnableAudio->setVisible(false);
    mEnableVideo->setVisible(false);
    mAudioMonitor->setVisible(false);
    mRemOwnSpeaker->setVisible(false);
    mSetOnHold->setVisible(false);
    mOnHoldLabel->setVisible(false);
    mJoinCallWithVideo->setVisible(true);
    mJoinCallWithoutVideo->setVisible(true);
    mLocalWidget->setOnHold(false);
}

void MeetingView::setConnecting()
{
    mLocalWidget->setVisible(false);
    mHangup->setVisible(true);
    mEndCall->setVisible(true);
    mRequestSpeaker->setVisible(false);
    mRequestSpeakerCancel->setVisible(false);
    mEnableAudio->setVisible(false);
    mEnableVideo->setVisible(false);
    mAudioMonitor->setVisible(false);
    mRemOwnSpeaker->setVisible(false);
    mSetOnHold->setVisible(false);
    mJoinCallWithVideo->setVisible(false);
    mJoinCallWithoutVideo->setVisible(false);
}

bool MeetingView::hasLowResByCid(uint32_t cid)
{
    return mThumbsWidget.find(cid) != mThumbsWidget.end();
}

bool MeetingView::hasHiResByCid(uint32_t cid)
{
    return mHiResWidget.find(cid) != mHiResWidget.end();
}

std::string MeetingView::callStateToString(const ::megachat::MegaChatCall &call)
{
    switch (call.getStatus())
    {
        case ::megachat::MegaChatCall::CALL_STATUS_INITIAL:
            return "Initial";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_USER_NO_PRESENT:
            return "No Present";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_CONNECTING:
            return "Connecting";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_WAITING_ROOM:
            return "Waiting room";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_JOINING:
            return "Joining";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS:
            return "In-Progress";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION:
            return "Terminating";
        break;
        case ::megachat::MegaChatCall::CALL_STATUS_DESTROYED:
            return "Destroyed";
        break;
        default:
            assert(false);
            return "Unknown";
            break;
    }
}

void MeetingView::addLowResByCid(megachat::MegaChatHandle chatid, uint32_t cid)
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

void MeetingView::addHiResByCid(megachat::MegaChatHandle chatid, uint32_t cid)
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

void MeetingView::createRingingWindow(megachat::MegaChatHandle callid)
{
    if (!mRingingWindow)
    {
        mRingingWindow = mega::make_unique<QMessageBox>(this);
        mRingingWindow->setText("New call");
        mRingingWindow->setInformativeText("Answer?");
        mRingingWindow->setStandardButtons(QMessageBox::Yes|QMessageBox::Cancel|QMessageBox::Ignore);
        int ringingWindowOption = mRingingWindow->exec();
        if (ringingWindowOption == QMessageBox::Yes)
        {
            mMegaChatApi.answerChatCall(mChatid, true);
        }
        else if (ringingWindowOption == QMessageBox::Cancel)
        {
            mMegaChatApi.hangChatCall(callid);
        }
        else if (ringingWindowOption == QMessageBox::Ignore)
        {
            mMegaChatApi.setIgnoredCall(mChatid);
        }
    }
}

void MeetingView::destroyRingingWindow()
{
    if (mRingingWindow)
    {
        mRingingWindow.reset(nullptr);
    }
}

void MeetingView::addLocalVideo(PeerWidget *widget)
{
    assert(!mLocalWidget);
    mLocalWidget = widget;
    mLocalWidget->setVisible(false);
    mLocalLayout->layout()->addWidget(mLocalWidget);
    adjustSize();
}

void MeetingView::joinedToCall(const megachat::MegaChatCall &call)
{
    updateAudioButtonText(call);
    updateVideoButtonText(call);
    mLocalWidget->setVisible(true);
    mHangup->setVisible(true);
    mEndCall->setVisible(true);
    mRequestSpeaker->setVisible(true);
    mRequestSpeakerCancel->setVisible(true);
    mEnableAudio->setVisible(true);
    mEnableVideo->setVisible(true);
    mAudioMonitor->setVisible(true);
    mRemOwnSpeaker->setVisible(true);
    mSetOnHold->setVisible(true);
}

bool MeetingView::hasSession(megachat::MegaChatHandle h)
{
    return mSessionWidgets.find(static_cast<uint32_t>(h)) != mSessionWidgets.end();
}

void MeetingView::addSession(const megachat::MegaChatSession &session)
{
    if (hasSession(session.getClientid()))
    {
        return;
    }

    QString cid(std::to_string(session.getClientid()).c_str());
    QVariant data(cid);
    MeetingSession *widget = new MeetingSession(this, session);
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(Qt::UserRole, data);

    int itemHeight = 35; // Set the desired height of the item
    item->setSizeHint(QSize(item->sizeHint().width(), itemHeight));
    widget->setWidgetItem(item);
    widget->setFixedSize(QSize(widget->sizeHint().width(), itemHeight)); // Set the fixed size of the widget
    mListWidget->insertItem(static_cast<int>(mSessionWidgets.size()), item);
    mListWidget->setItemWidget(item, widget);
    assert(mSessionWidgets.find(static_cast<uint32_t>(session.getClientid())) == mSessionWidgets.end());
    mSessionWidgets[static_cast<uint32_t>(session.getClientid())] = widget;
}

size_t MeetingView::getNumSessions( ) const
{
    return mSessionWidgets.size();
}

void MeetingView::removeSession(const megachat::MegaChatSession& session)
{
    auto it = mSessionWidgets.find(static_cast<uint32_t>(session.getClientid()));
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
    auto it = mSessionWidgets.find(static_cast<uint32_t>(session.getClientid()));
    if (it != mSessionWidgets.end())
    {
        it->second->updateWidget(session);
    }
}

void MeetingView::updateAudioButtonText(const megachat::MegaChatCall& call)
{
    std::string text;
    if (call.hasLocalAudio())
    {
        text = "Disable Audio";
    }
    else
    {
        text = "Enable Audio";
    }

    mEnableAudio->setText(text.c_str());
}

void MeetingView::updateVideoButtonText(const megachat::MegaChatCall &call)
{
    std::string text;
    if (call.hasLocalVideo())
    {
        text = "Disable Video";
    }
    else
    {
        text = "Enable Video";
    }

    mEnableVideo->setText(text.c_str());
}

void MeetingView::setOnHold(bool isOnHold, megachat::MegaChatHandle cid)
{
    if (cid == megachat::MEGACHAT_INVALID_HANDLE)
    {
        mLocalWidget->setOnHold(isOnHold);
        mOnHoldLabel->setVisible(isOnHold);
    }
    else
    {
        // update session item
        auto sessIt = mSessionWidgets.find(static_cast<uint32_t>(cid));
        if (sessIt != mSessionWidgets.end())
        {
            sessIt->second->setOnHold(isOnHold);
        }

        // set low-res widget onHold
        auto it = mThumbsWidget.find(static_cast<uint32_t>(cid));
        if (it != mThumbsWidget.end())
        {
            it->second->setOnHold(isOnHold);
        }

        // set hi-res widget onHold
        auto auxit = mHiResWidget.find(static_cast<uint32_t>(cid));
        if (auxit != mHiResWidget.end())
        {
            auxit->second->setOnHold(isOnHold);
        }
    }
}

std::string MeetingView::sessionToString(const megachat::MegaChatSession &session)
{
    std::string returnedString;
    std::unique_ptr<megachat::MegaChatRoom> chatRoom(mMegaChatApi.getChatRoom(mChatid));
    for (size_t i = 0; i < chatRoom->getPeerCount(); i++)
    {
        if (chatRoom->getPeerHandle(static_cast<unsigned int>(i)) == session.getPeerid())
        {
            const char *firstName = chatRoom->getPeerFirstname(static_cast<unsigned int>(i));
            if (firstName)
            {
                returnedString.append(firstName);
            }

            const char *email = chatRoom->getPeerEmail(static_cast<unsigned int>(i));
            if (email)
            {
                returnedString.append(" (");
                returnedString.append(email);
                returnedString.append(" )");
            }
        }
    }

    returnedString.append(" [ClientId: ");
    returnedString.append(std::to_string(session.getClientid())).append("]");
    return returnedString;
}

void MeetingView::onHangUp()
{
    std::unique_ptr<megachat::MegaChatCall> call = std::unique_ptr<megachat::MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (call)
    {
        mMegaChatApi.hangChatCall(call->getCallId());
    }
}

void MeetingView::onEndCall()
{
    std::unique_ptr<megachat::MegaChatCall> call = std::unique_ptr<megachat::MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (call)
    {
        mMegaChatApi.endChatCall(call->getCallId());
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
    std::string stopThumb("Stop vThumb");
    std::string stopHiRes("Stop hiRes");
    std::string approveSpeak("Approve Speak");
    std::string rejectSpeak("Reject Speak");
    std::string pushWr("Push waiting room");
    std::string kickWr("Kick waiting room");
    submenu.addAction(requestThumb.c_str());

    QMenu *hiResMenuQuality = submenu.addMenu("Request hiRes with quality");
    QAction action3("Default", this);
    connect(&action3, &QAction::triggered, this, [=](){
        mMegaChatApi.requestHiResVideoWithQuality(mChatid, cid, megachat::MegaChatCall::CALL_QUALITY_HIGH_DEF);});
    hiResMenuQuality->addAction(&action3);

    QAction action4("2x lower", this);
    connect(&action4, &QAction::triggered, this, [=](){
        mMegaChatApi.requestHiResVideoWithQuality(mChatid, cid, megachat::MegaChatCall::CALL_QUALITY_HIGH_MEDIUM);});
    hiResMenuQuality->addAction(&action4);

    QAction action5("4x lower", this);
    connect(&action5, &QAction::triggered, this, [=](){
        mMegaChatApi.requestHiResVideoWithQuality(mChatid, cid, megachat::MegaChatCall::CALL_QUALITY_HIGH_LOW);});
    hiResMenuQuality->addAction(&action5);

    QMenu *hiResMenu = submenu.addMenu("Adjust High Resolution");
    QAction action6("Default", this);
    connect(&action6, &QAction::triggered, this, [=](){
        mMegaChatApi.requestHiResQuality(mChatid, cid, megachat::MegaChatCall::CALL_QUALITY_HIGH_DEF);});
    hiResMenu->addAction(&action6);

    QAction action7("2x lower", this);
    connect(&action7, &QAction::triggered, this, [=](){
        mMegaChatApi.requestHiResQuality(mChatid, cid, megachat::MegaChatCall::CALL_QUALITY_HIGH_MEDIUM);});
    hiResMenu->addAction(&action7);

    QAction action8("4x lower", this);
    connect(&action8, &QAction::triggered, this, [=](){
        mMegaChatApi.requestHiResQuality(mChatid, cid, megachat::MegaChatCall::CALL_QUALITY_HIGH_LOW);});
    hiResMenu->addAction(&action8);

    //submenu.addAction(requestHiRes.c_str());
    submenu.addAction(stopThumb.c_str());
    submenu.addAction(stopHiRes.c_str());

    std::unique_ptr<megachat::MegaChatCall> call(mMegaChatApi.getChatCall(mChatid));
    std::unique_ptr<megachat::MegaChatRoom> chatRoom = std::unique_ptr<megachat::MegaChatRoom>(mMegaChatApi. getChatRoom(mChatid));
    bool moderator = (chatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_MODERATOR);
    if (call && moderator)
    {
       submenu.addAction(requestDelSpeaker.c_str());
       megachat::MegaChatSession* session = call->getMegaChatSession(cid);
       if (session->hasRequestSpeak())
       {
           submenu.addAction(approveSpeak.c_str());
           submenu.addAction(rejectSpeak.c_str());
       }

       QMenu* wrMenu = submenu.addMenu("Waiting Room management");
       wrMenu->addAction(pushWr.c_str());
       wrMenu->addAction(kickWr.c_str());
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
        else if (rightClickItem->text().contains(stopThumb.c_str()))
        {
            std::unique_ptr<mega::MegaHandleList> handleList = std::unique_ptr<mega::MegaHandleList>(mega::MegaHandleList::createInstance());
            handleList->addMegaHandle(cid);
            mMegaChatApi.stopLowResVideo(mChatid, handleList.get());
        }
        else if (rightClickItem->text().contains(stopHiRes.c_str()))
        {
            std::unique_ptr<mega::MegaHandleList> handleList = std::unique_ptr<mega::MegaHandleList>(mega::MegaHandleList::createInstance());
            handleList->addMegaHandle(cid);
            mMegaChatApi.stopHiResVideo(mChatid, handleList.get());
        }
        else if (rightClickItem->text().contains(pushWr.c_str()))
        {
            megachat::MegaChatSession* sess = call->getMegaChatSession(cid);
            if (sess)
            {
                std::unique_ptr<mega::MegaHandleList> l(mega::MegaHandleList::createInstance());
                l->addMegaHandle(sess->getPeerid());
                mMegaChatApi.pushUsersIntoWaitingRoom(call->getChatid(), l.get(), false);
            }
        }
        else if (rightClickItem->text().contains(kickWr.c_str()))
        {
            megachat::MegaChatSession* sess = call->getMegaChatSession(cid);
            if (sess)
            {
                std::unique_ptr<mega::MegaHandleList> l(mega::MegaHandleList::createInstance());
                l->addMegaHandle(sess->getPeerid());
                mMegaChatApi.kickUsersFromCall(call->getChatid(), l.get());
            }
        }
    }
}

void MeetingView::onRequestSpeak(bool request)
{
    std::unique_ptr<megachat::MegaChatCall> call = std::unique_ptr<megachat::MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
    if (!call)
    {
        assert(false);
        return;
    }

    request
            ? mMegaChatApi.requestSpeak(mChatid)
            : mMegaChatApi.removeRequestSpeak(mChatid);
}

void MeetingView::onEnableAudio()
{
    std::unique_ptr<megachat::MegaChatCall> call = std::unique_ptr<megachat::MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
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
    std::unique_ptr<megachat::MegaChatCall> call = std::unique_ptr<megachat::MegaChatCall>(mMegaChatApi.getChatCall(mChatid));
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

void MeetingView::onRemoveSpeaker(uint32_t)
{
    mMegaChatApi.removeSpeaker(mChatid, megachat::MEGACHAT_INVALID_HANDLE);
}

void MeetingView::onRemoveSpeaker()
{
    mMegaChatApi.removeSpeaker(mChatid, megachat::MEGACHAT_INVALID_HANDLE);
}

void MeetingView::onEnableAudioMonitor(bool)
{
    mMegaChatApi.isAudioLevelMonitorEnabled(mChatid)
           ? mMegaChatApi.enableAudioLevelMonitor(false, mChatid)
           : mMegaChatApi.enableAudioLevelMonitor(true, mChatid);
}

void MeetingView::onJoinCallWithVideo()
{
    mMegaChatApi.startChatCall(mChatid);
}

void MeetingView::onWrShow()
{
    std::string wrText = "<br /><span style='color:#A30010'>NO WAITING ROOM</span>";
    std::unique_ptr<megachat::MegaChatCall> call(mMegaChatApi.getChatCall(mChatid));
    if (call && call->getWaitingRoom())
    {
        wrText.assign("<br /><span style='color:#A30010'>EMPTY WAITING ROOM</span>");
        const megachat::MegaChatWaitingRoom* wr = call->getWaitingRoom();
        std::unique_ptr<mega::MegaHandleList>wrPeers(wr->getPeers());
        if (wrPeers && wrPeers->size())
        {
            wrText.clear();
            for (size_t i= 0; i < wrPeers->size(); ++i)
            {
                ::mega::MegaHandle h = wrPeers->get(static_cast<unsigned int>(i));
                mega::unique_ptr<const char[]>b64handle(::mega::MegaApi::userHandleToBase64(h));
                wrText.append(b64handle.get());
                wrText.append(" : ");
                wrText.append(wr->getPeerStatus(h) == megachat::MegaChatWaitingRoom::MWR_ALLOWED
                                  ? "<span style='color:#00A310'>"
                                  : "<span style='color:#A30010'>");
                wrText.append(megachat::MegaChatWaitingRoom::peerStatusToString(wr->getPeerStatus(h)));
                wrText.append("</span><br />");
            }
        }
    }

    QMessageBox msg;
    msg.setIcon(QMessageBox::Information);
    msg.setText(wrText.c_str());
    msg.exec();
}

void MeetingView::onAllowJoin()
{
    QString peerId = QInputDialog::getText(this, tr("Allow peer to Join"), tr("Enter peerId (B64)"));
    std::unique_ptr<mega::MegaHandleList> handleList = std::unique_ptr<mega::MegaHandleList>(mega::MegaHandleList::createInstance());
    handleList->addMegaHandle(::mega::MegaApi::base64ToUserHandle(peerId.toStdString().c_str()));
    mMegaChatApi.allowUsersJoinCall(mChatid, handleList.get());
}

void MeetingView::onPushWr()
{
    QString peerId = QInputDialog::getText(this, tr("Push user into Wr"), tr("Enter peerId (B64)"));
    std::unique_ptr<mega::MegaHandleList> handleList = std::unique_ptr<mega::MegaHandleList>(mega::MegaHandleList::createInstance());
    handleList->addMegaHandle(::mega::MegaApi::base64ToUserHandle(peerId.toStdString().c_str()));
    mMegaChatApi.pushUsersIntoWaitingRoom(mChatid, handleList.get(), false);
}

void MeetingView::onKickWr()
{
    QString peerId = QInputDialog::getText(this, tr("Kick user from call"), tr("Enter peerId (B64)"));
    std::unique_ptr<mega::MegaHandleList> handleList = std::unique_ptr<mega::MegaHandleList>(mega::MegaHandleList::createInstance());
    handleList->addMegaHandle(::mega::MegaApi::base64ToUserHandle(peerId.toStdString().c_str()));
    mMegaChatApi.kickUsersFromCall(mChatid, handleList.get());
}

void MeetingView::onJoinCallWithoutVideo()
{
    mMegaChatApi.startChatCall(mChatid, false);
}
#endif
