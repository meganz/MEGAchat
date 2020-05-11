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
    ui->globalChatDnd->setValidator(new QIntValidator(0, 31536000, this)); // Max value -> seconds in a year
    connect(ui->globalChatNotificationsEnabled, SIGNAL(clicked(bool)), this, SLOT(onGlobalChatClicked(bool)));
    connect(ui->scheduleEnabled, SIGNAL(clicked(bool)), this, SLOT(onScheduleEnabled(bool)));
    if (!mApp->getNotificationSettings() || !mApp->getTimeZoneDetails())
    {
        ui->pushNotifications->setEnabled(false);
    }

    // presence config UI
    ui->presence->setEnabled(false);
}

SettingWindow::~SettingWindow()
{
    delete mPresenceConfig;
    delete ui;
}

void SettingWindow::show()
{
    onPushNotificationSettingsUpdate();
    onPresenceConfigUpdate();
    QDialog::show();
}

void SettingWindow::onPushNotificationSettingsUpdate()
{
    ui->pushNotifications->setEnabled(false);

    auto notificationSettings = mApp->getNotificationSettings();
    auto timeZoneDetails = mApp->getTimeZoneDetails();

    if (!notificationSettings || !timeZoneDetails)
    {
        return;
    }

    ::mega::m_time_t now = ::mega::m_time(NULL);
    ui->pcr->setChecked(notificationSettings->isContactsEnabled());
    ui->shares->setChecked(notificationSettings->isSharesEnabled());

    ui->globalNotificationsEnabled->setChecked(notificationSettings->isGlobalEnabled());
    mGlobalDifference = notificationSettings->getGlobalDnd() - now;
    mGlobalDifference = (mGlobalDifference >= 0) ? mGlobalDifference : 0;
    std::string globalDnd = std::to_string(mGlobalDifference);
    ui->globalDnd->setText(globalDnd.c_str());
    ui->globalDnd->setEnabled(!notificationSettings->isGlobalEnabled());

    ui->globalChatNotificationsEnabled->setChecked(notificationSettings->isChatsEnabled());
    mGlobalChatsDifference = notificationSettings->getGlobalChatsDnd() - now;
    mGlobalChatsDifference = (mGlobalChatsDifference >= 0) ? mGlobalChatsDifference : 0;
    std::string globalChatsDnd = std::to_string(mGlobalChatsDifference);
    ui->globalChatDnd->setText(globalChatsDnd.c_str());
    ui->globalChatDnd->setEnabled(!notificationSettings->isChatsEnabled());

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

void SettingWindow::onPresenceConfigUpdate()
{
    ui->presence->setEnabled(false);

    delete mPresenceConfig;
    mPresenceConfig = mApp->megaChatApi()->getPresenceConfig();

    if (!mPresenceConfig || mPresenceConfig->isPending())
    {
        return;
    }

    ui->statusComboBox->setCurrentIndex(mPresenceConfig->getOnlineStatus() - 1);
    ui->autoAwayCheckBox->setChecked(mPresenceConfig->isAutoawayEnabled());
    ui->autoAwayTimeoutSLineEdit->setText(QString::number(mPresenceConfig->getAutoawayTimeout()));
    ui->persistStatusCheckBox->setChecked(mPresenceConfig->isPersist());
    ui->showLastGreenCheckBox->setChecked(mPresenceConfig->isLastGreenVisible());

    ui->presence->setEnabled(true);
}

void SettingWindow::savePushNotificationSettings()
{
    bool updated = false;
    ::mega::m_time_t now = ::mega::m_time(NULL);
    int globalDnd = ui->globalDnd->text().toInt();
    int globalChatsDnd = ui->globalChatDnd->text().toInt();

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

    if (ui->globalChatNotificationsEnabled->isChecked() != notificationSettings->isChatsEnabled()
            || (!ui->globalChatNotificationsEnabled->isChecked() && mGlobalChatsDifference != globalChatsDnd))
    {
        updated = true;
        // If we want to enable global chat notifications by setting mGlobalChatsDND to -1
        if (ui->globalChatNotificationsEnabled->isChecked())
        {
            notificationSettings->enableChats(true);
        }
        else
        {
            if (globalChatsDnd)
            {   // If we want to set a valid do not disturb period
                notificationSettings->setChatsDnd(globalChatsDnd + now);
            }
            else
            {   // If we want to disable global chat notifications by setting mGlobalChatsDND to 0
                notificationSettings->enableChats(false);
            }
        }
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

void SettingWindow::savePresenceConfig()
{
    bool autoawayEnabled = ui->autoAwayCheckBox->isChecked();
    int autoawayTimeout = ui->autoAwayTimeoutSLineEdit->text().toInt();
    if (autoawayEnabled != mPresenceConfig->isAutoawayEnabled()
            || autoawayTimeout != mPresenceConfig->getAutoawayTimeout())
    {
        mApp->megaChatApi()->setPresenceAutoaway(autoawayEnabled, autoawayTimeout);
    }

    int status = ui->statusComboBox->currentIndex() + 1;
    if (status != mPresenceConfig->getOnlineStatus())
    {
        mApp->megaChatApi()->setOnlineStatus(status);
    }

    bool showLastGreen = ui->showLastGreenCheckBox->isChecked();
    if (showLastGreen != mPresenceConfig->isLastGreenVisible())
    {
        mApp->megaChatApi()->setLastGreenVisible(showLastGreen);
    }
}

void SettingWindow::onClicked(QAbstractButton *button)
{
    QPushButton *pushButton = static_cast<QPushButton*>(button);
    if (pushButton == ui->confirmButtons->button(QDialogButtonBox::Apply))
    {
        savePushNotificationSettings();
        savePresenceConfig();
    }
    else if (pushButton == ui->confirmButtons->button(QDialogButtonBox::Reset))
    {
        onPushNotificationSettingsUpdate();
        onPresenceConfigUpdate();
    }
    else if (pushButton == ui->confirmButtons->button(QDialogButtonBox::Close))
    {
        hide();
        ui->pushNotifications->setEnabled(false);
    }
}

void SettingWindow::onGlobalClicked(bool value)
{
    ui->globalDnd->setEnabled(!value);
}

void SettingWindow::onGlobalChatClicked(bool value)
{
    ui->globalChatDnd->setEnabled(!value);
}

void SettingWindow::onScheduleEnabled(bool value)
{
    ui->startTime->setEnabled(value);
    ui->endTime->setEnabled(value);
    ui->timeZones->setEnabled(value);
}

void SettingWindow::on_autoAwayCheckBox_clicked(bool checked)
{
    if (checked)
    {
        ui->persistStatusCheckBox->setChecked(false);
        ui->statusComboBox->setCurrentIndex(MegaChatApi::STATUS_ONLINE - 1);
    }
}
