#include "meetingSession.h"

MeetingSession::MeetingSession(MeetingView *meetingView, const megachat::MegaChatSession &session)
    : QWidget(meetingView)
{
    mMeetingView = meetingView;
    updateWidget(session);
    show();
}

MeetingSession::~MeetingSession()
{

}

void MeetingSession::updateWidget(const megachat::MegaChatSession &session)
{
    if (mLayout)
    {
        // remove widgets from current layout if exists
        assert(layout());
        if (mStatusLabel)    {layout()->removeWidget(mStatusLabel.get());     mStatusLabel->clear();}
        if (mTitleLabel)     {layout()->removeWidget(mTitleLabel.get());      mTitleLabel->clear();}
        if (mAudioLabel)     {layout()->removeWidget(mAudioLabel.get());      mAudioLabel->clear();}
        if (mVideoLabel)     {layout()->removeWidget(mVideoLabel.get());      mVideoLabel->clear();}
        if (mReqSpealLabel)  {layout()->removeWidget(mReqSpealLabel.get());   mReqSpealLabel->clear();}
        if (mModeratorLabel) {layout()->removeWidget(mModeratorLabel.get());  mModeratorLabel->clear();}
    }

    mLayout.reset(new QHBoxLayout());
    mLayout->setAlignment(Qt::AlignLeft);
    setLayout(mLayout.get());
    mCid = static_cast<uint32_t>(session.getClientid());

    if (session.isModerator())
    {
        // Moderator lbl
        mModeratorLabel.reset(new QLabel());
        mModeratorLabel->setText(QString::fromUtf8("<span style='font-size:20px'>\xE2\x99\x9A</span>"));
        layout()->addWidget(mModeratorLabel.get());
    }

    // status lbl
    QPixmap statusImg = session.isOnHold()
            ? QApplication::style()->standardPixmap(QStyle::SP_MediaPause)
            : QApplication::style()->standardPixmap(QStyle::SP_MediaPlay);

    mStatusLabel.reset(new QLabel());
    mStatusLabel->setPixmap(statusImg);
    layout()->addWidget(mStatusLabel.get());

    // title lbl
    std::string title = mMeetingView->sessionToString(session);
    mTitleLabel.reset(new QLabel(title.c_str()));
    layout()->addWidget(mTitleLabel.get());
    setToolTip(title.c_str());

    // audio lbl
    mAudio = session.hasAudio();
    QPixmap pixMap = mAudio
           ? QApplication::style()->standardPixmap(QStyle::SP_MediaVolume)
           : QApplication::style()->standardPixmap(QStyle::SP_MediaVolumeMuted);

    mAudioLabel.reset(new QLabel());
    mAudioLabel->setPixmap(pixMap);
    layout()->addWidget(mAudioLabel.get());

    // video lbl
    mVideo = session.hasVideo();
    QPixmap auxPixMap = mVideo
           ? QApplication::style()->standardPixmap(QStyle::SP_DialogYesButton)
           : QApplication::style()->standardPixmap(QStyle::SP_DialogNoButton);

    mVideoLabel.reset(new QLabel());
    mVideoLabel->setPixmap(auxPixMap);
    layout()->addWidget(mVideoLabel.get());

    // reqSpeak lbl
    mRequestSpeak = session.hasRequestSpeak();
    if (mRequestSpeak)
    {
       mReqSpealLabel.reset(new QLabel());
       mReqSpealLabel->setPixmap(QApplication::style()->standardPixmap(QStyle::SP_MessageBoxQuestion));
       layout()->addWidget(mReqSpealLabel.get());
    }
}

QListWidgetItem *MeetingSession::getWidgetItem() const
{
    return mListWidgetItem;
}

void MeetingSession::setOnHold(bool isOnhold)
{
    assert(mStatusLabel);
    QPixmap statusImg = isOnhold
            ? QApplication::style()->standardPixmap(QStyle::SP_MediaPause)
            : QApplication::style()->standardPixmap(QStyle::SP_MediaPlay);

    mStatusLabel->setPixmap(statusImg);
}

void MeetingSession::setWidgetItem(QListWidgetItem *listWidgetItem)
{
    mListWidgetItem = listWidgetItem;
}
