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
    void setWidgetItem(QListWidgetItem *listWidgetItem);
    QListWidgetItem *getWidgetItem() const;
    void setOnHold(bool isOnhold);
    void updateWidget(const megachat::MegaChatSession &session);

private:
    uint32_t mCid;
    bool mAudio;
    bool mVideo;
    bool mRequestSpeak;
    MeetingView *mMeetingView;
    QListWidgetItem *mListWidgetItem;
    std::unique_ptr <QHBoxLayout> mLayout;
    std::unique_ptr <QLabel> statusLabel;
    std::unique_ptr <QLabel> titleLabel;
    std::unique_ptr <QLabel> audioLabel;
    std::unique_ptr <QLabel> videoLabel;
    std::unique_ptr <QLabel> reqSpealLabel;
};

#endif // MEETINGSESSION_H
