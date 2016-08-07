#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QDialog>
#include <chatd.h>
#include <ui_chat.h>
#include <ui_chatmessage.h>
#include <ui_manualSendMsg.h>
#include <QDateTime>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMenu>
#include <QMoveEvent>
#include <QScrollBar>
#include <QProgressBar>
#include <QMimeData>
#include <QToolTip>
#include <chatdDb.h>
#include <chatClient.h>
#include "callGui.h"
#include <mega/base64.h> //for jid base32 conversion
namespace Ui
{
class ChatWindow;
}

enum {kHistBatchSize = 16, kMsgfDeleted = 1};
extern QString gOnlineIndColors[karere::Presence::kLast+1];

class ChatWindow;
class MessageWidget: public QWidget
{
    Q_OBJECT
protected:
    Ui::ChatMessage ui;
    ChatWindow& mChatWindow;
    chatd::Message* mMessage; ///The message whose contents is displayed
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
    MessageWidget(ChatWindow& parent, chatd::Message& msg,
                  chatd::Message::Status status, chatd::Idx idx);
    MessageWidget& setAuthor(karere::Id userid);
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
        txt.setText(QString::fromUtf8(msg.buf(), msg.dataSize()));
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
        ui.mEditDisplay->setText(mMessage->updated?tr("(edited)"): QString());
        return *this;
    }
    MessageWidget& disableEditGui(bool fadeToNormal=true)
    {
        if (fadeToNormal)
            fadeIn(Qt::yellow);
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
    MessageWidget& fadeIn(const QColor& color, int dur=500, const QEasingCurve& curve=QEasingCurve::Linear)
    {
        auto a = new QPropertyAnimation(this, "msgColor");
        a->setStartValue(QColor(color));
        a->setEndValue(QColor(Qt::white));
        a->setDuration(dur);
        a->setEasingCurve(curve);
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return *this;
    }
    MessageWidget& setEdited(const QString& txt=QObject::tr("(Edited)"))
    {
        ui.mEditDisplay->setText(txt);
        return *this;
    }
    void msgDeleted();
    void removeFromList();
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

class WaitMsg: protected std::shared_ptr<WaitMsgWidget>
{
    ChatWindow& mChatWindow;
public:
    WaitMsg(ChatWindow& chatWindow);
    ~WaitMsg();
    void addMsg(const QString& msg);
};
class ManualSendMsgWidget: public QWidget
{
    Q_OBJECT
public:
    Ui::ManualSendMsg ui;
    ChatWindow& mChatWindow;
    std::unique_ptr<chatd::Message> mMessage; ///The message whose contents is displayed
    uint64_t mId;
    uint8_t mReason;
    ManualSendMsgWidget(ChatWindow& chatWin, chatd::Message* aMsg, uint64_t id, uint8_t reason);
public slots:
    void onSendBtn();
    void onDiscardBtn();
protected:
    void removeFromListAndDelete();
};

class ChatWindow: public QDialog, public karere::IGui::IChatWindow
{
    Q_OBJECT
public:
    MainWindow& mainWindow;
    Ui::ChatWindow ui;
    QListWidget* mManualSendList = nullptr;
protected:
    karere::ChatRoom& mRoom;
    chatd::Chat* mChat = nullptr;
    MessageWidget* mEditedWidget = nullptr; ///pointer to the widget being edited. Also signals whether we are editing or writing a new message (nullptr - new msg, editing otherwise)
    std::unique_ptr<HistFetchUi> mHistFetchUi;
    CallGui* mCallGui = nullptr;
    bool mLastHistReqByScroll = false;
    WaitMsg mWaitMsg;
/** The position in the widget list before which is the last message in server history,
 *  and after which are all unsent messages, in order
 */
    int mHistAddPos = 0;
    friend class CallGui;
    friend class CallAnswerGui;
    friend class WaitMsg;
    friend class MessageWidget;
public slots:
    void onMsgSendBtn()
    {
        QString qtext = ui.mMessageEdit->toPlainText();
        if (qtext.isEmpty())
            return;
        auto text = qtext.toUtf8();
        ui.mMessageEdit->setText(QString());

        if (mEditedWidget)
        {
            submitEdit(text.data(), text.size());
            assert(!mEditedWidget);
        }
        else
        {
            postNewMessage(text.data(), text.size());
        }
    }
    void submitEdit(const char* data, size_t size)
    {
        auto widget = mEditedWidget;
        auto& msg = *mEditedWidget->mMessage;
        mEditedWidget = nullptr;
        if (!data) //delete message
        {
            if (!mChat->msgModify(msg, nullptr, 0, msg.userp))
            {
                showCantEditNotice();
                goto noedit;
            }
            widget->disableEditGui();
            widget->setEdited("deleting");
            widget->updateStatus(chatd::Message::kSending);
            return;
        }
        else //try to edit message
        {
            if (msg.dataEquals(data, size))  //no change
                goto noedit;

            chatd::Message* edited = mChat->msgModify(msg, data, size, msg.userp);
            if (!edited) //can't edit, msg too old
            {
                showCantEditNotice();
                goto noedit;
            }
            //successfully edited, don't fade back to white until edit is comfirmed by server
            widget->setText(*edited);
            widget->disableEditGui(false);
            widget->setEdited();
            widget->updateStatus(chatd::Message::kSending);
        }
        return;
noedit:
        widget->disableEditGui();
    }
    void postNewMessage(const char* data, size_t size, chatd::Message::Type type=chatd::Message::kMsgNormal)
    {
        if (!data)
            throw std::runtime_error("postNewMessage: Can't post message with NULL data");

        auto msg = mChat->msgSubmit(data, size, type, nullptr);
        msg->userp = addMsgWidget(*msg, CHATD_IDX_INVALID, chatd::Message::kSending, false);
        ui.mMessageList->scrollToBottom();
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
            auto delAction = menu->addAction(tr("Delete message"));
            delAction->setData(QVariant::fromValue(msgWidget));
            connect(delAction, SIGNAL(triggered()), this, SLOT(onMessageDelAction()));
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
    void onMessageDelAction()
    {
        auto action = qobject_cast<QAction*>(QObject::sender());
        auto widget = action->data().value<MessageWidget*>();
        assert(widget);
        auto& msg = widget->mMessage;
        if (!mChat->msgModify(*msg, nullptr, 0, msg->userp))
        {
            showCantEditNotice(tr("delete"));
            return;
        }
        widget->updateStatus(chatd::Message::kSending);
    }
    void editLastMsg()
    {
        if (mEditedWidget)
            return;
        auto msglist = ui.mMessageList;
        int count = msglist->count();
        for(int i=count-1; i>=0; i--)
        {
            auto item = msglist->item(i);
            auto widget = qobject_cast<MessageWidget*>(msglist->itemWidget(item));
            if (widget->mIsMine && !(widget->mMessage->userFlags & kMsgfDeleted))
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
//        if (mChat->isFetchingHistory() && !mChat->isFetchDecrypting())
//            return;
        if (mChat->histFetchState() == chatd::kHistNoMore)
        {
            //TODO: Show in some way in the GUI that we have reached the start of history
            return;
        }
        bool isRemote = mChat->getHistory(kHistBatchSize);
        if (isRemote)
        {
            createHistFetchUi();
        }
    }
    void createHistFetchUi()
    {
        mHistFetchUi.reset(new HistFetchUi(this));
        auto layout = qobject_cast<QBoxLayout*>(ui.mTitlebar->layout());
        auto bar = mHistFetchUi->progressBar();
        bar->setMinimum(0);
        bar->setMaximum(mChat->lastReqdHistCount());
        layout->insertWidget(2, bar);
    }
    void onVideoCallBtn(bool) { onCallBtn(true); }
    void onAudioCallBtn(bool) { onCallBtn(false); }
    void onMembersBtn(bool);
    void onMemberRemove();
    void onMemberSetPriv();
    void onMemberPrivateChat();
    void onScroll(int value);
public:
    ChatWindow(karere::ChatRoom& room, MainWindow& parent);
    virtual ~ChatWindow();
    chatd::Chat& chat() const { return *mChat; }
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
        auto jid = karere::useridToJid(uh);
        createCallGui();
        mRoom.parent.client.rtc->startMediaCall(mCallGui, jid, karere::AvFlags(true, video));
    }

    static MessageWidget* widgetFromMessage(const chatd::Message& msg)
    {
        if (!msg.userp)
            return nullptr;
        auto item = static_cast<QListWidgetItem*>(msg.userp);
        auto list = item->listWidget();
        return qobject_cast<MessageWidget*>(list->itemWidget(item));
    }
    QListWidgetItem* addMsgWidget(chatd::Message& msg, chatd::Idx idx,
        chatd::Message::Status status, bool first, QColor* color=nullptr)
    {
        auto widget = new MessageWidget(*this, msg, status, idx);
        connect(widget->ui.mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
//      connect(widget->ui.mAuthorDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMsgAuthorCtxMenu(const QPoint&)));

        auto* item = new QListWidgetItem;
        msg.userp = item;
        item->setSizeHint(widget->size());
        if (msg.isSending()) //we need to add it to the actual end of the list
        {
            CHAT_LOG_DEBUG("Adding unsent message widget of msgxid %s", msg.id().toString().c_str());
            ui.mMessageList->addItem(item);
        }
        else
        {
            if (first) //old message
            {
                ui.mMessageList->insertItem(0, item);
            }
            else
            {
                ui.mMessageList->insertItem(mHistAddPos, item);
            }
            mHistAddPos++; //doesn't matter if we put it at start of end, we increased the size of the history widget range
        }
        ui.mMessageList->setItemWidget(item, widget);
        if (color)
            widget->fadeIn(*color);
//        QApplication::processEvents(); //TODO: This call makes histry fetching smooth, but causes reentrancy and random crashes, find a way to make history smooth in another way
        return item;
    }
    void startEditingMsgWidget(MessageWidget& widget)
    {
        assert(!mEditedWidget);
        mEditedWidget = &widget;
        auto cancelBtn = widget.startEditing();
        connect(cancelBtn, SIGNAL(clicked()), this, SLOT(cancelMsgEdit()));
        ui.mMessageEdit->setText(widget.ui.mMsgDisplay->toPlainText());
        ui.mMessageEdit->moveCursor(QTextCursor::End);
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
    virtual void init(chatd::Chat& chat, chatd::DbInterface*& dbIntf)
    {
        mChat = &chat;
        updateOnlineIndication(mRoom.presence());
        updateChatdStatusDisplay(mChat->onlineState());
        if (mChat->empty())
            return;
        mChat->replayUnsentNotifications(); //works synchronously
        auto first = mChat->decryptedLownum();
        for (chatd::Idx idx = mChat->decryptedHighnum(); idx>=first; idx--)
        {
            auto& msg = mChat->at(idx);
            handleHistoryMsg(msg, idx, mChat->getMsgStatus(msg, idx));
        }
        mChat->loadManualSending();
        if (mChat->isFetchingHistory())
        {
            createHistFetchUi();
        }
        else
        {
            if ((mChat->size() < 16) && (mChat->onlineState() == chatd::kChatStateOnline))
            QMetaObject::invokeMethod(this, "fetchMoreHistory", Qt::QueuedConnection, Q_ARG(bool, false));
        }
    }
    virtual void onDestroy(){ close(); }
    virtual void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status)
    {
        mRoom.onRecvNewMessage(idx, msg, status);
        if (msg.empty())
            return;
        auto sbar = ui.mMessageList->verticalScrollBar();
        bool wasAtBottom = sbar->value() == sbar->maximum();
        addMsgWidget(msg, idx, status, false);
        if (wasAtBottom)
            ui.mMessageList->scrollToBottom();
        if (!isVisible() || !wasAtBottom)
        {
            printf("Playing sound\n");
//            auto file = new QFile(":/icq-incoming-msg.mp3");
//            file->open(QIODevice::ReadOnly);
        }
    }
    virtual void onRecvHistoryMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status, bool isFromDb)
    {
        assert(idx != CHATD_IDX_INVALID); assert(msg.id());
        if (mHistFetchUi)
        {
            auto bar = mHistFetchUi->progressBar();
            bar->setValue(bar->value()+1);
            bar->repaint();
        }
        handleHistoryMsg(msg, idx, status);
    }
    virtual void handleHistoryMsg(chatd::Message& msg, chatd::Idx idx, chatd::Message::Status status)
    {
        if (msg.empty())
            return;// once a message becomes empty(i.e. deleted), it can't be edited anymore, so no pending edit handling is necessary
        addMsgWidget(msg, idx, status, true);
        handlePendingEdits(msg);
    }
    virtual void onHistoryTruncated(const chatd::Message& msg, chatd::Idx idx);
    void handlePendingEdits(const chatd::Message& msg)
    {
        auto edit = mChat->pendingEdits().find(msg.id());
        if (edit != mChat->pendingEdits().end())
        {
            auto widget = widgetFromMessage(msg);
            widget->setText(*edit->second);
            widget->setStatus(chatd::Message::kSending);
            widget->setEdited();
            widget->setBgColor(QColor(Qt::yellow));
        }
    }
    void showCantEditNotice(const QString& action=QObject::tr("edit"));
    virtual void onHistoryDone(bool isFromDb)
    {
        mHistFetchUi.reset();
        if (!mChat->lastHistObtainCount())
            return;

        auto& list = *ui.mMessageList;
        //chech if we have filled the window height with history, if not, fetch more
        auto idx = list.indexAt(QPoint(list.rect().left()+10, list.rect().bottom()-2));
        if (!idx.isValid() && mChat->histFetchState() != chatd::kHistNoMore)
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
            int last = idx.isValid()
              ?(min((unsigned)idx.row(), mChat->lastHistObtainCount()))
              :list.count()-1;
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
    virtual void onMessageConfirmed(karere::Id msgxid, const chatd::Message& msg, chatd::Idx idx)
    {
        // add to history, message was just created at the server
        assert(msgxid); assert(msg.id()); assert(idx != CHATD_IDX_INVALID);
        auto widget = widgetFromMessage(msg);
        if (!widget)
        {
            CHAT_LOG_ERROR("onMessageConfirmed: No widget assigned for message with msgxid %s", msgxid.toString().c_str());
            return;
        }
#ifndef NDEBUG
        auto item = static_cast<QListWidgetItem*>(msg.userp);
        assert(item->listWidget()->row(item) == mHistAddPos);
#endif
        mHistAddPos++;
        widget->updateStatus(chatd::Message::kServerReceived);
    }
    virtual void onMessageEdited(const chatd::Message& msg, chatd::Idx idx);
    virtual void onEditRejected(const chatd::Message& msg, uint8_t opcode);
    virtual void onOnlineStateChange(chatd::ChatState state)
    {
        mRoom.onOnlineStateChange(state);
        updateChatdStatusDisplay(state);

        if ((state == chatd::kChatStateOnline) && (mChat->size() < 2)
        && ((!mChat->isFetchingHistory())))
        {
            // avoid re-entrancy - we are in a chatd callback. We could use mega::marshallCall instead,
            // but this is safer as the window may get destroyed before the message is processed
            QMetaObject::invokeMethod(this, "fetchMoreHistory", Qt::QueuedConnection, Q_ARG(bool, false));
        }
    }
    void updateChatdStatusDisplay(chatd::ChatState state)
    {
        ui.mChatdStatusDisplay->setText(chatd::chatStateToStr(mChat->onlineState()));
        if (state != chatd::kChatStateOnline)
            ui.mChatdStatusDisplay->show();
        else
            ui.mChatdStatusDisplay->hide();
    }
    virtual void onUnsentMsgLoaded(chatd::Message& msg)
    {
        auto item = addMsgWidget(msg, CHATD_IDX_INVALID, chatd::Message::kSending, false);
        msg.userp = item;
        ui.mMessageList->scrollToBottom();
    }
    virtual void onUnsentEditLoaded(chatd::Message& msg, bool oriMsgIsSending);
    virtual void onUserJoin(karere::Id userid, chatd::Priv priv)
    {
        mRoom.onUserJoin(userid, priv);
    }
    virtual void onUserLeave(karere::Id userid) { mRoom.onUserLeave(userid); }
    virtual void onUnreadChanged() { mRoom.onUnreadChanged(); }
    virtual void onManualSendRequired(chatd::Message* msg, uint64_t id, int reason);
    virtual karere::IGui::ICallGui* callGui() { return mCallGui; }
    virtual rtcModule::IEventHandler* callEventHandler() { return mCallGui; }
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
