#ifndef KARERE_DISABLE_WEBRTC
#ifndef MEETINGSESSION_H
#define MEETINGSESSION_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QStyle>
#include <QListWidgetItem>
#include <QFile>
#include <megachatapi.h>
#include "meetingView.h"

class MeetingView;
class MeetingSession : public QWidget
{
    Q_OBJECT
public:
    explicit MeetingSession(MeetingView *parent, const megachat::MegaChatSession &session);
    virtual ~MeetingSession();
    void setWidgetItem(QListWidgetItem *listWidgetItem);
    QListWidgetItem *getWidgetItem() const;
    void setOnHold(bool isOnhold);
    megachat::MegaChatHandle getUserId() { return mUserid; }
    void updateWidget(const megachat::MegaChatSession &session);

private:
    uint32_t mCid;
    bool mAudio;
    bool mVideo;
    bool mRequestSpeak;
    megachat::MegaChatHandle mUserid = megachat::MEGACHAT_INVALID_HANDLE;
    MeetingView *mMeetingView;
    QListWidgetItem *mListWidgetItem;
    std::unique_ptr <QHBoxLayout> mLayout;
    std::unique_ptr <QLabel> mModeratorLabel;
    std::unique_ptr <QLabel> mStatusLabel;
    std::unique_ptr <QLabel> mTitleLabel;
    std::unique_ptr <QLabel> mAudioLabel;
    std::unique_ptr <QLabel> mSpkPermLabel;
    std::unique_ptr <QLabel> mVideoLabel;
    std::unique_ptr <QLabel> mReqSpealLabel;
    std::unique_ptr <QLabel> mRecordingLabel;
    std::unique_ptr <QLabel> mRaiseHandLabel;
};

#endif // MEETINGSESSION_H
#endif
