#include "SettingWindow.h"
#include "MegaChatApplication.h"
#include "ui_SettingWindow.h"
#include "assert.h"
#include <mega/utils.h>

SettingWindow::SettingWindow(MegaChatApplication *app) :
    QDialog(app->mainWindow()),
    ui(new Ui::SettingWindow),
    mApp(app)
{
    ui->setupUi(this);

    // notification settings UI
    ui->chatsView->setModel(&mNotificationSettingsPerChat);
    connect(ui->confirmButtons, SIGNAL(clicked(QAbstractButton*)), this, SLOT(onClicked(QAbstractButton*)));
    ui->globalDnd->setValidator(new QIntValidator(0, 31536000, this)); // Max value -> seconds in a year
    connect(ui->globalNotificationsEnabled, SIGNAL(clicked(bool)), this, SLOT(onGlobalClicked(bool)));
    connect(ui->scheduleEnabled, SIGNAL(clicked(bool)), this, SLOT(onScheduleEnabled(bool)));
    if (!mApp->getNotificationSettings() || !mApp->getTimeZoneDetails())
    {
        ui->pushNotifications->setEnabled(false);
    }
}

SettingWindow::~SettingWindow()
{
    delete ui;
}

void SettingWindow::show()
{
    onPushNotificationSettingsUpdate();
    QDialog::show();
}

void SettingWindow::onPushNotificationSettingsUpdate()
{
    auto notificationSettings = mApp->getNotificationSettings();
    auto timeZoneDetails = mApp->getTimeZoneDetails();
    assert(notificationSettings);
    assert(timeZoneDetails);

    ::mega::m_time_t now = ::mega::m_time(NULL);
    ui->chats->setChecked(notificationSettings->isChatsEnabled());
    ui->pcr->setChecked(notificationSettings->isContactsEnabled());
    ui->shares->setChecked(notificationSettings->isSharesEnabled());

    ui->globalNotificationsEnabled->setChecked(notificationSettings->isGlobalEnabled());
    mGlobalDifference = notificationSettings->getGlobalDnd() - now;
    mGlobalDifference = (mGlobalDifference >= 0) ? mGlobalDifference : 0;
    std::string globalDnd = std::to_string(mGlobalDifference);
    ui->globalDnd->setText(globalDnd.c_str());
    ui->globalDnd->setEnabled(!notificationSettings->isGlobalEnabled());

    ui->scheduleEnabled->setChecked(notificationSettings->isGlobalScheduleEnabled());
    onScheduleEnabled(notificationSettings->isGlobalScheduleEnabled());

    int scheduleTime = (notificationSettings->getGlobalScheduleStart() > 0) ? notificationSettings->getGlobalScheduleStart() : 0;
    int hours = scheduleTime / 60;
    int minutes = scheduleTime % 60;
    ui->startTime->setTime(QTime(hours, minutes));

    scheduleTime = (notificationSettings->getGlobalScheduleEnd() > 0) ? notificationSettings->getGlobalScheduleEnd() : 0;
    hours = scheduleTime / 60;
    minutes = scheduleTime % 60;
    ui->endTime->setTime(QTime(hours, minutes));

    QList<QString> stringsList;
    int index = timeZoneDetails->getDefault();
    const char *timeZone = notificationSettings->getGlobalScheduleTimezone();
    for (int i = 0; i < timeZoneDetails->getNumTimeZones(); i++)
    {
        stringsList.append(QString(timeZoneDetails->getTimeZone(i)));
        if (strcmp(timeZoneDetails->getTimeZone(i), timeZone) == 0)
        {
            index = i;
        }
    }
    delete [] timeZone;

    ui->timeZones->addItems(stringsList);
    ui->timeZones->setCurrentIndex(index);

    mNotificationSettingsPerChat.clear();
    ::mega::MegaTextChatList *chatList = mApp->megaApi()->getChatList();
    for (int i = 0; i < chatList->size(); i++)
    {
        ::mega::MegaHandle chatid = chatList->get(i)->getHandle();
        const char *chatid_64 = ::mega::MegaApi::userHandleToBase64(chatid);
        if (notificationSettings->isChatDndEnabled(chatid))
        {
            ::mega::m_time_t chatDND = notificationSettings->getChatDnd(chatid);
            std::string value = (chatDND > 0) ? std::to_string(chatDND - now) : "disabled (0)";
            std::string rowContain = std::string(chatid_64) + " -> " + value;
            QStandardItem *chat = new QStandardItem(QString(rowContain.c_str()));
            mNotificationSettingsPerChat.appendRow(chat);
        }
        if (notificationSettings->isChatAlwaysNotifyEnabled(chatid))
        {
            std::string rowContain = std::string(chatid_64) + " -> always-notify";
            QStandardItem *chat = new QStandardItem(QString(rowContain.c_str()));
            mNotificationSettingsPerChat.appendRow(chat);
        }
        delete [] chatid_64;
    }

    delete chatList;

    ui->pushNotifications->setEnabled(true);
}

void SettingWindow::savePushNotificationSettings()
{
    bool updated = false;
    ::mega::m_time_t now = ::mega::m_time(NULL);
    int globalDnd = ui->globalDnd->text().toInt();

    auto notificationSettings = mApp->getNotificationSettings();
    assert(notificationSettings);

    /* If globalNotifications checkbox changed respect initial value or if globalNotifications checkbox
       is disabled and do not disturb period has been changed */
    if (ui->globalNotificationsEnabled->isChecked() != notificationSettings->isGlobalEnabled()
            || (!ui->globalNotificationsEnabled->isChecked() && mGlobalDifference != globalDnd))
    {
        updated = true;
        // If we want to enable global notifications by setting mGlobalDND to -1
        if (ui->globalNotificationsEnabled->isChecked())
        {
            notificationSettings->disableGlobalDnd();
        }
        else
        {
            if (globalDnd)
            {   // If we want to set a valid do not disturb period
                notificationSettings->setGlobalDnd(globalDnd + now);
            }
            else
            {   // If we want to disable global notifications by setting mGlobalDND to 0
                notificationSettings->enableGlobal(false);
            }
        }
    }

    // Enable/disable notifications related to all chats
    if (ui->chats->isChecked() != notificationSettings->isChatsEnabled())
    {
        notificationSettings->enableChats(ui->chats->isChecked());
        updated = true;
    }

    // Enable/disable notifications related to all contacts
    if (ui->pcr->isChecked() != notificationSettings->isContactsEnabled())
    {
        notificationSettings->enableContacts(ui->pcr->isChecked());
        updated = true;
    }

    // Enable/disable notifications related to all shares
    if (ui->shares->isChecked() != notificationSettings->isSharesEnabled())
    {
        notificationSettings->enableShares(ui->shares->isChecked());
        updated = true;
    }

    QTime time = ui->startTime->time();
    int startTime = time.hour() * 60 + time.minute();
    time = ui->endTime->time();
    int endTime = time.hour() * 60 + time.minute();
    std::string timeZone = ui->timeZones->currentText().toStdString();
    const char *auxTimeZone = notificationSettings->getGlobalScheduleTimezone();

    // If schedule checkbox has changed
    // If schedule checkbox is enabled and start/end/timezone has changed
    if (ui->scheduleEnabled->isChecked() != notificationSettings->isGlobalScheduleEnabled()
            || (ui->scheduleEnabled->isChecked()
                && (startTime != notificationSettings->getGlobalScheduleStart()
                    || endTime != notificationSettings->getGlobalScheduleEnd()
                    || timeZone != auxTimeZone) ) )
    {
        updated = true;
        if (ui->scheduleEnabled->isChecked())
        {
            notificationSettings->setGlobalSchedule(startTime, endTime, timeZone.c_str());
        }
        else
        {
            notificationSettings->disableGlobalSchedule();
        }
    }

    // Update push notification settings
    if (updated)
    {
        mApp->megaApi()->setPushNotificationSettings(notificationSettings.get());
    }

    delete [] auxTimeZone;
}

void SettingWindow::onClicked(QAbstractButton *button)
{
    if (button->text() == "Apply")
    {
        savePushNotificationSettings();
    }
    else if (button->text() == "Reset")
    {
        onPushNotificationSettingsUpdate();
    }
    else if (button->text() == "Close")
    {
        hide();
        ui->pushNotifications->setEnabled(false);
    }
}

void SettingWindow::onGlobalClicked(bool value)
{
    ui->globalDnd->setEnabled(!value);
}

void SettingWindow::onScheduleEnabled(bool value)
{
    ui->startTime->setEnabled(value);
    ui->endTime->setEnabled(value);
    ui->timeZones->setEnabled(value);
}
