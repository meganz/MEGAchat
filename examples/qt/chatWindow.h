#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QDialog>
#include <chatd.h>
#include <ui_chat.h>
#include <ui_chatMessage.h>
#include <QDateTime>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMenu>
#include <QMoveEvent>
#include <QScrollBar>
#include <QTimer>
#include <QProgressBar>
#include <QMimeData>
#include <QMediaPlayer>
#include <QBuffer>
#include <QToolTip>
#include <chatdDb.h>
#include <chatClient.h>
#include "callGui.h"
#include <mega/base64.h> //for jid base32 conversion
namespace Ui
{
class ChatWindow;
}

enum {kHistBatchSize = 16};
extern QString gOnlineIndColors[karere::Presence::kLast+1];

class ChatWindow;
class MessageWidget: public QWidget
{
    Q_OBJECT
protected:
    Ui::ChatMessage ui;
    ChatWindow& mChatWindow;
    const chatd::Message* mMessage; //we only need this for the popup menu - Qt doesn't give the index of the clicked item, only a pointer to it
    const chatd::Message& mOriginal;
    short mEditCount = 0; //counts how many times we have edited the message, so that we can keep the edited display even after a canceled edit, in case there was a previous edit
    bool mIsMine;
    chatd::Idx mIndex;
    Q_PROPERTY(QColor msgColor READ msgColor WRITE setMsgColor)
    QColor msgColor() { return palette().color(QPalette::Base); }
    void setMsgColor(const QColor& color)
    {
        QPalette p(ui.mMsgDisplay->palette());
        p.setColor(QPalette::Base, color);
        ui.mMsgDisplay->setPalette(p);
    }
    friend class ChatWindow;
public:
    MessageWidget(ChatWindow& parent, const chatd::Message& msg,
                  chatd::Message::Status status, chatd::Idx idx);
    MessageWidget& setAuthor(const chatd::Id& userid)
    {
        if (mIsMine)
        {
            ui.mAuthorDisplay->setText(tr("me"));
            return *this;
        }
        auto email = karere::gClient->contactList->getUserEmail(userid);
        if (email)
            ui.mAuthorDisplay->setText(QString::fromStdString(*email));
        else
            ui.mAuthorDisplay->setText(tr("error"));

        karere::gClient->userAttrCache.getAttr(userid, mega::MegaApi::USER_ATTR_LASTNAME, this,
        [](Buffer* data, void* userp)
        {
            //buffer contains an unsigned char prefix that is the strlen() of the first name
            if (!data)
                return;
            auto self = static_cast<MessageWidget*>(userp);
            self->ui.mAuthorDisplay->setText(QString::fromUtf8(data->buf()+1));
        });
        return *this;
    }

    MessageWidget& setTimestamp(uint32_t ts)
    {
        QDateTime t;
        t.setTime_t(ts);
        ui.mTimestampDisplay->setText(t.toString("hh:mm:ss"));
        return *this;
    }
    MessageWidget& setStatus(chatd::Message::Status status)
    {
        ui.mStatusDisplay->setText(chatd::Message::statusToStr(status));
        return *this;
    }
    MessageWidget& setText(const chatd::Message& msg)
    {
        auto& txt = *ui.mMsgDisplay;
        if (msg.isEncrypted == chatd::Message::kEncrypted)
        {
            txt.setFontItalic(true);
            txt.setText(tr("Decrypting..."));
        }
        else if (msg.isEncrypted == chatd::Message::kDecryptError)
        {
            txt.setFontItalic(true);
            ui.mMsgDisplay->setText(tr("Decrypt error:\n")+QString::fromLatin1(msg.buf(), msg.dataSize()));
        }
        else
        {
            assert(!msg.isEncrypted);
            txt.setFontItalic(false);
            txt.setText(QString::fromUtf8(msg.buf(), msg.dataSize()));
        }
        return *this;
    }
    MessageWidget& updateStatus(chatd::Message::Status newStatus)
    {
        ui.mStatusDisplay->setText(chatd::Message::statusToStr(newStatus));
        return *this;
    }
    QPushButton* startEditing()
    {
        setBgColor(Qt::yellow);
        ui.mEditDisplay->hide();
        ui.mStatusDisplay->hide();

        auto btn = new QPushButton(this);
        btn->setText("Cancel edit");
        auto layout = static_cast<QBoxLayout*>(ui.mHeader->layout());
        layout->insertWidget(2, btn);
        this->layout();
        return btn;
    }
    MessageWidget& cancelEdit()
    {
        disableEditGui();
        ui.mEditDisplay->setText(mEditCount?tr("(edited)"): QString());
        return *this;
    }
    MessageWidget& disableEditGui()
    {
        fadeIn(QColor(Qt::yellow));
        auto header = ui.mHeader->layout();
        auto btn = header->itemAt(2)->widget();
        header->removeWidget(btn);
        ui.mEditDisplay->show();
        ui.mStatusDisplay->show();
        delete btn;
        return *this;
    }
    MessageWidget& setBgColor(const QColor& color)
    {
        QPalette p = ui.mMsgDisplay->palette();
        p.setColor(QPalette::Base, color);
        ui.mMsgDisplay->setPalette(p);
        return *this;
    }
    MessageWidget& fadeIn(const QColor& color, int dur=300, const QEasingCurve& curve=QEasingCurve::Linear)
    {
        auto a = new QPropertyAnimation(this, "msgColor");
        a->setStartValue(QColor(color));
        a->setEndValue(QColor(Qt::white));
        a->setDuration(dur);
        a->setEasingCurve(curve);
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return *this;
    }
    MessageWidget& setEdited()
    {
        ui.mEditDisplay->setText(tr("(Edited)"));
        return *this;
    }
};
struct HistFetchUi: public QProgressBar
{
    HistFetchUi(QWidget* parent): QProgressBar(parent) { setRange(0, kHistBatchSize); }
    QProgressBar* progressBar() { return this; }
};
class WaitMsgWidget: public QLabel
{
    std::set<QString> mMsgs;
public:
    WaitMsgWidget(QWidget* parent, const QString& msg);
    void addMsg(const QString& msg);
    void updateGui();
    void show();
};

class WaitMessage: protected std::shared_ptr<WaitMsgWidget>
{
    ChatWindow& mChatWindow;
public:
    WaitMessage(ChatWindow& chatWindow);
    ~WaitMessage();
    void addMsg(const QString& msg);
};

class ChatWindow: public QDialog, public karere::IGui::IChatWindow
{
    Q_OBJECT
public:
    MainWindow& mainWindow;
protected:
    karere::ChatRoom& mRoom;
    Ui::ChatWindow ui;
    chatd::Messages* mMessages = nullptr;
    MessageWidget* mEditedWidget = nullptr; ///pointer to the widget being edited. Also signals whether we are editing or writing a new message (nullptr - new msg, editing otherwise)
    std::map<chatd::Id, const chatd::Message*> mNotLinkedEdits;
    bool mUnsentChecked = false;
    std::unique_ptr<HistFetchUi> mHistFetchUi;
    CallGui* mCallGui = nullptr;
    bool mLastHistReqByScroll = false;
    WaitMessage mWaitMsg;
    friend class CallGui;
    friend class CallAnswerGui;
    friend class WaitMessage;
public slots:
    void onMsgSendBtn()
    {
        auto text = ui.mMessageEdit->toPlainText().toUtf8();
        if (mEditedWidget)
        {
            auto widget = mEditedWidget;
            auto& msg = *mEditedWidget->mMessage;
            mEditedWidget = nullptr;
            if ((text.size() == msg.dataSize())
              && (memcmp(text.data(), msg.buf(), text.size()) == 0))
            { //no change
                widget->disableEditGui();
                ui.mMessageEdit->setText(QString());
                return;
            }

            auto& original = widget->mOriginal;
            //TODO: see how to associate the edited message widget with the userp
            auto edited = mMessages->msgModify(original.id(), original.isSending(), text.data(), text.size(), original.userp);
            addMsgEdit(*edited, original);
            widget->disableEditGui().updateStatus(chatd::Message::kSending);
        }
        else
        {
            if (text.isEmpty())
                return;
            auto msg = mMessages->msgSubmit(text.data(), text.size(), chatd::Message::kTypeRegularMessage, nullptr);
            msg->userp = addMsgWidget(*msg, CHATD_IDX_INVALID, chatd::Message::kSending, false);
            ui.mMessageList->scrollToBottom();
        }
        ui.mMessageEdit->setText(QString());
    }
    void onMessageCtxMenu(const QPoint& point)
    {
        auto msgWidget = qobject_cast<MessageWidget*>(QObject::sender()->parent());
        //enable edit action only if the message is ours
        auto menu = msgWidget->ui.mMsgDisplay->createStandardContextMenu(point);

        if (msgWidget->mIsMine)
        {
            auto action = menu->addAction(tr("&Edit message"));
            action->setData(QVariant::fromValue(msgWidget));
            connect(action, SIGNAL(triggered()), this, SLOT(onMessageEditAction()));
        }
        menu->popup(msgWidget->mapToGlobal(point));
    }
    void onMessageCtxMenuHide()
    {
    }
    void onMessageEditAction()
    {
        auto action = qobject_cast<QAction*>(QObject::sender());
        startEditingMsgWidget(*action->data().value<MessageWidget*>());
    }
    void editLastMsg()
    {
        auto msglist = ui.mMessageList;
        int count = msglist->count();
        for(int i=count-1; i>=0; i--)
        {
            auto item = msglist->item(i);
            auto widget = qobject_cast<MessageWidget*>(msglist->itemWidget(item));
            if (widget->mIsMine)
            {
                msglist->scrollToItem(item);
                startEditingMsgWidget(*widget);
                return;
            }
        }
    }
    void cancelMsgEdit()
    {
        assert(mEditedWidget);
        mEditedWidget->cancelEdit();
        mEditedWidget = nullptr;
        ui.mMessageEdit->setText(QString());
    }
    void onMsgListRequestHistory()
    {
        fetchMoreHistory(true);
    }
    void fetchMoreHistory(bool byScroll)
    {
        mLastHistReqByScroll = byScroll;
        auto state = mMessages->histFetchState();
        if (state & chatd::kHistFetchingFlag)
            return;
        if (state == chatd::kHistNoMore)
        {
            //TODO: Show in some way in the GUI that we have reached the start of history
            return;
        }
        bool isRemote = mMessages->getHistory(kHistBatchSize);
        if (isRemote)
        {
            mHistFetchUi.reset(new HistFetchUi(this));
            auto layout = qobject_cast<QBoxLayout*>(ui.mTitlebar->layout());
            layout->insertWidget(2, mHistFetchUi->progressBar());
        }
    }
    void onVideoCallBtn(bool) { onCallBtn(true); }
    void onAudioCallBtn(bool) { onCallBtn(false); }
    void onMembersBtn(bool);
    void onMemberRemove();
    void onMemberSetPriv();
    void onMemberPrivateChat();
    void onScroll(int value);
    void onSoundPlayStateChanged(QMediaPlayer::State state)
    {
        printf("sound state change: %d\n", state);
        if (state == QMediaPlayer::StoppedState)
            qobject_cast<QMediaPlayer*>(QObject::sender())->deleteLater();
    }
public:
    ChatWindow(karere::ChatRoom& room, MainWindow& parent);
    virtual ~ChatWindow();
    chatd::Messages& messages() const { return *mMessages; }
protected:
    void createCallGui(const std::shared_ptr<rtcModule::ICall>& call=nullptr)
    {
        assert(!mCallGui);
        auto layout = qobject_cast<QBoxLayout*>(ui.mCentralWidget->layout());
        mCallGui = new CallGui(*this, call);
        layout->insertWidget(1, mCallGui, 1);
        ui.mTitlebar->hide();
        ui.mTextChatWidget->hide();
    }
    void deleteCallGui()
    {
        assert(mCallGui);
        delete mCallGui;
        mCallGui = nullptr;
        ui.mTitlebar->show();
        ui.mTextChatWidget->show();
    }
    void updateSeen();
    virtual void showEvent(QShowEvent* event)
    {
        mega::setTimeout([this]() { updateSeen(); }, 2000);
    }
    void closeEvent(QCloseEvent* event)
    {
        if (mCallGui)
            mCallGui->hangup();
        event->accept();
    }
    virtual void dragEnterEvent(QDragEnterEvent* event)
    {
        if (event->mimeData()->hasFormat("application/mega-user-handle"))
            event->acceptProposedAction();
    }
    virtual void dropEvent(QDropEvent* event);
    void createMembersMenu(QMenu& menu);
    void onCallBtn(bool video)
    {
        if (mCallGui)
            return;
        if (mRoom.isGroup())
        {
            QMessageBox::critical(this, "Call", "Nice try, but group audio and video calls are not implemented yet");
            return;
        }
        auto uh = static_cast<karere::PeerChatRoom&>(mRoom).peer();
        char buf[32];
        auto len = mega::Base32::btoa((byte*)&uh, sizeof(uh), buf);
        buf[len] = 0;
        std::string jid(buf);
        jid+='@';
        jid.append(KARERE_XMPP_DOMAIN);
        createCallGui();
        mRoom.parent.client.rtc->startMediaCall(mCallGui, jid, karere::AvFlags(true, video));
    }

    static MessageWidget* widgetFromMessage(const chatd::Message& msg)
    {
        if (!msg.userp)
            return nullptr;
        auto item = static_cast<QListWidgetItem*>(msg.userp);
        return qobject_cast<MessageWidget*>(item->listWidget()->itemWidget(item));
    }
    QListWidgetItem* addMsgWidget(const chatd::Message& msg, chatd::Idx idx, chatd::Message::Status status,
                      bool first, QColor* color=nullptr)
    {
        auto widget = new MessageWidget(*this, msg, status, idx);
        connect(widget->ui.mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
//      connect(widget->ui.mAuthorDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMsgAuthorCtxMenu(const QPoint&)));

        auto* item = new QListWidgetItem;
        msg.userp = item;
        item->setSizeHint(widget->size());
        if (first)
        {
            ui.mMessageList->insertItem(0, item);
        }
        else
        {
            ui.mMessageList->addItem(item);
        }
        ui.mMessageList->setItemWidget(item, widget);
        if(!checkHandleInboundEdits(msg))
        {
            if (color)
                widget->fadeIn(*color);
        }
//        QApplication::processEvents(); //TODO: This call makes histry fetching smooth, but causes reentrancy and random crashes, find a way to make history smooth in another way
        return item;
    }
    void addMsgEdit(const chatd::Message& msg, bool first, QColor* color=nullptr)
    {
        assert(msg.edits() && (msg.type == chatd::Message::kTypeEdit));
        auto idx = mMessages->msgIndexFromId(msg.edits());
        if (idx == CHATD_IDX_INVALID) //maybe message is not loaded in buffer? Ignore it then
        {
            auto it = mNotLinkedEdits.find(msg.edits());
            if ((it == mNotLinkedEdits.end()) || !first) //we are adding an oldest message, can't be a more recent edit, ignore
                mNotLinkedEdits[msg.edits()] = &msg;
            msg.userp = nullptr;
            //GUI_LOG_DEBUG("Can't find original message of edit, adding to map", msg.dataSize(), msg.buf());
            return;
        }
        auto& targetMsg = mMessages->at(idx);
        assert(targetMsg.id() == msg.edits());
        addMsgEdit(msg, targetMsg, color);
    }
    void addMsgEdit(const chatd::Message& edited, const chatd::Message& original, QColor* color=nullptr)
    {
        auto widget = widgetFromMessage(original);
//User pointer of referenced message by an edit can be null only if it's an edit itself. But edits
//can't point to other edits, they must reference only the original message and can't be chained
        assert(widget); //Edit message points to another edit message
        edited.userp = original.userp; //this is already done by msgModify that created the edited message
        widget->mMessage = &edited;
        widget->setText(edited).setEdited();
        widget->mEditCount++;
        if (color)
            widget->fadeIn(*color);
    }
    void startEditingMsgWidget(MessageWidget& widget)
    {
        mEditedWidget = &widget;
        auto cancelBtn = widget.startEditing();
        connect(cancelBtn, SIGNAL(clicked()), this, SLOT(cancelMsgEdit()));
        ui.mMessageEdit->setText(QString().fromUtf8(widget.mMessage->buf(), widget.mMessage->dataSize()));
        ui.mMessageEdit->moveCursor(QTextCursor::End);
    }
    bool checkHandleInboundEdits(const chatd::Message& msg)
    {
        assert(msg.userp); //message must be in the GUI already
        auto it = mNotLinkedEdits.find(msg.id());
        if (it == mNotLinkedEdits.end())
            return false;
        //GUI_LOG_DEBUG("Loaded message that had edits pending", msg.dataSize(), msg.buf());
        auto widget = widgetFromMessage(msg);
        auto updated = widget->mMessage = it->second;
        widget->setText(*updated).setEdited();
        mNotLinkedEdits.erase(it);
        return true;
    }
    void updateOnlineIndication(karere::Presence pres)
    {
        if (pres == karere::Presence::kOffline)
        {
            ui.mAudioCallBtn->hide();
            ui.mVideoCallBtn->hide();
        }
        else
        {
            ui.mAudioCallBtn->show();
            ui.mVideoCallBtn->show();
        }
        ui.mOnlineIndicator->setStyleSheet(
            QString("border-radius: 4px; background-color: ")+
            gOnlineIndColors[pres.val()]);
    }
    //we are online - we need to have fetched all new messages to be able to send unsent ones,
    //because the crypto layer needs to have received the most recent keys
    chatd::Listener* listenerInterface() { return static_cast<chatd::Listener*>(this); }
public:
    //chatd::Listener interface
    virtual void init(chatd::Messages& messages, chatd::DbInterface*& dbIntf)
    {
        mMessages = &messages;
        updateOnlineIndication(mRoom.presence());
        updateChatdStatusDisplay(mMessages->onlineState());
        if (mMessages->empty())
            return;
        auto last = mMessages->highnum();
        for (chatd::Idx idx = mMessages->lownum(); idx<=last; idx++)
        {
            auto& msg = mMessages->at(idx);
            if (msg.type == chatd::Message::kTypeEdit)
                addMsgEdit(msg, false);
            else
                addMsgWidget(msg, idx, mMessages->getMsgStatus(idx, msg.userid), false);
        }
        if (mMessages->onlineState() == chatd::kChatStateOnline)
            QMetaObject::invokeMethod(this, "fetchMoreHistory", Qt::QueuedConnection, Q_ARG(bool, false));
    }
    virtual void onDestroy(){ close(); }
    virtual void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status)
    {
        mRoom.onRecvNewMessage(idx, msg, status);
        auto sbar = ui.mMessageList->verticalScrollBar();
        bool wasAtBottom = sbar->value() == sbar->maximum();
        if (msg.edits())
            addMsgEdit(msg, false);
        else
            addMsgWidget(msg, idx, status, false);
        if (wasAtBottom)
            ui.mMessageList->scrollToBottom();
        if (!isVisible() || !wasAtBottom)
        {
            printf("Playing sound\n");
            auto file = new QFile(":/icq-incoming-msg.mp3");
            file->open(QIODevice::ReadOnly);
/*            QByteArray resdata = file.readAll();
            if (resdata.isEmpty())
            {
                printf("res file is empty\n");
                return;
            }
            auto media = new QBuffer(&resdata);
*/
            auto sound = new QMediaPlayer();
            connect(sound, SIGNAL(stateChanged(QMediaPlayer::State)), this, SLOT(onSoundPlayStateChanged(QMediaPlayer::State)));
            sound->setMedia(QMediaContent(), file); //qrc://icq-incoming-msg.mp3"));
            sound->play();
        }
    }
    virtual void onRecvHistoryMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status, bool isFromDb)
    {
        assert(idx != CHATD_IDX_INVALID); assert(msg.id());
        if (mHistFetchUi)
        {
            mHistFetchUi->progressBar()->setValue(mMessages->lastHistFetchCount());
            mHistFetchUi->progressBar()->repaint();
        }
        if (msg.edits())
        {
            addMsgEdit(msg, true);
        }
        else
        {
            addMsgWidget(msg, idx, status, true);
        }
    }
    virtual void onMsgDecrypted(chatd::Message& msg)
    {
        //if original was not (yet) loaded when this message was received, it's not linked
        //to any widget, so we don't need to update anything in the GUI
        if (msg.userp)
            widgetFromMessage(msg)->setText(msg);
        else
        {
            assert(msg.edits());
            GUI_LOG_DEBUG("Received an edit whose original is yet not loaded");
        }
    }
    virtual void onMsgDecryptError(chatd::Message &msg, const std::string &err)
    {
        assert(msg.isEncrypted == chatd::Message::kDecryptError);
        msg.clear();
        msg.append(err.c_str(), err.size());
        auto widget = widgetFromMessage(msg);
        if (widget)
            widget->setText(msg);
    }
    virtual void onHistoryDone(bool isFromDb)
    {
        mHistFetchUi.reset();
        if (!mMessages->lastHistFetchCount())
            return;

        auto& list = *ui.mMessageList;
        //chech if we have filled the window height with history, if not, fetch more
        auto idx = list.indexAt(QPoint(list.rect().left()+10, list.rect().bottom()-2));
        if (!idx.isValid() && mMessages->histFetchState() != chatd::kHistNoMore)
        {
            fetchMoreHistory(false);
            return;
        }
        if (!mLastHistReqByScroll)
        {
            ui.mMessageList->scrollToBottom();
        }
        else
        {
            int last = (idx.isValid())?std::min((unsigned)idx.row(), mMessages->lastHistFetchCount()):list.count()-1;
            for (int i=0; i<=last; i++)
                qobject_cast<MessageWidget*>(list.itemWidget(list.item(i)))->fadeIn(QColor(250,250,250));
        }
    }
    virtual void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message& msg)
    {
        mRoom.onMessageStatusChange(idx, newStatus, msg);
        auto widget = widgetFromMessage(msg);
        if (widget)
            widget->updateStatus(newStatus);
    }
    virtual void onMessageConfirmed(const chatd::Id& msgxid, const chatd::Id& msgid, chatd::Idx idx)
    {
        // add to history, message was just created at the server
        assert(msgxid); assert(msgid); assert(idx != CHATD_IDX_INVALID);
        auto& msg = mMessages->at(idx);
        auto widget = widgetFromMessage(msg);
        if (widget)
            widget->updateStatus(chatd::Message::kServerReceived);
    }
    virtual void onOnlineStateChange(chatd::ChatState state)
    {
        mRoom.onOnlineStateChange(state);
        updateChatdStatusDisplay(state);

        if ((state == chatd::kChatStateOnline) && (mMessages->size() < 2)
        && ((mMessages->histFetchState() & chatd::kHistFetchingFlag) == 0))
        {
            // avoid re-entrancy - we are in a chatd callback. We could use mega::marshallCall instead,
            // but this is safer as the window may get destroyed before the message is processed
            QMetaObject::invokeMethod(this, "fetchMoreHistory", Qt::QueuedConnection, Q_ARG(bool, false));
        }
    }
    void updateChatdStatusDisplay(chatd::ChatState state)
    {
        ui.mChatdStatusDisplay->setText(chatd::chatStateToStr(mMessages->onlineState()));
        if (state != chatd::kChatStateOnline)
            ui.mChatdStatusDisplay->show();
        else
            ui.mChatdStatusDisplay->hide();
    }
    virtual void onUnsentMsgLoaded(const chatd::Message& msg)
    {
        if (msg.edits()) //the
        {
            addMsgEdit(msg, false);
        }
        else
        {
            auto item = addMsgWidget(msg, CHATD_IDX_INVALID, chatd::Message::kSending, false);
            msg.userp = item;
        }
        ui.mMessageList->scrollToBottom();
    }
    virtual void onUserJoined(const chatd::Id& userid, char priv)
    {
        mRoom.onUserJoined(userid, priv);
    }
    virtual void onUserLeft(const chatd::Id& userid)
    {
        mRoom.onUserLeft(userid);
    }
    virtual void onUnreadChanged() { mRoom.onUnreadChanged(); }
    virtual karere::IGui::ICallGui* getCallGui() { return mCallGui; }
    //IChatWindow interface
    virtual void show() { QDialog::show(); raise(); }
    virtual void hide() { QDialog::hide(); }
    virtual void updateTitle(const std::string& title)
    {
        QString text = (mRoom.isGroup()) ? tr("Group: "): "";
        text += QString::fromStdString(title);
        ui.mTitleLabel->setText(text);
    }
};

#endif // CHATWINDOW_H
