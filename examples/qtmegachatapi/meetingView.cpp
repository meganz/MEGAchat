#include "meetingView.h"

MeetingView::MeetingView(megachat::MegaChatApi &megaChatApi, mega::MegaHandle chatid, QWidget *parent)
    : QWidget(parent)
    , mMegaChatApi(megaChatApi)
    , mChatid(chatid)
{
    mGridLayout = new QGridLayout(this);
    QWidget* widgetThumbs = new QWidget();
    mThumbLayout = new QHBoxLayout(widgetThumbs);
    QWidget* widgetHiRes = new QWidget();
    mHiResLayout = new QHBoxLayout(widgetHiRes);
    mLocalLayout = new QHBoxLayout();
    mThumbView = new QScrollArea();
    mHiResView = new QScrollArea();
    mButtonsLayout = new QVBoxLayout();

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

    mGridLayout->addWidget(mThumbView, 0, 0, 1, 1);
    mGridLayout->addWidget(mHiResView, 1, 0, 1, 1);

    mButtonsLayout->addWidget(mHangup);
    mButtonsLayout->addWidget(mRequestSpeaker);
    mButtonsLayout->addWidget(mRequestModerator);
    mButtonsLayout->addWidget(mEnableAudio);
    mButtonsLayout->addWidget(mEnableVideo);
    mGridLayout->addLayout(mLocalLayout, 2, 0, 1, 1);
    mGridLayout->setRowStretch(0, 1);
    mGridLayout->setRowStretch(1, 3);
    mGridLayout->setRowStretch(2, 3);
    mLocalLayout->addLayout(mButtonsLayout);
}

void MeetingView::addVthumb(PeerWidget *widget)
{
    mThumbLayout->addWidget(widget);
    mThumbsWidget[widget->getCid()] = widget;
}

void MeetingView::addHiRes(PeerWidget *widget)
{
    mHiResLayout->addWidget(widget, 1);
    mHiResWidget[widget->getCid()] = widget;
}

void MeetingView::addLocalVideo(PeerWidget *widget)
{
    QHBoxLayout * localLayout = new QHBoxLayout();
    localLayout->addWidget(widget);
    mLocalLayout->addLayout(localLayout);
}

void MeetingView::removeThumb(Cid_t cid)
{
    auto it = mThumbsWidget.find(cid);
    assert(it != mThumbsWidget.end());
    removeThumb(it->second);
}

void MeetingView::removeHiRes(Cid_t cid)
{
    auto it = mThumbsWidget.find(cid);
    assert(it != mThumbsWidget.end());
    removeThumb(it->second);
}

void MeetingView::removeThumb(PeerWidget *widget)
{
    mThumbLayout->removeWidget(widget);
}

void MeetingView::removeHiRes(PeerWidget *widget)
{
    mHiResLayout->removeWidget(widget);
}

void MeetingView::onHangUp()
{
    mMegaChatApi.hangChatCall(mChatid);
}

