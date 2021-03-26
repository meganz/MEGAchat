#include "meetingSession.h"

MeetingSession::MeetingSession(MeetingView *meetingView, const megachat::MegaChatSession &session)
    : QWidget(meetingView)
{
    mMeetingView = meetingView;
    updateWidget(session);
    show();
}

void MeetingSession::updateWidget(const megachat::MegaChatSession &session)
{
    mCid = session.getClientid();
    mLayout.reset(new QHBoxLayout());
    mLayout->setAlignment(Qt::AlignLeft);
    setLayout(mLayout.get());

    // status lbl
    QPixmap statusImg = session.isOnHold()
            ? QApplication::style()->standardPixmap(QStyle::SP_MediaPause)
            : QApplication::style()->standardPixmap(QStyle::SP_MediaPlay);

    statusLabel.reset(new QLabel());
    statusLabel->setPixmap(statusImg);
    layout()->addWidget(statusLabel.get());

    // title lbl
    std::string title = mMeetingView->sessionToString(session);
    titleLabel.reset(new QLabel(title.c_str()));
    layout()->addWidget(titleLabel.get());

    // audio lbl
    mAudio = session.hasAudio();
    QPixmap pixMap = mAudio
           ? QApplication::style()->standardPixmap(QStyle::SP_MediaVolume)
           : QApplication::style()->standardPixmap(QStyle::SP_MediaVolumeMuted);

    audioLabel.reset(new QLabel());
    audioLabel->setPixmap(pixMap);
    layout()->addWidget(audioLabel.get());

    // video lbl
    mVideo = session.hasVideo();
    QPixmap auxPixMap = mVideo
           ? QApplication::style()->standardPixmap(QStyle::SP_DialogYesButton)
           : QApplication::style()->standardPixmap(QStyle::SP_DialogNoButton);

    videoLabel.reset(new QLabel());
    videoLabel->setPixmap(auxPixMap);
    layout()->addWidget(videoLabel.get());

    // reqSpeak lbl
    mRequestSpeak = session.hasRequestSpeak();
    if (mRequestSpeak)
    {
       reqSpealLabel.reset(new QLabel());
       reqSpealLabel->setPixmap(QApplication::style()->standardPixmap(QStyle::SP_MessageBoxQuestion));
       layout()->addWidget(reqSpealLabel.get());
    }
}

QListWidgetItem *MeetingSession::getWidgetItem() const
{
    return mListWidgetItem;
}

void MeetingSession::setOnHold(bool isOnhold)
{
    assert(statusLabel);
    QPixmap statusImg = isOnhold
            ? QApplication::style()->standardPixmap(QStyle::SP_MediaPause)
            : QApplication::style()->standardPixmap(QStyle::SP_MediaPlay);

    statusLabel->setPixmap(statusImg);
}

void MeetingSession::setWidgetItem(QListWidgetItem *listWidgetItem)
{
    mListWidgetItem = listWidgetItem;
}
