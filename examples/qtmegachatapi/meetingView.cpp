#include "meetingView.h"
#include <QMenu>

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
    mRequestSpeaker = new QPushButton("Speak", this);
    mRequestModerator = new QPushButton("Moderator", this);
    mEnableAudio = new QPushButton("Audio", this);
    mEnableVideo = new QPushButton("Video", this);

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
    mGridLayout->addLayout(mLocalLayout, 2, 1, 1, 1);
    mGridLayout->setRowStretch(0, 1);
    mGridLayout->setRowStretch(1, 3);
    mGridLayout->setRowStretch(2, 3);
    mLocalLayout->addLayout(mButtonsLayout);
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
    QHBoxLayout * localLayout = new QHBoxLayout();
    localLayout->addWidget(widget);
    mLocalLayout->addLayout(localLayout);
}

void MeetingView::removeThumb(uint32_t cid)
{
    auto it = mThumbsWidget.find(cid);
    if (it != mThumbsWidget.end())
    {
        removeThumb(it->second);
    }
}

void MeetingView::removeHiRes(uint32_t cid)
{
    auto it = mThumbsWidget.find(cid);
    if (it != mThumbsWidget.end())
    {
        removeHiRes(it->second);
    }
}

void MeetingView::addSession(const megachat::MegaChatSession &session)
{
    QListWidgetItem* item = new QListWidgetItem(sessionToString(session).c_str());
    mListWidget->addItem(item);
    assert(mSessionItems.find(session.getClientid()) == mSessionItems.end());
    mSessionItems[session.getClientid()] = item;
}

void MeetingView::removeSession(const megachat::MegaChatSession& session)
{
    auto it = mSessionItems.find(session.getClientid());
    if (it != mSessionItems.end())
    {
        QListWidgetItem* item = it->second;
        mListWidget->removeItemWidget(item);
        mSessionItems.erase(it);
        delete item;
    }
}

void MeetingView::updateSession(const megachat::MegaChatSession &session)
{
    auto it = mSessionItems.find(session.getClientid());
    if (it != mSessionItems.end())
    {
        QListWidgetItem* item = it->second;
        item->setText(sessionToString(session).c_str());
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
    }

    returnedString.append("/");

    returnedString.append(std::to_string(session.getClientid())).append("/");

    returnedString.append("A:").append(std::to_string(session.hasAudio())).append("/");
    returnedString.append("V:").append(std::to_string(session.hasVideo())).append("/");
    returnedString.append("ReqS:").append(std::to_string(session.hasRequestSpeak())).append("/");

    return returnedString;
}

void MeetingView::onHangUp()
{
    mMegaChatApi.hangChatCall(mChatid);
}

void MeetingView::onSessionContextMenu(const QPoint &pos)
{
    QPoint globalPoint = mListWidget->mapToGlobal(pos);
    QListWidgetItem* item = mListWidget->itemAt(pos);
    if (!item)
    {
        return;
    }

    QString text = item->text();
    QStringList textList = text.split('/');
    uint32_t cid = textList[1].toUInt();

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
            //mMegaChatApi.requestHiResVideo(mChatid, cid);
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

