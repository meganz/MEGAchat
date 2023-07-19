#include "qmenu.h"
#include "chatMessage.h"
#include "ui_chatMessageWidget.h"
#include <QMessageBox>
#include <QClipboard>
#include "uiSettings.h"

using namespace megachat;

const char *messageStatus[] =
{
  "Sending", "SendingManual", "ServerReceived", "ServerRejected", "Delivered", "NotSeen", "Seen"
};

ChatMessage::ChatMessage(ChatWindow *parent, megachat::MegaChatApi *mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg)
    : QWidget((QWidget *)parent),
      ui(new Ui::ChatMessageWidget)
{
    mChatWindow=parent;
    this->mChatId=chatId;
    megaChatApi = mChatApi;
    ui->setupUi(this);
    mMessage = msg;
    setAuthor(NULL);
    setTimestamp(mMessage->getTimestamp());
    mReactWidget = new QWidget();
    ui->mReactions->setWidget(mReactWidget);
    mReactLayout = new QHBoxLayout();
    mReactWidget->setLayout(mReactLayout);
    mReactWidget->setStyleSheet("text-align:left");
    mReactWidget->setSizePolicy(QSizePolicy ::Expanding , QSizePolicy ::Expanding);
    mReactLayout->setAlignment(Qt::AlignLeft);
    ui->mReactions->setFrameShape(QFrame::NoFrame);

    megachat::MegaChatRoom *chatRoom = megaChatApi->getChatRoom(chatId);
    assert (chatRoom);
    if (chatRoom->isGroup() && mMessage->getStatus() == megachat::MegaChatMessage::STATUS_DELIVERED)
    {
        setStatus(megachat::MegaChatMessage::STATUS_SERVER_RECEIVED);
    }
    else
    {
        setStatus(mMessage->getStatus());
    }

    delete chatRoom;
    updateContent();

    switch(mMessage->getStatus())
    {
        case megachat::MegaChatMessage::STATUS_SERVER_RECEIVED:
        case megachat::MegaChatMessage::STATUS_DELIVERED:
        case megachat::MegaChatMessage::STATUS_SEEN:
        case megachat::MegaChatMessage::STATUS_NOT_SEEN:
        {
            mega::unique_ptr<::mega::MegaStringList> reactions(mChatWindow->mMegaChatApi->getMessageReactions(mChatId, mMessage->getMsgId()));
            for (int i = 0; i < reactions->size(); i++)
            {
                int count = megaChatApi->getMessageReactionCount(mChatId, mMessage->getMsgId(), reactions->get(i));
                Reaction *reaction = new Reaction(this, reactions->get(i), count);
                mReactWidget->layout()->addWidget(reaction); // takes ownership
            }
        }
    }

    connect(ui->mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
    updateToolTip();
    show();
}

ChatMessage::~ChatMessage()
{
    clearReactions();
    mReactLayout->deleteLater();
    mReactWidget->deleteLater();
    delete mMessage;
    delete ui;
}

const Reaction *ChatMessage::getLocalReaction(const char *reactionStr) const
{
    assert(reactionStr);
    for (int i = 0; i < mReactWidget->layout()->count(); i++)
    {
        QLayoutItem *item = mReactWidget->layout()->itemAt(i);
        Reaction *reaction = static_cast<Reaction*>(item->widget());
        if (reaction->getReactionString() == reactionStr)
        {
            return reaction;
        }
    }
    return nullptr;
}

void ChatMessage::updateReaction(const char *reactionStr, int count)
{
    assert(reactionStr);
    bool found = false;
    int size = mReactWidget->layout()->count();
    for (int i = 0; i < size; i++)
    {
        QLayoutItem *item = mReactWidget->layout()->itemAt(i);
        Reaction *reaction = static_cast<Reaction*>(item->widget());
        if (reaction->getReactionString() == reactionStr)
        {
            found = true;
            if (count == 0)
            {
                item->widget()->deleteLater();
                delete mReactWidget->layout()->takeAt(i);
            }
            else
            {
                reaction->updateReactionCount(count);
            }
            break;
        }
    }

    if (!found && count)
    {
        Reaction *reaction = new Reaction(this, reactionStr, count);
        mReactWidget->layout()->addWidget(reaction);
    }
}

void ChatMessage::updateToolTip()
{
    QString tooltip;
    megachat::MegaChatHandle msgId;
    std::string msgIdBin;

    int status = mMessage->getStatus();
    switch (status)
    {
    case megachat::MegaChatMessage::STATUS_SENDING:
        tooltip.append(tr("tempId: "));
        msgId = mMessage->getTempId();
        break;
    case megachat::MegaChatMessage::STATUS_SENDING_MANUAL:
        tooltip.append(tr("rowId: "));
        msgId = mMessage->getRowId();
        break;
    default:
        tooltip.append(tr("msgId: "));
        msgId = mMessage->getMsgId();
        msgIdBin = std::to_string(mMessage->getMsgId());
        break;
    }

    const char *auxMsgId_64 = mChatWindow->mMegaApi->userHandleToBase64(msgId);
    const char *auxUserId_64 = mChatWindow->mMegaApi->userHandleToBase64(mMessage->getUserHandle());
    tooltip.append(QString::fromStdString(auxMsgId_64))
            .append(tr("\nMsg handle bin: "))
            .append(msgIdBin.c_str())
            .append(tr("\ntype: "))
            .append(QString::fromStdString(std::to_string(mMessage->getType())))
            .append(tr("\nuserid: "))
            .append(QString::fromStdString(auxUserId_64));
    ui->mHeader->setToolTip(tooltip);
    delete [] auxMsgId_64;
    delete [] auxUserId_64;
}

void ChatMessage::showContainsMetaData()
{
    const MegaChatContainsMeta *containsMeta = mMessage->getContainsMeta();
    assert(containsMeta);
    QString text = tr("[Contains-metadata msg]");
    if (containsMeta)
    {
        switch (containsMeta->getType()) {
            case  megachat::MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW:
            {
                const MegaChatRichPreview *richPreview = containsMeta->getRichPreview();
                text.append(tr("\nSubtype: rich-link"))
                    .append(tr("\nOriginal content: "))
                    .append(richPreview->getText())
                    .append(tr("\nURL: "))
                    .append(richPreview->getUrl())
                    .append(tr("\nDomain Name: "))
                    .append(richPreview->getDomainName())
                    .append(tr("\nTitle: "))
                    .append(richPreview->getTitle())
                    .append(tr("\nDescription: "))
                    .append(richPreview->getDescription())
                    .append(tr("\nHas icon: "))
                    .append(richPreview->getIcon() ? "yes" : "no")
                    .append(tr("\nHas image: "))
                    .append(richPreview->getImage() ? "yes" : "no");
                break;
            }
            case  megachat::MegaChatContainsMeta::CONTAINS_META_GEOLOCATION:
            {
                const MegaChatGeolocation *geolocation = containsMeta->getGeolocation();
                text.append(tr("\nSubtype: Geolocation"))
                    .append(tr("\nText content: "))
                    .append(containsMeta->getTextMessage())
                    .append(tr("\nLongitude: "))
                    .append(QString::number(geolocation->getLongitude()))
                    .append(tr("\nLatitude: "))
                    .append(QString::number(geolocation->getLatitude()))
                    .append(tr("\nHas image: "))
                    .append(geolocation->getImage() ? "yes" : "no");
                break;
            }
        case  megachat::MegaChatContainsMeta::CONTAINS_META_GIPHY:
            {
                const MegaChatGiphy *giphy = containsMeta->getGiphy();
                text.append(tr("\nSubtype: Giphy"))
                    .append(tr("\nText content: "))
                    .append(containsMeta->getTextMessage())
                    .append(tr("\nTitle: "))
                    .append(giphy->getTitle())
                    .append(tr("\nMp4 source: "))
                    .append(giphy->getMp4Src())
                    .append(tr("\nWebp source: "))
                    .append(giphy->getWebpSrc())
                    .append(tr("\nMp4 Size: "))
                    .append(QString::number(giphy->getMp4Size()))
                    .append(tr("\nWebp Size: "))
                    .append(QString::number(giphy->getWebpSize()))
                    .append(tr("\ngiphy width: "))
                    .append(QString::number(giphy->getWidth()))
                    .append(tr("\ngiphy height: "))
                    .append(QString::number(giphy->getHeight()));
                break;
            }
            default:
            {
                text.append(tr("\nSubtype: unkown"))
                    .append(tr("\nText content: "))
                    .append(containsMeta->getTextMessage());
                break;
            }
        }
    }

    ui->mMsgDisplay->setText(text);
    ui->mMsgDisplay->setStyleSheet("background-color: rgba(213,245,160,128)\n");
    ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
    ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
    ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
}

QListWidgetItem *ChatMessage::getWidgetItem() const
{
    return mListWidgetItem;
}

void ChatMessage::setWidgetItem(QListWidgetItem *item)
{
    mListWidgetItem = item;
}

megachat::MegaChatMessage *ChatMessage::getMessage() const
{
    return mMessage;
}

void ChatMessage::setMessage(megachat::MegaChatMessage *message)
{
    if (mMessage)
    {
        delete mMessage;
    }

    this->mMessage = message;
}

void ChatMessage::setMessageContent(const char *content)
{
    ui->mMsgDisplay->setText(content);
}

QString ChatMessage::nodelistText()
{
    QString text;
    ::mega::MegaNodeList *nodeList = mMessage->getMegaNodeList();
    if (!nodeList)
    {
        return text;
    }

    for(int i = 0; i < nodeList->size(); i++)
    {
        const char *auxNodeHandle_64 = mChatWindow->mMegaApi->handleToBase64(nodeList->get(i)->getHandle());
        text.append("\n[Node").append(std::to_string(i+1).c_str()).append("]")
        .append("\nHandle: ")
        .append(QString::fromStdString(auxNodeHandle_64))
        .append("\nName: ")
        .append(nodeList->get(i)->getName())
        .append("\nSize: ")
        .append(QString::fromStdString(std::to_string(nodeList->get(i)->getSize())))
        .append(" bytes");
        delete [] auxNodeHandle_64;
    }
    return text;
}

megachat::MegaChatHandle ChatMessage::getChatId() const
{
    return mChatId;
}

megachat::MegaChatApi *ChatMessage::getMegaChatApi() const
{
    return megaChatApi;
}

ChatWindow *ChatMessage::getChatWindow() const
{
    return mChatWindow;
}

void ChatMessage::clearReactions()
{
    QLayoutItem *item;
    while ((item = mReactWidget->layout()->takeAt(0)))
    {
        item->widget()->deleteLater();
        delete item;
    }
}

void ChatMessage::updateContent()
{
    if (mMessage->isEdited())
        markAsEdited();

    if (!mMessage->isManagementMessage())
    {
        switch (mMessage->getType())
        {
            case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
            {
                QString text;
                text.append(tr("[Nodes attachment msg]"));
                text.append(nodelistText());
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(198,251,187,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->bSettings->show();
                break;
            }
            case megachat::MegaChatMessage::TYPE_VOICE_CLIP:
            {
                QString text;
                text.append(tr("[Voice clip msg]"));
                text.append(nodelistText());
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(229,66,244,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->bSettings->show();
                break;
            }
            case megachat::MegaChatMessage::TYPE_CONTACT_ATTACHMENT:
            {
                QString text;
                text.append(tr("[Contacts attachment msg]"));
                for(unsigned int i = 0; i < mMessage->getUsersCount(); i++)
                {
                  const char *auxUserHandle_64 =this->mChatWindow->mMegaApi->userHandleToBase64(mMessage->getUserHandle(i));
                  text.append(tr("\n[User]"))
                  .append("\nHandle: ")
                  .append(auxUserHandle_64)
                  .append("\nName: ")
                  .append(mMessage->getUserName(i))
                  .append("\nEmail: ")
                  .append(mMessage->getUserEmail(i));
                  delete [] auxUserHandle_64;
                }
                ui->mMsgDisplay->setText(text);
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(205,254,251,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                text.clear();
                break;
            }
            case megachat::MegaChatMessage::TYPE_NORMAL:
            {
                ui->mMsgDisplay->setStyleSheet("background-color: rgba(255,255,255,128)\n");
                ui->mHeader->setStyleSheet("background-color: rgba(107,144,163,128)\n");
                ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
                setMessageContent(mMessage->getContent());
                break;
            }
            case megachat::MegaChatMessage::TYPE_CONTAINS_META:
            {
                showContainsMetaData();
                break;
            }
            case megachat::MegaChatMessage::TYPE_INVALID:
            {
                int errorCode = mMessage->getCode();
                std::string content = "Invalid message [warn]: - (";
                if (errorCode == MegaChatMessage::INVALID_SIGNATURE)
                    content.append("invalid signature");
                else if (errorCode == MegaChatMessage::INVALID_FORMAT)
                    content.append("malformed");
                else
                    content.append(std::to_string(errorCode));
                content.append(")\nContent: ");
                if (mMessage->getContent())
                    content.append(mMessage->getContent());
                setMessageContent(content.c_str());
                break;
            }
            case megachat::MegaChatMessage::TYPE_UNKNOWN:
            {
                int errorCode = mMessage->getCode();
                std::string content = "Unknown type [hide]: - (";
                if (errorCode == MegaChatMessage::INVALID_KEY)
                    content.append("invalid key");
                else if (errorCode == MegaChatMessage::DECRYPTING)
                    content.append("decrypting");
                else if (errorCode == MegaChatMessage::INVALID_TYPE)
                    content.append("invalid type");
                else
                    content.append(std::to_string(errorCode));
                content.append(")\nContent: ");
                if (mMessage->getContent())
                    content.append(mMessage->getContent());
                setMessageContent(content.c_str());
                break;
            }
        }
    }
    else
    {
        ui->mMsgDisplay->setText(managementInfoToString().c_str());
        ui->mHeader->setStyleSheet("background-color: rgba(192,123,11,128)\n");
        ui->mAuthorDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
        ui->mTimestampDisplay->setStyleSheet("color: rgba(0,0,0,128)\n");
    }
}

std::string ChatMessage::managementInfoToString() const
{
    std::string ret;
    ret.reserve(128);
    const char *userHandle_64 = this->mChatWindow->mMegaApi->userHandleToBase64(mMessage->getUserHandle());
    const char *actionHandle_64 = this->mChatWindow->mMegaApi->userHandleToBase64(mMessage->getHandleOfAction());

    switch (mMessage->getType())
    {
        case megachat::MegaChatMessage::TYPE_ALTER_PARTICIPANTS:
        {
            ret.append("User ").append(userHandle_64)
               .append((mMessage->getPrivilege() == megachat::MegaChatRoom::PRIV_RM) ? " removed" : " added")
               .append(" user ").append(actionHandle_64);
            break;
        }
        case megachat::MegaChatMessage::TYPE_TRUNCATE:
        {
            ChatListItemController *itemController = mChatWindow->mMainWin->getChatControllerById(mChatId);
            if(itemController)
            {
               ChatItemWidget *widget = itemController->getWidget();
               if (widget)
               {
                  widget->updateToolTip(itemController->getItem());
                  ret.append("Chat history was truncated by user ").append(userHandle_64);
               }
            }
            break;
        }
        case megachat::MegaChatMessage::TYPE_PRIV_CHANGE:
        {
            ret.append("User ").append(userHandle_64)
               .append(" set privilege of user ").append(actionHandle_64)
               .append(" to ").append(std::to_string(mMessage->getPrivilege()));
            break;
        }
        case megachat::MegaChatMessage::TYPE_CHAT_TITLE:
        {
            ret.append("User ").append(userHandle_64)
               .append(" set chat title to '")
               .append(mMessage->getContent())+='\'';
            break;
        }
        case megachat::MegaChatMessage::TYPE_CALL_ENDED:
        {
            ret.append("User ").append(userHandle_64);
            ret.append(" called user/s: ");

            ::mega::MegaHandleList *handleList = mMessage->getMegaHandleList();
            for (unsigned int i = 0; i < handleList->size(); i++)
            {
                char *participant_64 = mChatWindow->mMegaApi->userHandleToBase64(handleList->get(i));
                ret.append(participant_64).append(", ");
                delete [] participant_64;
            }
            if (handleList->size())
            {
                ret = ret.substr(0, ret.size() - 2);
            }

            ret.append("\nDuration: ")
               .append(std::to_string(mMessage->getDuration()))
               .append("s. TermCode: ")
               .append(std::to_string(mMessage->getTermCode()));
            break;
        }
        case megachat::MegaChatMessage::TYPE_CALL_STARTED:
        {
            ret.append("User ").append(userHandle_64)
               .append(" has started a call");
            break;
        }
        case megachat::MegaChatMessage::TYPE_PUBLIC_HANDLE_CREATE:
        {
            ret.append("User ").append(userHandle_64)
               .append(" created a public handle ");
            break;
        }
        case megachat::MegaChatMessage::TYPE_PUBLIC_HANDLE_DELETE:
        {
            ret.append("User ").append(userHandle_64)
                    .append(" removed a public handle ");
            break;
        }
        case megachat::MegaChatMessage::TYPE_SET_PRIVATE_MODE:
        {
            ret.append("User ").append(userHandle_64)
                    .append(" converted chat into private mode ");
            break;
        }
        case megachat::MegaChatMessage::TYPE_SET_RETENTION_TIME:
        {
            ret.append("User ").append(userHandle_64)
                    .append(" set retention time ")
                    .append(std::to_string(mMessage->getPrivilege()))
                    .append(" seconds");
            break;
        }
        case megachat::MegaChatMessage::TYPE_SCHED_MEETING:
        {
            auto isEmpty = [](const mega::MegaStringList* l) -> bool
            {
                if (!l || !l->size()) { return true; }
                for (int i = 0; i < l->size(); ++i)
                {
                    if (strlen(l->get(i)))
                    {
                        return false;
                    }
                }
                return true;
            };

            std::string changeSet;
            std::string changeListStr;
            auto getChangeListStr = [this, &isEmpty, &changeSet, &changeListStr](const unsigned int change, std::string changeStr) -> void
            {
                changeSet.append(" ").append(changeStr).append(": ").append(std::to_string(mMessage->hasSchedMeetingChanged(change)));
                const mega::MegaStringList* l = mMessage->getScheduledMeetingChange(change);
                if (isEmpty(l)) { return; }

                changeListStr.append("\n * ").append(changeStr).append("=>");
                changeListStr.append(" Old: ").append(l->get(0));
                if (l->size() == 2)
                {
                    changeListStr.append(" New: ").append(l->get(1));
                }
            };

            getChangeListStr(MegaChatScheduledMeeting::SC_NEW_SCHED, "NEW");
            getChangeListStr(MegaChatScheduledMeeting::SC_PARENT, "p");
            getChangeListStr(MegaChatScheduledMeeting::SC_TZONE,  "tz");
            getChangeListStr(MegaChatScheduledMeeting::SC_START,  "s");
            getChangeListStr(MegaChatScheduledMeeting::SC_END,    "e");
            getChangeListStr(MegaChatScheduledMeeting::SC_TITLE,  "t");
            getChangeListStr(MegaChatScheduledMeeting::SC_ATTR,   "at");
            getChangeListStr(MegaChatScheduledMeeting::SC_FLAGS,  "f");
            getChangeListStr(MegaChatScheduledMeeting::SC_RULES,  "r");

            ret.append("User ").append(userHandle_64).append(" ");
            if (mMessage->hasSchedMeetingChanged(MegaChatScheduledMeeting::SC_NEW_SCHED))
            {
                ret.append("created new scheduled meeting: ") .append(actionHandle_64);
            }
            else
            {
                ret.append("modified scheduled meeting: ") .append(actionHandle_64)
                    .append("\nchangeset: ") .append(changeSet)
                    .append("\nchanges: ") .append(changeListStr);
            }
            break;
        }
        default:
        {
            ret.append("Management message with unknown type: ")
               .append(std::to_string(mMessage->getType()));
            break;
        }
    }
    delete [] userHandle_64;
    delete [] actionHandle_64;
    return ret;
}

void ChatMessage::setTimestamp(int64_t ts)
{
    QDateTime t;
    t.setTime_t(static_cast<unsigned int>(ts));
    ui->mTimestampDisplay->setText(t.toString("hh:mm:ss - dd.MM.yy"));
}

void ChatMessage::setStatus(int status)
{
    if (status == megachat::MegaChatMessage::STATUS_UNKNOWN)
        ui->mStatusDisplay->setText("Invalid");
    else
    {
        ui->mStatusDisplay->setText(messageStatus[status]);
    }
}

void ChatMessage::setAuthor(const char *author)
{
    if (author)
    {
        ui->mAuthorDisplay->setText(tr(author));
        return;
    }

    MegaChatHandle uh = mMessage->getUserHandle();
    if (isMine())
    {
        ui->mAuthorDisplay->setText(tr("me"));
    }
    else
    {
        mega::unique_ptr<megachat::MegaChatRoom> chatRoom(megaChatApi->getChatRoom(mChatId));
        mega::unique_ptr<const char[]> msgAuthor(megaChatApi->getUserFirstnameFromCache(mMessage->getUserHandle()));
        mega::unique_ptr<const char[]> autorizationToken(chatRoom->getAuthorizationToken());

        if (msgAuthor && msgAuthor[0] != '\0')
        {
            ui->mAuthorDisplay->setText(msgAuthor.get());
        }
        else
        {
            msgAuthor.reset(mChatWindow->mMainWin->mApp->getFirstname(uh, autorizationToken.get()));
            if (msgAuthor)
            {
                ui->mAuthorDisplay->setText(msgAuthor.get());
            }
            else
            {
                ui->mAuthorDisplay->setText(tr("Loading firstname..."));
            }
        }
    }
}

bool ChatMessage::isMine() const
{
    return (mMessage->getUserHandle() == megaChatApi->getMyUserHandle());
}

void ChatMessage::markAsEdited()
{
    setStatus(mMessage->getStatus());
    ui->mStatusDisplay->setText(ui->mStatusDisplay->text()+" (Edited)");
}

void ChatMessage::onMessageCtxMenu(const QPoint& point)
{
    QMenu *menu = ui->mMsgDisplay->createStandardContextMenu(point);

    auto actReactCount = menu->addAction(tr("Get reactions count"));
    connect(actReactCount, &QAction::triggered, this, [=](){onReactCount();});

    QMenu *addReactMenu = menu->addMenu("React to this message");
    for (int i = 0; i < utf8reactionsList.size(); i++)
    {
        std::string react = utf8reactionsList.at(i).toStdString();
        auto actReact = addReactMenu->addAction(react.c_str());
        connect(actReact, &QAction::triggered, this, [=](){onManageReaction(false, react.c_str());});
    }
    auto actReact = addReactMenu->addAction(tr("Add reaction (CUSTOM)"));
    connect(actReact, &QAction::triggered, this, [=](){onManageReaction(false);});

    QMenu *delReactMenu = menu->addMenu("Del reaction");
    for (int i = 0; i < utf8reactionsList.size(); i++)
    {
        std::string react = utf8reactionsList.at(i).toStdString();
        auto delReact = delReactMenu->addAction(react.c_str());
        connect(delReact, &QAction::triggered, this, [=](){onManageReaction(true, react.c_str());});
    }
    auto delReact = delReactMenu->addAction(tr("Del reaction (CUSTOM)"));
    connect(delReact, &QAction::triggered, this, [=](){onManageReaction(true);});

    if (isMine() && !mMessage->isManagementMessage())
    {
        if (mMessage->isEditable())
        {
            auto action = menu->addAction(tr("&Edit message"));
            action->setData(QVariant::fromValue(this));
            connect(action, SIGNAL(triggered()), this, SLOT(onMessageEditAction()));
        }

        if (mMessage->isDeletable())
        {
            auto delAction = menu->addAction(tr("Delete message"));
            delAction->setData(QVariant::fromValue(this));
            connect(delAction, SIGNAL(triggered()), this, SLOT(onMessageDelAction()));
        }

        if (mMessage->getType() == MegaChatMessage::TYPE_CONTAINS_META
                && mMessage->getContainsMeta()
                && mMessage->getContainsMeta()->getType() == MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW)
        {
            auto richAction = menu->addAction(tr("Remove rich link"));
            richAction->setData(QVariant::fromValue(this));
            connect(richAction, SIGNAL(triggered()), this, SLOT(onMessageRemoveLinkAction()));
        }
    }

    auto actCopy = menu->addAction(tr("Copy to clipboard message id"));
    connect(actCopy, SIGNAL(triggered()), this, SLOT(onCopyHandle()));

    menu->popup(this->mapToGlobal(point));
}

void ChatMessage::onMessageDelAction()
{
    mChatWindow->deleteChatMessage(this->mMessage);
}

void ChatMessage::onMessageEditAction()
{
    startEditingMsgWidget();
}

void ChatMessage::onReactCount()
{
    mega::unique_ptr<::mega::MegaStringList> reactions(mChatWindow->mMegaChatApi->getMessageReactions(mChatId, mMessage->getMsgId()));
    QMessageBox msg;
    msg.setIcon(QMessageBox::Information);
    msg.setText(std::to_string(reactions->size()).c_str());
    msg.exec();
}

void ChatMessage::onManageReaction(bool del, const char *reactionStr)
{
    QString reaction = reactionStr
            ? reactionStr
            : mChatWindow->mMainWin->mApp->getText(del ? "Del reaction" : "Add reaction", false).c_str();

    if (reaction.isEmpty())
    {
        return;
    }

    std::string utfstring = reaction.toUtf8().toStdString();
    del ? mChatWindow->mMegaChatApi->delReaction(mChatId, mMessage->getMsgId(), utfstring.c_str())
        : mChatWindow->mMegaChatApi->addReaction(mChatId, mMessage->getMsgId(), utfstring.c_str());
}

void ChatMessage::onMessageRemoveLinkAction()
{
    megaChatApi->removeRichLink(mChatId, mMessage->getMsgId());
}

void ChatMessage::cancelMsgEdit(bool /*clicked*/)
{
    clearEdit();
    mChatWindow->ui->mMessageEdit->setText(QString());
}

void ChatMessage::saveMsgEdit(bool /*clicked*/)
{
    std::string editedMsg = mChatWindow->ui->mMessageEdit->toPlainText().toStdString();
    std::string previousContent = mMessage->getContent();
    if(editedMsg != previousContent)
    {
        megachat::MegaChatHandle messageId;
        if (mMessage->getStatus() == megachat::MegaChatMessage::STATUS_SENDING)
        {
            messageId = mMessage->getTempId();
        }
        else
        {
            messageId = mMessage->getMsgId();
        }

        megachat::MegaChatMessage *message = megaChatApi->editMessage(mChatId, messageId, editedMsg.c_str());
        if (message)
        {
            setMessage(message);
            setMessageContent(message->getContent());
        }
    }

    clearEdit();
}

void ChatMessage::startEditingMsgWidget()
{
    mChatWindow->ui->mMsgSendBtn->setEnabled(false);
    mChatWindow->ui->mMessageEdit->blockSignals(true);
    ui->mEditDisplay->hide();
    ui->mStatusDisplay->hide();

    QPushButton *cancelBtn = new QPushButton(this);
    connect(cancelBtn, SIGNAL(clicked(bool)), this, SLOT(cancelMsgEdit(bool)));
    cancelBtn->setText("Cancel edit");
    auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
    layout->insertWidget(2, cancelBtn);

    QPushButton *saveBtn = new QPushButton(this);
    connect(saveBtn, SIGNAL(clicked(bool)), this, SLOT(saveMsgEdit(bool)));
    saveBtn->setText("Save");
    layout->insertWidget(3, saveBtn);

    setLayout(layout);

    std::string content = mMessage->getContent();

    mChatWindow->ui->mMessageEdit->setText(content.c_str());
    mChatWindow->ui->mMessageEdit->moveCursor(QTextCursor::End);
}

void ChatMessage::clearEdit()
{
    mChatWindow->ui->mMessageEdit->setText("");
    mChatWindow->ui->mMessageEdit->moveCursor(QTextCursor::Start);
    auto header = ui->mHeader->layout();
    auto cancelBtn = header->itemAt(2)->widget();
    auto saveBtn = header->itemAt(3)->widget();
    header->removeWidget(cancelBtn);
    header->removeWidget(saveBtn);
    ui->mEditDisplay->show();
    ui->mStatusDisplay->show();
    delete cancelBtn;
    delete saveBtn;
    mChatWindow->ui->mMsgSendBtn->setEnabled(true);
    mChatWindow->ui->mMessageEdit->blockSignals(false);
}

void ChatMessage::setManualMode(bool manualMode)
{
    if(manualMode)
    {
        ui->mEditDisplay->hide();
        ui->mStatusDisplay->hide();
        QPushButton *manualSendBtn = new QPushButton(this);
        connect(manualSendBtn, SIGNAL(clicked(bool)), this, SLOT(onManualSending()));
        manualSendBtn->setText("Send (Manual mode)");
        auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
        layout->insertWidget(2, manualSendBtn);

        QPushButton *discardBtn = new QPushButton(this);
        connect(discardBtn, SIGNAL(clicked(bool)), this, SLOT(onDiscardManualSending()));
        discardBtn->setText("Discard");
        layout->insertWidget(3, discardBtn);
        setLayout(layout);
    }
    else
    {
        ui->mEditDisplay->show();
        ui->mStatusDisplay->show();
        auto header = ui->mHeader->layout();
        auto manualSending = header->itemAt(2)->widget();
        auto discardManualSending = header->itemAt(3)->widget();
        delete manualSending;
        delete discardManualSending;
    }
}

void ChatMessage::onManualSending()
{
   if(mChatWindow->mChatRoom->getOwnPrivilege() == megachat::MegaChatPeerList::PRIV_RO)
   {
       QMessageBox::critical(nullptr, tr("Manual sending"), tr("You don't have permissions to send this message"));
   }
   else
   {
       megaChatApi->removeUnsentMessage(mChatWindow->mChatRoom->getChatId(), mMessage->getRowId());
       megachat::MegaChatMessage *tempMessage = megaChatApi->sendMessage(mChatWindow->mChatRoom->getChatId(), mMessage->getContent());
       setManualMode(false);
       mChatWindow->eraseChatMessage(mMessage, true);
       mChatWindow->moveManualSendingToSending(tempMessage);
   }
}

void ChatMessage::onDiscardManualSending()
{
   megaChatApi->removeUnsentMessage(mChatWindow->mChatRoom->getChatId(), mMessage->getRowId());
   mChatWindow->eraseChatMessage(mMessage, true);
}

void ChatMessage::on_bSettings_clicked()
{
    if (mMessage->getType() != megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT
       && mMessage->getType() != megachat::MegaChatMessage::TYPE_VOICE_CLIP)
    {
        return;
    }

    QMenu menu(this);
    menu.setAttribute(Qt::WA_DeleteOnClose);
    switch (mMessage->getType())
    {
        case megachat::MegaChatMessage::TYPE_NODE_ATTACHMENT:
        {
            ::mega::MegaNodeList *nodeList = mMessage->getMegaNodeList();
            for (int i = 0; i < nodeList->size(); i++)
            {
                ::mega::MegaNode *node = nodeList->get(i);
                QString text("Download \"");
                text.append(node->getName()).append("\"");
                auto actDownload = menu.addAction(tr(text.toStdString().c_str()));
                connect(actDownload,  &QAction::triggered, this, [this, nodeList, i]{onNodeDownloadOrImport(nodeList->get(i), false);});

                text.clear();
                text.append("Import \"");
                text.append(nodeList->get(i)->getName()).append("\" to cloud drive");
                auto actImport = menu.addAction(tr(text.toStdString().c_str()));
                connect(actImport,  &QAction::triggered, this, [this, nodeList, i]{onNodeDownloadOrImport(nodeList->get(i), true);});

                text.clear();
                text = "Stream \"";
                text.append(node->getName()).append("\"");
                auto actStream = menu.addAction(tr(text.toStdString().c_str()));
                connect(actStream,  &QAction::triggered, this, [this, node]{ onNodePlay(node); });
            }
            break;
        }

        case megachat::MegaChatMessage::TYPE_VOICE_CLIP:
        {
            ::mega::MegaNodeList *nodeList = mMessage->getMegaNodeList();
            for (int i = 0; i < nodeList->size(); i++)
            {
                ::mega::MegaNode *node = nodeList->get(i);
                QString text("Play \"");
                text.append(node->getName()).append("\"");
                auto actStream = menu.addAction(tr(text.toStdString().c_str()));
                connect(actStream,  &QAction::triggered, this, [this, node]{ onNodePlay(node); });
            }
            break;
        }

        default:
            break;
    }
    QPoint pos = ui->bSettings->pos();
    pos.setX(pos.x() + ui->bSettings->width());
    pos.setY(pos.y() + ui->bSettings->height());
    menu.exec(mapToGlobal(pos));
}

void ChatMessage::onCopyHandle()
{
    const char *messageid_64 = mChatWindow->mMegaApi->userHandleToBase64(mMessage->getMsgId());
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(messageid_64);
    delete []messageid_64;
}

void ChatMessage::onNodeDownloadOrImport(mega::MegaNode *node, bool import)
{
    mega::MegaNode *resultNode = node;

    //Autorize node with PH if we are in preview mode
    if (mChatWindow->mChatRoom->isPreview())
    {
        const char *cauth = mChatWindow->mChatRoom->getAuthorizationToken();
        resultNode = mChatWindow->mMegaApi->authorizeChatNode(node, cauth);
        delete [] cauth;
    }

    QMessageBox msgBoxAns;
    if(import)
    {
        mega::MegaNode *parent= mChatWindow->mMegaApi->getRootNode();
        if (parent)
        {
            std::string message("Node will be imported to the cloud drive");
            msgBoxAns.setText(message.c_str());
            msgBoxAns.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            if (msgBoxAns.exec() == QMessageBox::Ok)
            {
                mChatWindow->mMegaApi->copyNode(resultNode, parent);
            }
            delete parent;
        }
        else
        {
            std::string message("Node cannot be imported in anonymous mode");
            msgBoxAns.setText(message.c_str());
            msgBoxAns.setStandardButtons(QMessageBox::Ok);
            msgBoxAns.exec();
        }
    }
    else
    {
        std::string target(mChatWindow->mMegaChatApi->getAppDir());
        target.append("/").append(node->getName());
        std::string message("Node will be saved in "+target+".\nDo you want to continue?");
        msgBoxAns.setText(message.c_str());
        msgBoxAns.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        if (msgBoxAns.exec() == QMessageBox::Ok)
        {
            mChatWindow->mMegaApi->startDownload(resultNode, target.c_str(), nullptr, nullptr, false, nullptr,
                                                 mega::MegaTransfer::COLLISION_CHECK_FINGERPRINT,
                                                 mega::MegaTransfer::COLLISION_RESOLUTION_OVERWRITE);
        }
    }
}

void ChatMessage::onNodePlay(mega::MegaNode *node)
{
    mega::MegaNode *resultNode = node;

    //Start an HTTP proxy server
    if (mChatWindow->mMegaApi->httpServerIsRunning() == 0)
    {
        mChatWindow->mMegaApi->httpServerStart();
    }

    //Autorize node with PH if we are in preview mode
    if (mChatWindow->mChatRoom->isPreview())
    {
        const char *cauth = mChatWindow->mChatRoom->getAuthorizationToken();
        resultNode = mChatWindow->mMegaApi->authorizeChatNode(node, cauth);
        delete [] cauth;

        char *aux = mChatWindow->mMegaApi->httpServerGetLocalLink(resultNode);
        if (aux)
        {
             QMessageBox::information(nullptr, tr("Node Url"), tr(aux));
             delete [] aux;
        }
        else
        {
            QMessageBox::information(nullptr, tr("ERROR"), tr("ERROR"));
        }
    }
}
