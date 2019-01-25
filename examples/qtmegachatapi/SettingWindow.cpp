#include "SettingWindow.h"
#include "ui_SettingWindow.h"
#include "assert.h"
#include <mega/utils.h>

SettingWindow::SettingWindow(::mega::MegaApi *megaApi, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingWindow),
    mMegaApi(megaApi)
{
    ui->setupUi(this);

    ui->chatsView->setModel(&mModel);

    connect(ui->confirmButtons, SIGNAL(accepted()), this, SLOT(accepted()));
    connect(ui->confirmButtons, SIGNAL(rejected()), this, SLOT(rejected()));
    mMegaRequestDelegate = new ::mega::QTMegaRequestListener(mMegaApi, this);
    mMegaApi->fetchTimeZone(mMegaRequestDelegate);
}

SettingWindow::~SettingWindow()
{
    delete ui;
    delete mPushNotificationSettings;
    delete mTimeZoneDetails;
    delete mMegaRequestDelegate;
}

void SettingWindow::onRequestFinish(::mega::MegaApi* api, ::mega::MegaRequest *request, ::mega::MegaError* e)
{
    switch (request->getType())
    {
        case ::mega::MegaRequest::TYPE_GET_ATTR_USER:
            if (request->getParamType() == ::mega::MegaApi::USER_ATTR_PUSH_SETTINGS)
            {
                if (request->getMegaPushNotificationSettings())
                {
                    mPushNotificationSettings = request->getMegaPushNotificationSettings()->copy();
                    if (mPushNotificationSettings && mTimeZoneDetails)
                    {
                        ui->pushNotifications->setEnabled(true);
                        fillWidget();
                    }
                }
            }
            break;
        case ::mega::MegaRequest::TYPE_FETCH_TIMEZONE:
            mTimeZoneDetails = request->getMegaTimeZoneDetails();
            if (mPushNotificationSettings && mTimeZoneDetails)
            {
                ui->pushNotifications->setEnabled(true);
                fillWidget();
            }
            break;
        default:
            break;
    }
}

void SettingWindow::show()
{
    delete mPushNotificationSettings;
    mPushNotificationSettings = NULL;
    ui->pushNotifications->setEnabled(false);
    mMegaApi->getPushNotificationSettings(mMegaRequestDelegate);
    QDialog::show();
}

void SettingWindow::fillWidget()
{
    assert(mPushNotificationSettings);
    ::mega::m_time_t timestamp = ::mega::m_time(NULL);
    ui->chats->setChecked(mPushNotificationSettings->isChatsEnabled());
    ui->pcr->setChecked(mPushNotificationSettings->isContactsEnabled());
    ui->shares->setChecked(mPushNotificationSettings->isSharesEnabled());

    mGlobalDifference = mPushNotificationSettings->getGlobalDnd() - timestamp;
    mGlobalDifference = (mGlobalDifference >= 0) ? mGlobalDifference : -1;
    std::string globalDnd = std::to_string(mGlobalDifference);
    ui->globalDnd->setText(globalDnd.c_str());

    int scheduleTime = (mPushNotificationSettings->getGlobalScheduleStart() > 0) ? mPushNotificationSettings->getGlobalScheduleStart() : 0;
    int hours = scheduleTime / 60;
    int minutes = scheduleTime % 60;
    ui->startTime->setTime(QTime(hours, minutes));

    scheduleTime = (mPushNotificationSettings->getGlobalScheduleEnd() > 0) ? mPushNotificationSettings->getGlobalScheduleEnd() : 0;
    hours = scheduleTime / 60;
    minutes = scheduleTime % 60;
    ui->endTime->setTime(QTime(hours, minutes));

    mModel.clear();
    ::mega::MegaTextChatList *chatList = mMegaApi->getChatList();
    for (int i = 0; i < chatList->size(); i++)
    {
        ::mega::MegaHandle chatid = chatList->get(i)->getHandle();
        std::cerr << "chat ids: " << chatid << std::endl;
        if (mPushNotificationSettings->getChatDnd(chatid) > -1)
        {
            const char *chatid_64 = mMegaApi->handleToBase64(chatid);
            ::mega::m_time_t differenceChats = mPushNotificationSettings->getChatDnd(chatid) - timestamp;
            std::string rowContain = std::string(chatid_64) + " -> " + std::to_string(differenceChats);
            QStandardItem *chat = new QStandardItem(QString(rowContain.c_str()));
            mModel.appendRow(chat);
            delete [] chatid_64;
        }
    }

    delete chatList;
}

void SettingWindow::accepted()
{
    bool updated = false;
    ::mega::m_time_t timestamp = ::mega::m_time(NULL);
    QString stringDnd = ui->globalDnd->text();
    int globalDnd = stringDnd.toInt();
    if (mGlobalDifference != globalDnd)
    {
        mPushNotificationSettings->setGlobalDnd(globalDnd + timestamp);
        updated = true;
    }

    if (ui->chats->isChecked() != mPushNotificationSettings->isChatsEnabled())
    {
        mPushNotificationSettings->enableChats(ui->chats->isChecked());
        updated = true;
    }

    if (ui->pcr->isChecked() != mPushNotificationSettings->isContactsEnabled())
    {
        mPushNotificationSettings->enableContacts(ui->pcr->isChecked());
        updated = true;
    }

    if (ui->shares->isChecked() != mPushNotificationSettings->isSharesEnabled())
    {
        mPushNotificationSettings->enableShares(ui->shares->isChecked());
        updated = true;
    }

    QTime time = ui->startTime->time();
    int startTime = time.hour() * 60 + time.minute();
    startTime = (startTime == 0) ? -1 : startTime;
    time = ui->endTime->time();
    int endTime = time.hour() * 60 + time.minute();
    endTime = (endTime == 0) ? -1 : endTime;
    std::string timeZone;

    if (startTime != mPushNotificationSettings->getGlobalScheduleStart() || endTime != mPushNotificationSettings->getGlobalScheduleEnd())
    {
        mPushNotificationSettings->setGlobalSchedule(startTime, endTime, timeZone.c_str());
        updated = true;
    }

    if (updated)
    {
        mMegaApi->setPushNotificationSettings(mPushNotificationSettings);
    }
}

void SettingWindow::rejected()
{

}
