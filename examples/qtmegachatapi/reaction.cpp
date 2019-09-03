#include <QMenu>
#include <QClipboard>
#include "reaction.h"
#include "ui_reaction.h"

Reaction::Reaction(ChatMessage *parent, const char *reactionString, int count) :
    QWidget((QWidget *)parent),
    ui(new Ui::Reaction)
{        
    mChatMessage = parent;
    ui->setupUi(this);
    mCount = count;    
    mReactionString = reactionString ? reactionString :std::string();

    QString text(mReactionString.c_str());
    text.append(" ")
        .append(std::to_string(count).c_str());

    ui->mReaction->setText(text);
    setAttribute(::Qt::WA_Hover, true);
}

Reaction::~Reaction()
{
    delete ui;
}

void Reaction::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);    
    auto actAdd = menu.addAction(tr("React to this message"));
    connect(actAdd, SIGNAL(triggered()), this, SLOT(onAddReact()));
    auto actRemove = menu.addAction(tr("Del reaction"));
    connect(actRemove, SIGNAL(triggered()), this, SLOT(onRemoveReact()));
    auto actCopy = menu.addAction(tr("Copy UTF-8"));
    connect(actCopy, SIGNAL(triggered()), this, SLOT(onCopyReact()));

    QPoint pos = ui->mReaction->pos();
    pos.setX(pos.x() + ui->mReaction->width());
    pos.setY(pos.y() + ui->mReaction->height());
    menu.exec(mapToGlobal(pos));
    menu.deleteLater();
}

std::string Reaction::getReactionString() const
{
    return mReactionString;
}

void Reaction::updateReactionCount(int count)
{
    if (!count)
    {
        return;
    }

    mCount = count;
    QString text(mReactionString.c_str());
    text.append(" ")
        .append(std::to_string(count).c_str());

    ui->mReaction->setText(text.toStdString().c_str());
}

void Reaction::onCopyReact()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(mReactionString.c_str());
}

void Reaction::onRemoveReact()
{
    ChatWindow *chatwindow = mChatMessage->getChatWindow();
    if (!chatwindow)
    {
        return;
    }

    MegaChatHandle chatid = mChatMessage->getChatId();
    MegaChatHandle msgid = mChatMessage->getMessage()->getMsgId();
    const char *reaction = mReactionString.c_str();

    MegaChatError *res = chatwindow->getMegaChatApi()->delReaction(chatid, msgid, reaction);
    if (res->getErrorCode() != MegaChatError::ERROR_OK)
    {
        QMessageBox msg;
        msg.setParent(nullptr);
        msg.setIcon(QMessageBox::Information);
        msg.setText(res->toString());
        msg.exec();
    }
    delete res;
}

void Reaction::onAddReact()
{
    ChatWindow *chatwindow = mChatMessage->getChatWindow();
    if (!chatwindow)
    {
        return;
    }

    MegaChatHandle chatid = mChatMessage->getChatId();
    MegaChatHandle msgid = mChatMessage->getMessage()->getMsgId();
    const char *reaction = mReactionString.c_str();

    MegaChatError *res = chatwindow->getMegaChatApi()->addReaction(chatid, msgid, reaction);
    if (res->getErrorCode() != MegaChatError::ERROR_OK)
    {
        QMessageBox msg;
        msg.setParent(nullptr);
        msg.setIcon(QMessageBox::Information);
        msg.setText(res->toString());
        msg.exec();
    }
     delete res;
}

void Reaction::enterEvent(QEvent *event)
{
    megachat::MegaChatApi *megachatApi = mChatMessage->getMegaChatApi();
    std::unique_ptr <::mega::MegaHandleList> users (megachatApi->getReactionUsers(mChatMessage->getChatId(), mChatMessage->getMessage()->getMsgId(), mReactionString.c_str()));
    std::unique_ptr <megachat::MegaChatRoom> chatRoom(megachatApi->getChatRoom(mChatMessage->getChatId()));
    if (!megachatApi || !users || !chatRoom)
    {
        return;
    }

    QString text;
    for (unsigned int i = 0; i < users->size(); i++)
    {        
        const char *autorizationToken = chatRoom->getAuthorizationToken();
        const char *firstName = mChatMessage->getChatWindow()->getMainWin()->getApp()->getFirstname(users->get(i), autorizationToken);

        if (firstName)
        {
            text.append(firstName).append("\n");
        }

        delete [] autorizationToken;
        delete [] firstName;
    }
    ui->mReaction->setToolTip(text);
}
