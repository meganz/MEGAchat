#ifndef KARERE_DISABLE_WEBRTC
#include "meetingView.h"
#include "MainWindow.h"
#include <QMenu>
#include <QApplication>

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

    mLocalAudioDetected = new QLabel("AUDIO DETECTED", this);
    mLocalAudioDetected->setStyleSheet("background-color:#088529 ;color:#FFFFFF; font-weight:bold;");
    mLocalAudioDetected->setAlignment(Qt::AlignCenter);
    mLocalAudioDetected->setContentsMargins(0, 0, 0, 0);
    mLocalAudioDetected->setVisible(false);
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
    mButtonsLayout->addWidget(mRequestSpeaker);
    mButtonsLayout->addWidget(mRequestSpeakerCancel);
    mButtonsLayout->addWidget(mRemOwnSpeaker);
    mButtonsLayout->addWidget(mEnableAudio);
    mButtonsLayout->addWidget(mEnableVideo);
    mButtonsLayout->addWidget(mAudioMonitor);
    mButtonsLayout->addWidget(mSetOnHold);
    mButtonsLayout->addWidget(mOnHoldLabel);
    mButtonsLayout->addWidget(mLocalAudioDetected);
    mButtonsLayout->addWidget(mJoinCallWithVideo);
    mButtonsLayout->addWidget(mJoinCallWithoutVideo);
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

    mLogger = ((MainWindow *)parent)->mLogger;
}

MeetingView::~MeetingView()
{
}

void MeetingView::updateAudioMonitor(bool enabled)
{
    QString audioMonTex = enabled ? "Audio monitor (is enabled)" : "Audio monitor (is disabled)";
    mAudioMonitor->setText(audioMonTex.toStdString().c_str());
}

void MeetingView::updateLabel(unsigned participants, const std::string &state)
{
    std::string txt = "Participants: ";
    txt.append(std::to_string(participants));
    txt.append("  State: ");
    txt.append(state);
    mLabel->setText(txt.c_str());
}

void MeetingView::setNotParticipating()
{
    mLocalWidget->setVisible(false);
    mHangup->setVisible(false);
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

void MeetingView::localAudioDetected(bool audio)
{
    mLocalAudioDetected->setVisible(audio);
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
    mRequestSpeaker->setVisible(true);
    mRequestSpeakerCancel->setVisible(true);
    mEnableAudio->setVisible(true);
    mEnableVideo->setVisible(true);
    mAudioMonitor->setVisible(true);
    mRemOwnSpeaker->setVisible(true);
    mSetOnHold->setVisible(true);
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
    mListWidget->insertItem(static_cast<int>(mSessionWidgets.size()), item);
    mListWidget->setItemWidget(item, widget);
    assert(mSessionWidgets.find(static_cast<uint32_t>(session.getClientid())) == mSessionWidgets.end());
    mSessionWidgets[static_cast<uint32_t>(session.getClientid())] = widget;
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

void MeetingView::onRequestFinish(megachat::MegaChatApi*, megachat::MegaChatRequest*,
                                  megachat::MegaChatError* e)
{
    if (e->getErrorCode() == megachat::MegaChatError::ERROR_OK)
    {
        mUserDataReceivedFunc();
    }
    else
    {
        mLogger->postLog("Couldn't retrieve user data for user pariticipating in the session.");
    }
}

std::string MeetingView::sessionToString(megachat::MegaChatHandle sessionPeerId,
                                         megachat::MegaChatHandle sessionClientId,
                                         std::function<void()> userDataReceived)
{
    std::string returnedString;
    std::unique_ptr<megachat::MegaChatRoom> chatRoom(mMegaChatApi.getChatRoom(mChatid));
    for (size_t i = 0; i < chatRoom->getPeerCount(); i++)
    {
        megachat::MegaChatHandle userHandle = chatRoom->getPeerHandle(static_cast<unsigned int>(i));
        if (userHandle == sessionPeerId)
        {
            auto firstName =
                std::unique_ptr<const char[]>(mMegaChatApi.getUserFirstnameFromCache(userHandle));
            if (firstName)
            {
                returnedString.append(firstName.get());
            }
            else
            {
                returnedString.append("Retrieving data...");
                mUserDataReceivedFunc = userDataReceived;
                auto peersList = std::unique_ptr<::mega::MegaHandleList>(
                    ::mega::MegaHandleList::createInstance());
                peersList->addMegaHandle(userHandle);
                mMegaChatApi.loadUserAttributes(mChatid, peersList.get(), this);
            }

            auto email =
                std::unique_ptr<const char[]>(mMegaChatApi.getUserEmailFromCache(userHandle));
            if (email)
            {
                returnedString.append(" (");
                returnedString.append(email.get());
                returnedString.append(" )");
            }
        }
    }

    returnedString.append(" [ClientId: ");
    returnedString.append(std::to_string(sessionClientId)).append("]");
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

void MeetingView::onRemoveSpeaker(uint32_t cid)
{
    mMegaChatApi.removeSpeaker(mChatid, megachat::MEGACHAT_INVALID_HANDLE);
}

void MeetingView::onEnableAudioMonitor(bool audioMonitorEnable)
{
    mMegaChatApi.isAudioLevelMonitorEnabled(mChatid)
           ? mMegaChatApi.enableAudioLevelMonitor(false, mChatid)
           : mMegaChatApi.enableAudioLevelMonitor(true, mChatid);
}

void MeetingView::onJoinCallWithVideo()
{
    mMegaChatApi.startChatCall(mChatid);
}

void MeetingView::onJoinCallWithoutVideo()
{
    mMegaChatApi.startChatCall(mChatid, false);
}
#endif
