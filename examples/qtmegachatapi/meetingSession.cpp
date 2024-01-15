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
        if (mRecordingLabel) {layout()->removeWidget(mRecordingLabel.get());  mRecordingLabel->clear();}
        if (mSpkPermLabel)   {layout()->removeWidget(mSpkPermLabel.get());  mSpkPermLabel->clear();}
    }

    mLayout.reset(new QHBoxLayout());
    mLayout->setAlignment(Qt::AlignLeft);
    setLayout(mLayout.get());
    mCid = static_cast<uint32_t>(session.getClientid());
    mUserid = session.getPeerid();

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

    // recording lbl
    if (session.isRecording())
    {
        mRecordingLabel.reset(new QLabel("<span style='font-weight:bold; color:#A30000'>[REC]</span>"));
        layout()->addWidget(mRecordingLabel.get());
    }

    // title lbl
    std::string title = mMeetingView->sessionToString(session);
    if (session.isAudioDetected())
    {
        title.append("  Speaking");
    }
    else
    {
        title.append( "No Speaking");
    }

    mTitleLabel.reset(new QLabel(title.c_str()));
    layout()->addWidget(mTitleLabel.get());
    setToolTip(title.c_str());

    const auto chatid = mMeetingView->getChatid();
    std::unique_ptr<megachat::MegaChatCall> call(mMeetingView->megachatApi().getChatCall(chatid));
    if (!call)
    {
        assert(false); // call should exists at this point
        return;
    }

    const bool speakPermission = call->hasUserSpeakPermission(session.getPeerid());
    QPixmap spkPerPixMap = speakPermission
                               ? QApplication::style()->standardPixmap(QStyle::SP_DialogYesButton)
                               : QApplication::style()->standardPixmap(QStyle::SP_DialogNoButton);

    mSpkPermLabel.reset(new QLabel());
    mSpkPermLabel->setPixmap(spkPerPixMap);
    layout()->addWidget(mSpkPermLabel.get());

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
    mRequestSpeak = call->hasUserPendingSpeakRequest(session.getPeerid());
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
