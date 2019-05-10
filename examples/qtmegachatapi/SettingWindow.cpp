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
    init();
}

SettingWindow::~SettingWindow()
{
    delete ui;
    delete mMegaRequestDelegate;
    reset();
}

void SettingWindow::onRequestFinish(::mega::MegaApi* api, ::mega::MegaRequest *request, ::mega::MegaError* e)
{
    switch (request->getType())
    {
        case ::mega::MegaRequest::TYPE_GET_ATTR_USER:
            if (request->getParamType() == ::mega::MegaApi::USER_ATTR_PUSH_SETTINGS)
            {
                mPushNotificationSettings = request->getMegaPushNotificationSettings() ? request->getMegaPushNotificationSettings()->copy() : (::mega::MegaPushNotificationSettings::createInstance());
                if (mTimeZoneDetails)
                {
                    ui->pushNotifications->setEnabled(true);
                    fillWidget();
                }
            }
            break;
        case ::mega::MegaRequest::TYPE_FETCH_TIMEZONE:
        {
            ::mega::MegaTimeZoneDetails *aux = request->getMegaTimeZoneDetails();
            if (aux)
            {
                mTimeZoneDetails = request->getMegaTimeZoneDetails()->copy();
                if (mPushNotificationSettings && mTimeZoneDetails)
                {
                    ui->pushNotifications->setEnabled(true);
                    fillWidget();
                }
            }
            break;
        }
        default:
            break;
    }
}


void SettingWindow::reset()
{
    if (mPushNotificationSettings)
    {
        delete mPushNotificationSettings;
        mPushNotificationSettings = NULL;
    }

    if (mTimeZoneDetails)
    {
        delete mTimeZoneDetails;
        mTimeZoneDetails = NULL;
    }

    if (!mMegaRequestDelegate)
    {
        mMegaRequestDelegate = new ::mega::QTMegaRequestListener(mMegaApi, this);
    }
}

void SettingWindow::init()
{
    reset();
    mMegaApi->fetchTimeZone(mMegaRequestDelegate);
    mMegaApi->getPushNotificationSettings(mMegaRequestDelegate);

    ui->pushNotifications->setEnabled(false);
    connect(ui->confirmButtons, SIGNAL(accepted()), this, SLOT(accepted()));
    connect(ui->confirmButtons, SIGNAL(rejected()), this, SLOT(rejected()));
    ui->globalDnd->setValidator(new QIntValidator(0, 31536000, this)); // Max value -> seconds in a year
    connect(ui->globalNotificationsEnabled, SIGNAL(clicked(bool)), this, SLOT(onGlobalClicked(bool)));
    connect(ui->scheduleEnabled, SIGNAL(clicked(bool)), this, SLOT(onScheduleClicked(bool)));
}

void SettingWindow::show()
{
    QDialog::show();
}

void SettingWindow::fillWidget()
{
    assert(mPushNotificationSettings);
    ::mega::m_time_t timestamp = ::mega::m_time(NULL);
    ui->chats->setChecked(mPushNotificationSettings->isChatsEnabled());
    ui->pcr->setChecked(mPushNotificationSettings->isContactsEnabled());
    ui->shares->setChecked(mPushNotificationSettings->isSharesEnabled());

    ui->globalNotificationsEnabled->setChecked(mPushNotificationSettings->isGlobalEnabled());
    mGlobalDifference = mPushNotificationSettings->getGlobalDnd() - timestamp;
    mGlobalDifference = (mGlobalDifference >= 0) ? mGlobalDifference : 0;
    std::string globalDnd = std::to_string(mGlobalDifference);
    ui->globalDnd->setText(globalDnd.c_str());
    ui->globalDnd->setEnabled(!mPushNotificationSettings->isGlobalEnabled());

    int scheduleTime = (mPushNotificationSettings->getGlobalScheduleStart() > 0) ? mPushNotificationSettings->getGlobalScheduleStart() : 0;
    int hours = scheduleTime / 60;
    int minutes = scheduleTime % 60;
    ui->startTime->setTime(QTime(hours, minutes));

    QList<QString> stringsList;
    int index = 0;

    if (mTimeZoneDetails != NULL)
    {
        index = mTimeZoneDetails->getDefault();
        for (int i = 0; i < mTimeZoneDetails->getNumTimeZones(); i++)
        {
            const char *timeZone = mPushNotificationSettings->getGlobalScheduleTimezone();
            stringsList.append(QString(mTimeZoneDetails->getTimeZone(i)));

            if (strcmp(mTimeZoneDetails->getTimeZone(i), timeZone) == 0)
            {
                index = i;
            }

            delete [] timeZone;
        }

        ui->timeZones->addItems(stringsList);
        ui->timeZones->setCurrentIndex(index);

        scheduleTime = (mPushNotificationSettings->getGlobalScheduleEnd() > 0) ? mPushNotificationSettings->getGlobalScheduleEnd() : 0;
        hours = scheduleTime / 60;
        minutes = scheduleTime % 60;
        ui->endTime->setTime(QTime(hours, minutes));
        ui->scheduleEnabled->setChecked(mPushNotificationSettings->isGlobalScheduleEnabled());
        onScheduleClicked(mPushNotificationSettings->isGlobalScheduleEnabled());

        mModel.clear();
        ::mega::MegaTextChatList *chatList = mMegaApi->getChatList();
        for (int i = 0; i < chatList->size(); i++)
        {
            ::mega::MegaHandle chatid = chatList->get(i)->getHandle();
            if (mPushNotificationSettings->isChatDndEnabled(chatid))
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
}

void SettingWindow::accepted()
{
    bool updated = false;
    ::mega::m_time_t timestamp = ::mega::m_time(NULL);
    QString stringDnd = ui->globalDnd->text();
    int globalDnd = stringDnd.toInt();

    /* If globalNotifications checkbox changed respect initial value or if globalNotifications checkbox
       is disabled and do not disturb period has been changed */
    if (ui->globalNotificationsEnabled->isChecked() != mPushNotificationSettings->isGlobalEnabled()
            || !ui->globalNotificationsEnabled->isChecked() && mGlobalDifference != globalDnd)
    {
        updated = true;
        // If we want to enable global notifications by setting mGlobalDND to -1
        if (ui->globalNotificationsEnabled->isChecked())
        {
            mPushNotificationSettings->disableGlobalDnd();
        }
        else
        {
            if (globalDnd)
            {   // If we want to set a valid do not disturb period
                mPushNotificationSettings->setGlobalDnd(globalDnd + timestamp);
            }
            else
            {   // If we want to disable global notifications by setting mGlobalDND to 0
                mPushNotificationSettings->enableGlobal(false);
            }
        }
    }

    // Enable/disable notifications related to all chats
    if (ui->chats->isChecked() != mPushNotificationSettings->isChatsEnabled())
    {
        mPushNotificationSettings->enableChats(ui->chats->isChecked());
        updated = true;
    }

    // Enable/disable notifications related to all contacts
    if (ui->pcr->isChecked() != mPushNotificationSettings->isContactsEnabled())
    {
        mPushNotificationSettings->enableContacts(ui->pcr->isChecked());
        updated = true;
    }

    // Enable/disable notifications related to all shares
    if (ui->shares->isChecked() != mPushNotificationSettings->isSharesEnabled())
    {
        mPushNotificationSettings->enableShares(ui->shares->isChecked());
        updated = true;
    }

    QTime time = ui->startTime->time();
    int startTime = time.hour() * 60 + time.minute();
    time = ui->endTime->time();
    int endTime = time.hour() * 60 + time.minute();
    std::string timeZone = ui->timeZones->currentText().toStdString();
    const char *auxTimeZone = mPushNotificationSettings->getGlobalScheduleTimezone();

    // If schedule checkbox has changed
    // If schedule checkbox is enabled and start/end/timezone has changed
    if (ui->scheduleEnabled->isChecked() != mPushNotificationSettings->isGlobalScheduleEnabled() ||
            (ui->scheduleEnabled->isChecked() && (startTime != mPushNotificationSettings->getGlobalScheduleStart() ||
            endTime != mPushNotificationSettings->getGlobalScheduleEnd() || timeZone != auxTimeZone)))
    {
        updated = true;
        if (ui->scheduleEnabled->isChecked())
        {
            mPushNotificationSettings->setGlobalSchedule(startTime, endTime, timeZone.c_str());
        }
        else
        {
            mPushNotificationSettings->disableGlobalSchedule();
        }
    }

    // Update push notification settings
    if (updated)
    {
        mMegaApi->setPushNotificationSettings(mPushNotificationSettings);
    }

    delete [] auxTimeZone;
}

void SettingWindow::rejected()
{

}

void SettingWindow::onGlobalClicked(bool value)
{
    ui->globalDnd->setEnabled(!value);
}

void SettingWindow::onScheduleClicked(bool value)
{
    ui->startTime->setEnabled(value);
    ui->endTime->setEnabled(value);
    ui->timeZones->setEnabled(value);
}
