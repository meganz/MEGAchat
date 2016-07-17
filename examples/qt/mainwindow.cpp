#include "mainwindow.h"
#include <ui_mainwindow.h>
#include <ui_loginDialog.h>
#include <ui_settings.h>
#include "qmessagebox.h"
#include <string>
#include <videoRenderer_Qt.h>
#include <gcm.h>
#include "rtcModule/IRtcModule.h"
#include <iostream>
//#include <rapidjson/document.h>
#include <sdkApi.h>
#include <chatClient.h>
#include <QFont>
#include <QPainter>
#include <QImage>
#include <math.h>

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;

using namespace std;
using namespace mega;
using namespace karere;
using namespace promise;
using namespace mega;
QChar kOnlineSymbol_InProgress(0x267a);
QChar kOnlineSymbol_Set(0x25cf);
QString kOnlineStatusBtnStyle = QStringLiteral(
    u"color: %1;"
    "border: 0px;"
    "border-radius: 2px;"
    "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1,"
        "stop:0 rgba(100,100,100,255),"
        "stop:1 rgba(160,160,160,255));");

class LoginDialog: public QDialog, public karere::IGui::ILoginDialog
{
    Q_OBJECT
    Ui::LoginDialog ui;
    promise::Promise<std::pair<std::string, std::string>> mPromise;
    static QString sLoginStageStrings[kLast+1];
public:
    LoginDialog(QWidget* parent): QDialog(parent)
    {
        ui.setupUi(this);
        ui.mOkBtn->setEnabled(false);
        connect(ui.mOkBtn, SIGNAL(clicked(bool)), this, SLOT(onOkBtn(bool)));
        connect(ui.mCancelBtn, SIGNAL(clicked(bool)), this, SLOT(onCancelBtn(bool)));
        connect(ui.mEmailInput, SIGNAL(textChanged(const QString&)), this, SLOT(onType(const QString&)));
        connect(ui.mPasswordInput, SIGNAL(textChanged(const QString&)), this, SLOT(onType(const QString&)));
        ui.mPasswordInput->installEventFilter(this);
    }
    void enableControls(bool enable)
    {
        ui.mOkBtn->setEnabled(enable);
        ui.mCancelBtn->setEnabled(enable);
        ui.mEmailInput->setEnabled(enable);
        ui.mPasswordInput->setEnabled(enable);
    }
    virtual promise::Promise<std::pair<std::string, std::string>> requestCredentials()
    {
        if (!isVisible())
            show();
        else //reusing, recreate promise
        {
            enableControls(true);
            mPromise = promise::Promise<std::pair<std::string, std::string>>();
        }
        return mPromise;
    }
    virtual void setState(LoginStage state)
    {
        ui.mLoginStateDisplay->setStyleSheet((state == kBadCredentials)?"color:red":"color:black");
        ui.mLoginStateDisplay->setText(sLoginStageStrings[state]);
    }
public slots:
    void onOkBtn(bool)
    {
        if (mPromise.done())
            return;
        enableControls(false);
        mPromise.resolve(make_pair(
            ui.mEmailInput->text().toStdString(), ui.mPasswordInput->text().toStdString()));
    }
    void onCancelBtn(bool)
    {
        if (mPromise.done())
            return;
        mPromise.reject("Login dialog canceled by user");
    }
    void onType(const QString&)
    {
        QString email = ui.mEmailInput->text();
        bool enable = !email.isEmpty() && !ui.mPasswordInput->text().isEmpty();
        enable = enable & email.contains(QChar('@')) && email.contains(QChar('.'));
        if (enable != ui.mOkBtn->isEnabled())
            ui.mOkBtn->setEnabled(enable);
    }
};
QString LoginDialog::sLoginStageStrings[] = {
    tr("Authenticating"), tr("Bad credentials"), tr("Logging in"),
    tr("Fetching filesystem"), tr("Login complete")
};

MainWindow::MainWindow(Client* aClient): mClient(aClient)
{
    ui.setupUi(this);
    connect(ui.mSettingsBtn, SIGNAL(clicked(bool)), this, SLOT(onSettingsBtn(bool)));
    connect(ui.mOnlineStatusBtn, SIGNAL(clicked(bool)), this, SLOT(onOnlineStatusBtn(bool)));
//    setWindowFlags(Qt::CustomizeWindowHint|Qt::WindowTitleHint|Qt::WindowMaximizeButtonHint
//                   |Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint);
    ui.contactList->setSortingEnabled(true);
}
void MainWindow::onOnlineStatusBtn(bool)
{
    printf("click\n");
    auto list = new QMenu(this);
    auto actOnline = list->addAction("Online");
    actOnline->setData(QVariant(karere::Presence::kOnline));
    connect(actOnline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actAway = list->addAction("Away");
    actAway->setData(QVariant(karere::Presence::kAway));
    connect(actAway, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actDnd = list->addAction("Busy");
    actDnd->setData(QVariant(karere::Presence::kBusy));
    connect(actDnd, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actOffline = list->addAction("Offline");
    actOffline->setData(QVariant(karere::Presence::kOffline));
    connect(actOffline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto rect = ui.mOnlineStatusBtn->rect();
    list->move(mapToGlobal(QPoint(1,rect.bottom())));
    list->resize(rect.width(), 100);
    list->setStyleSheet("QMenu {"
        "background-color: qlineargradient("
        "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}"
        "QMenu::item:!selected{"
            "color: white;"
        "}"
        "QMenu::item:selected{"
            "background-color: qlineargradient("
            "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}");
    list->exec();
}
void MainWindow::onOwnPresence(Presence pres)
{
    ui.mOnlineStatusBtn->setText((pres.val() & Presence::kInProgress)
        ?kOnlineSymbol_InProgress
        :kOnlineSymbol_Set);
    ui.mOnlineStatusBtn->setStyleSheet(
        kOnlineStatusBtnStyle.arg(gOnlineIndColors[pres.status()]));
}

void MainWindow::setOnlineStatus()
{
    auto action = qobject_cast<QAction*>(QObject::sender());
    assert(action);
    bool ok;
    auto pres = action->data().toUInt(&ok);
    if (!ok || (pres > karere::Presence::kLast))
    {
        GUI_LOG_WARNING("setOnlineStatus: action data is not a valid presence code");
        return;
    }
    client().setPresence(pres);
}

SettingsDialog::SettingsDialog(MainWindow &parent)
    :QDialog(&parent), mMainWindow(parent)
{
    ui.setupUi(this);
    vector<string> audio;
    mMainWindow.client().rtc->getAudioInDevices(audio);
    for (auto& name: audio)
        ui.audioInCombo->addItem(name.c_str());
    mAudioInIdx = 0;

    vector<string> video;
    mMainWindow.client().rtc->getVideoInDevices(video);
    for (auto& name: video)
        ui.videoInCombo->addItem(name.c_str());
    mVideoInIdx = 0;
}

void SettingsDialog::applySettings()
{
    if (ui.audioInCombo->currentIndex() != mAudioInIdx)
        selectAudioInput();
    if (ui.videoInCombo->currentIndex() != mVideoInIdx)
        selectVideoInput();
}

void SettingsDialog::selectAudioInput()
{
    auto combo = ui.audioInCombo;
    string device = combo->itemText(combo->currentIndex()).toLatin1().data();
    bool ret = mMainWindow.client().rtc->selectAudioInDevice(device);
    if (!ret)
    {
        QMessageBox::critical(this, "Error", "Selected device not present");
        return;
    }
    KR_LOG_DEBUG("Selected audio device '%s'", device.c_str());
}

void SettingsDialog::selectVideoInput()
{
    auto combo = ui.videoInCombo;
    string device = combo->itemText(combo->currentIndex()).toLatin1().data();
    bool ret = mMainWindow.client().rtc->selectVideoInDevice(device);
    if (!ret)
    {
        QMessageBox::critical(this, "Error", "Selected device not present");
        return;
    }
    KR_LOG_DEBUG("Selected video device '%s'", device.c_str());
}

MainWindow::~MainWindow()
{}
QColor gAvatarColors[16] = {
    "aliceblue", "gold", "darkseagreen", "crimson",
    "firebrick", "lightsteelblue", "#70a1ff", "maroon",
    "cadetblue", "#db00d3", "darkturquoise", "lightblue",
    "honeydew", "lightyellow", "violet", "turquoise"
};

QString gOnlineIndColors[karere::Presence::kLast+1] =
{  "lightgray", "red", "orange", "lightgreen", "lightblue" };


karere::IGui::IContactGui*
MainWindow::createContactItem(karere::Contact& contact)
{
    auto clist = ui.contactList;
    auto contactGui = new CListContactItem(clist, contact);
    auto item = new QListWidgetItem;
    item->setSizeHint(contactGui->size());
    clist->addItem(item);
    clist->setItemWidget(item, contactGui);
    return contactGui;
}
karere::IGui::IContactGui*
MainWindow::createGroupChatItem(karere::GroupChatRoom& room)
{
    auto clist = ui.contactList;
    auto chatGui = new CListGroupChatItem(clist, room);
    auto item = new QListWidgetItem;
    item->setSizeHint(chatGui->size());
    clist->insertItem(0, item);
    clist->setItemWidget(item, chatGui);
    return chatGui;
}

void MainWindow::removeItem(IContactGui* item, bool isGroup)
{
    auto clist = ui.contactList;
    auto size = clist->count();
    for (int i=0; i<size; i++)
    {
        auto gui = static_cast<CListItem*>(clist->itemWidget(clist->item(i)));
        if (item == gui)
        {
            if (gui->isGroup() != isGroup)
                throw std::runtime_error("ContactList: removeItem: Asked to remove a different type of contactlist item");
            delete clist->takeItem(i);
            return;
        }
    }
    throw std::runtime_error("ContactList: removeItem: Item not found");
}
void MainWindow::removeGroupChatItem(IContactGui* item)
{
    removeItem(item, true);
}
void MainWindow::removeContactItem(IContactGui* item)
{
    removeItem(item, false);
}

karere::IGui::IChatWindow* MainWindow::createChatWindow(karere::ChatRoom& room)
{
    return new ChatWindow(room, *this);
}
karere::IGui::IChatWindow& MainWindow::chatWindowForPeer(uint64_t handle)
{
    auto it = mClient->contactList->find(handle);
    if (it == mClient->contactList->end())
        throw std::runtime_error("chatWindowForPeer: peer '"+Id(handle).toString()+"' not in contact list");
    auto room = it->second->chatRoom();
    if (!room)
        throw std::runtime_error("chatWindowForPeer: peer contact has no chatroom");
    return room->chatWindow();
}
karere::IGui::ILoginDialog* MainWindow::createLoginDialog()
{
    return new LoginDialog(nullptr);
}

void MainWindow::onIncomingContactRequest(const MegaContactRequest &req)
{
    const char* cMail = req.getSourceEmail();
    assert(cMail); //should be already checked by the caller
    QString mail = cMail;

    auto ret = QMessageBox::question(nullptr, tr("Incoming contact request"),
        tr("Contact request from %1, accept?").arg(mail));
    int action = (ret == QMessageBox::Yes)
            ? MegaContactRequest::REPLY_ACTION_ACCEPT
            : MegaContactRequest::REPLY_ACTION_DENY;
    client().api->call(&MegaApi::replyContactRequest, (MegaContactRequest*)&req, action)
    .fail([this, mail](const promise::Error& err)
    {
        QMessageBox::critical(nullptr, tr("Accept request"), tr("Error replying to contact request from ")+mail);
        return err;
    });
}
void MainWindow::onAddContact()
{
    auto email = QInputDialog::getText(this, tr("Add contact"), tr("Please enter the email of the user to add"));
    if (email.isNull())
        return;
    if (email == client().api->getMyEmail())
    {
        QMessageBox::critical(this, tr("Add contact"), tr("You can't add your own email as contact"));
        return;
    }
    auto utf8 = email.toUtf8();
    for (auto& item: *client().contactList)
    {
        if ((item.second->email() == utf8.data()) && (item.second->visibility() != ::mega::MegaUser::VISIBILITY_HIDDEN))
        {
            QMessageBox::critical(this, tr("Add contact"),
                tr("User with email '%1' already exists in your contactlist with screen name '%2'")
                .arg(email).arg(QString::fromStdString(item.second->titleString())));
            return;
        }
    }
    client().api->call(&MegaApi::inviteContact, email.toUtf8().data(), tr("I'd like to add you to my contact list").toUtf8().data(), MegaContactRequest::INVITE_ACTION_ADD)
    .fail([this, email](const promise::Error& err)
    {
        QString msg;
        if (err.code() == API_ENOENT)
            msg = tr("User with email '%1' does not exist").arg(email);
        else if (err.code() == API_EEXIST)
        {
            std::unique_ptr<MegaUser> user(client().api->getContact(email.toUtf8().data()));
            if (!user)
            {
                msg = tr("Bug: API said user exists in our contactlist, but SDK can't find it");
            }
            else
            {
                auto userid = user->getHandle();
                auto it = client().contactList->find(userid);
                if (it == client().contactList->end())
                {
                    msg = tr("Bug: API and SDK have the user, but karere contactlist can't find it");
                }
                else
                {
                    msg = tr("User with email '%1' already exists in your contactlist").arg(email);
                    if (it->second->titleString() != email.toUtf8().data())
                            msg.append(tr(" with the screen name '")).append(QString::fromStdString(it->second->titleString())).append('\'');
                }
            }
        }
        else if (err.code() == API_EARGS)
        {
            msg = tr("Invalid email address '%1'").arg(email);
        }
        else
        {
            msg = tr("Error inviting '")+email+tr("': ")+QLatin1String(err.what());
        }
        QMessageBox::critical(this, tr("Add contact"), msg);
        return err;
    });
}

QString prettyInterval(int64_t secs)
{
    enum {secsPerMonth = 86400*30, secsPerYear = 86400 * 365};
    if (secs < 60)
        return QObject::tr("%1 seconds").arg(secs);
    else if (secs < 3600)
        return QObject::tr("%1 minutes").arg(round(float(secs)/60));
    else if (secs < 86400)
        return QObject::tr("%1 hours").arg(secs/3600);
    else if (secs < secsPerMonth)
        return QObject::tr("%1 days").arg(round(float(secs)/86400));
    else if (secs < secsPerYear)
        return QObject::tr("%1 months %2 days").arg(secs/secsPerMonth).arg(round(secs%secsPerMonth)/86400);
    else
    {
        auto years = secs/secsPerYear;
        auto months = (secs % secsPerYear) / secsPerMonth;
        auto days = (secs % secsPerMonth) / 86400;
        return QObject::tr("%1 years %2 months %3 days").arg(years).arg(months).arg(days);
    }
}

void MainWindow::onSettingsBtn(bool)
{
    SettingsDialog dialog(*this);
    if (dialog.exec() == QDialog::Accepted)
        dialog.applySettings();
}

#include <mainwindow.moc>
