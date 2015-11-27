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
//#include <msgInputBox.h>
#include <QScrollBar>
#include <QTimer>
#include <QProgressBar>
#include <sqlite3.h>
#include <autoHandle.h>

class SqliteStmt
{
protected:
    sqlite3_stmt* mStmt;
    sqlite3* mDb;
    int mLastBindCol = 0;
    void check(int code, const char* opname)
    {
        if (code != SQLITE_OK)
            throw std::runtime_error(getLastErrorMsg(opname));
    }
    std::string getLastErrorMsg(const char* opname)
    {
        std::string msg("SqliteStmt error ");
        msg.append(std::to_string(sqlite3_errcode(mDb))).append(" on ");
        if (opname)
        {
            msg.append("operation '").append(opname).append("': ");
        }
        else
        {
            const char* sql = sqlite3_sql(mStmt);
            if (sql)
                msg.append("query\n").append(sql).append("\n");
        }
        const char* errMsg = sqlite3_errmsg(mDb);
        msg.append(errMsg?errMsg:"(no error message)");
        return msg;
    }
public:
    SqliteStmt(sqlite3* db, const char* sql) :mDb(db)
    {
        assert(db);
        check(sqlite3_prepare_v2(db, sql, -1, &mStmt, nullptr), "create statement");
        assert(mStmt);
    }
    ~SqliteStmt()
    {
        if (mStmt)
            sqlite3_finalize(mStmt);
    }
    operator sqlite3_stmt*() { return mStmt; }
    const operator sqlite3_stmt*() const {return mStmt; }
    SqliteStmt& bind(int col, int val) { check(sqlite3_bind_int(mStmt, col, val), "bind"); return *this; }
    SqliteStmt& bind(int col, const int64_t& val) { check(sqlite3_bind_int64(mStmt, col, val), "bind"); return *this; }
    SqliteStmt& bind(int col, const std::string& val) { check(sqlite3_bind_text(mStmt, col, val.c_str(), (int)val.size(), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const char* val, size_t size) { check(sqlite3_bind_text(mStmt, col, val, size, SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const void* val, size_t size) { check(sqlite3_bind_blob(mStmt, col, val, size, SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const Buffer& buf) { check(sqlite3_bind_blob(mStmt, col, buf.buf(), buf.dataSize(), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const uint64_t& val) { check(sqlite3_bind_int64(mStmt, col, (int64_t)val), "bind"); return *this; }
    SqliteStmt& bind(int col, unsigned int val) { check(sqlite3_bind_int(mStmt, col, (int)val), "bind"); return *this; }
    SqliteStmt& clearBind() { mLastBindCol = 0; check(sqlite3_clear_bindings(mStmt), "clear bindings"); return *this; }
    SqliteStmt& reset() { check(sqlite3_reset(mStmt), "reset"); return *this; }
    template <class T>
    SqliteStmt& bind(const T& val) { bind(++mLastBindCol, val); return *this; }
    template <class T>
    SqliteStmt& operator<<(const T& val) { return bind(val);}
    bool step()
    {
        int ret = sqlite3_step(mStmt);
        if (ret == SQLITE_DONE)
            return false;
        else if (ret == SQLITE_ROW)
            return true;
        else
            throw std::runtime_error(getLastErrorMsg(nullptr));
    }
    void stepMustHaveData() { if (!step()) throw std::runtime_error("SqliteStmt::stepMustHaveData: No rows returned"); }
    int intCol(int num) { return sqlite3_column_int(mStmt, num); }
    int64_t int64Col(int num) { return sqlite3_column_int64(mStmt, num); }
    std::string stringCol(int num)
    {
        const unsigned char* data = sqlite3_column_text(mStmt, num);
        if (!data)
            return std::string();
        int size = sqlite3_column_bytes(mStmt, num);
        return std::string((const char*)data, size);
    }
    void blobCol(int num, Buffer& buf)
    {
        buf.clear();
        const void* data = sqlite3_column_blob(mStmt, num);
        if (!data)
            return;
        int size = sqlite3_column_bytes(mStmt, num);
        buf.assign(data, size);
    }
    uint64_t uint64Col(int num) { return (uint64_t)sqlite3_column_int64(mStmt, num);}
    unsigned int uintCol(int num) { return (unsigned int)sqlite3_column_int(mStmt, num);}
};

namespace Ui
{
class ChatWindow;
}

//TODO: Implement properly
namespace karere
{
static inline promise::Promise<std::string> getUsernameFromHandle(const chatd::Id& userid)
{
    return promise::Promise<std::string>(userid.toString());
}
extern sqlite3* db;
}

enum {kHistBatchSize = 32};

class ChatWindow;
class MessageWidget: public QWidget
{
    Q_OBJECT
protected:
    std::unique_ptr<Ui::ChatMessage> ui;
    const chatd::Message* mMessage; //we only need this for the popup menu - Qt doesn't give the index of the clicked item, only a pointer to it
    const chatd::Message& mOriginal;
    short mEditCount = 0; //counts how many times we have edited the message, so that we can keep the edited display even after a canceled edit, in case there was a previous edit
    bool mIsMine;
    Q_PROPERTY(QColor msgColor READ msgColor WRITE setMsgColor)
    QColor msgColor() { return palette().color(QPalette::Base); }
    void setMsgColor(const QColor& color)
    {
        QPalette p = ui->mMsgDisplay->palette();
        p.setColor(QPalette::Base, color);
        ui->mMsgDisplay->setPalette(p);
    }
    friend class ChatWindow;
public:
    MessageWidget(QWidget* parent, const chatd::Message& msg, chatd::Message::Status status, const chatd::Messages& chatdMsgs)
    : QWidget(parent), ui(new Ui::ChatMessage), mMessage(&msg), mOriginal(msg), mIsMine(msg.userid == chatdMsgs.client().userId())
    {
        ui->setupUi(this);
        setAuthor(msg.userid);
        setTimestamp(msg.ts);
        setStatus(status);
        setText(msg);
        show();
    }
    MessageWidget& setAuthor(const chatd::Id& userid)
    {
        if (mIsMine)
        {
            ui->mAuthorDisplay->setText(tr("me"));
        }
        else
        {
            karere::getUsernameFromHandle(userid)
            .then([this](const std::string& username)
            {
                ui->mAuthorDisplay->setText(QString().fromUtf8(username.c_str(), username.size()));
            });
        }
        return *this;
    }

    MessageWidget& setTimestamp(uint32_t ts)
    {
        QDateTime t;
        t.setTime_t(ts);
        ui->mTimestampDisplay->setText(t.toString("hh:mm:ss dd.MM.yyyy"));
        return *this;
    }
    MessageWidget& setStatus(chatd::Message::Status status)
    {
        ui->mStatusDisplay->setText(chatd::Message::statusToStr(status));
        return *this;
    }
    MessageWidget& setText(const chatd::Message& msg)
    {
        ui->mMsgDisplay->setText(QString::fromUtf8(msg.buf(), msg.dataSize()));
        return *this;
    }
    MessageWidget& updateStatus(chatd::Message::Status newStatus)
    {
        ui->mStatusDisplay->setText(chatd::Message::statusToStr(newStatus));
        return *this;
    }
    QPushButton* startEditing()
    {
        setBgColor(Qt::yellow);
        ui->mEditDisplay->hide();
        ui->mStatusDisplay->hide();

        auto btn = new QPushButton(this);
        btn->setText("Cancel edit");
        auto layout = static_cast<QBoxLayout*>(ui->mHeader->layout());
        layout->insertWidget(2, btn);
        this->layout();
        return btn;
    }
    MessageWidget& cancelEdit()
    {
        disableEditGui();
        ui->mEditDisplay->setText(mEditCount?tr("(edited)"): QString());
        return *this;
    }
    MessageWidget& disableEditGui()
    {
        fadeIn(QColor(Qt::yellow));
        auto header = ui->mHeader->layout();
        auto btn = header->itemAt(2)->widget();
        header->removeWidget(btn);
        ui->mEditDisplay->show();
        ui->mStatusDisplay->show();
        delete btn;
        return *this;
    }
    MessageWidget& setBgColor(const QColor& color)
    {
        QPalette p = ui->mMsgDisplay->palette();
        p.setColor(QPalette::Base, color);
        ui->mMsgDisplay->setPalette(p);
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
        ui->mEditDisplay->setText(tr("(Edited)"));
        return *this;
    }
};
struct HistFetchUi: public QProgressBar
{
    HistFetchUi(QWidget* parent): QProgressBar(parent) { setRange(0, kHistBatchSize); }
    QProgressBar* progressBar() { return this; }
};

class ChatWindow: public QDialog, public chatd::Listener
{
    Q_OBJECT
protected:
    Ui::ChatWindow* ui;
    chatd::Messages* mMessages = nullptr;
    chatd::MessageOutput* mMessageOutput = nullptr;
    MessageWidget* mEditedWidget = nullptr; ///pointer to the widget being edited. Also signals whether we are editing or writing a new message (nullptr - new msg, editing otherwise)
    std::map<chatd::Id, const chatd::Message*> mNotLinkedEdits;
    bool mUnsentChecked = false;
    std::unique_ptr<HistFetchUi> mHistFetchUi;
public slots:
    void onMsgSendBtn()
    {
        auto text = ui->mMessageEdit->toPlainText().toUtf8();
        //save to 'sending' table
        SqliteStmt stmt(karere::db, "insert into sending(edited, chatid, ts, data) values(?,?,?,?)");
        stmt << (mEditedWidget ? mEditedWidget->mOriginal.id : chatd::Id::null())
             << mMessages->chatId() << (uint32_t)time(NULL);
        stmt.bind(4, text.data(), text.size());
        stmt.step();
        //===
        int64_t rowid = sqlite3_last_insert_rowid(karere::db);
        if (mEditedWidget)
        {
            auto widget = mEditedWidget;
            auto& msg = *mEditedWidget->mMessage;
            mEditedWidget = nullptr;
            if ((text.size() == msg.dataSize())
              && (memcmp(text.data(), msg.buf(), text.size()) == 0))
            { //no change
                widget->disableEditGui();
                ui->mMessageEdit->setText(QString());
                return;
            }

            auto& original = widget->mOriginal;
            //TODO: see how to associate the edited message widget with the userp
            auto edited = mMessageOutput->msgModify(original.id, text.data(), text.size(), rowid, original.userp);
            addMsgEdit(*edited, original);
            widget->disableEditGui().updateStatus(chatd::Message::kSending);
        }
        else
        {
            if (text.isEmpty())
                return;
            auto msg = mMessageOutput->msgSubmit(text.data(), text.size(), rowid, nullptr);
            msg->userp = addMsgWidget(*msg, chatd::Message::kSending, false);
            ui->mMessageList->scrollToBottom();
        }
        ui->mMessageEdit->setText(QString());
    }
    void onMessageCtxMenu(const QPoint& point)
    {
        auto msgWidget = qobject_cast<MessageWidget*>(QObject::sender()->parent());
        //enable edit action only if the message is ours
        auto menu = msgWidget->ui->mMsgDisplay->createStandardContextMenu(point);

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
        auto msglist = ui->mMessageList;
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
        ui->mMessageEdit->setText(QString());
    }
    void onMsgListRequestHistory(int scrollDelta)
    {
        fetchMoreHistory();
    }
    void fetchMoreHistory()
    {
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
            auto layout = qobject_cast<QBoxLayout*>(ui->titlebar->layout());
            layout->insertWidget(1, mHistFetchUi->progressBar());
        }
    }
public:
    ChatWindow(QWidget* parent): QDialog(parent), ui(new Ui::ChatWindow)
    {
        ui->setupUi(this);
        ui->mMessageList->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->mMsgSendBtn, SIGNAL(clicked()), this, SLOT(onMsgSendBtn()));
        connect(ui->mMessageEdit, SIGNAL(sendMsg()), this, SLOT(onMsgSendBtn()));
        connect(ui->mMessageEdit, SIGNAL(editLastMsg()), this, SLOT(editLastMsg()));
        connect(ui->mMessageList, SIGNAL(requestHistory(int)), this, SLOT(onMsgListRequestHistory(int)));
        show();
    }
protected:
    MessageWidget* widgetFromMessage(const chatd::Message& msg)
    {
        if (!msg.userp)
            return nullptr;
        return qobject_cast<MessageWidget*>(ui->mMessageList->itemWidget(static_cast<QListWidgetItem*>(msg.userp)));
    }
    QListWidgetItem* addMsgWidget(const chatd::Message& msg, chatd::Message::Status status,
                      bool first, QColor* color=nullptr)
    {
        auto widget = new MessageWidget(this, msg, status, *mMessages);
        connect(widget->ui->mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
//      connect(widget->ui->mAuthorDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMsgAuthorCtxMenu(const QPoint&)));

        auto* item = new QListWidgetItem;
        msg.userp = item;
        item->setSizeHint(widget->size());
        if (first)
        {
            ui->mMessageList->insertItem(0, item);
        }
        else
        {
            ui->mMessageList->addItem(item);
        }
        ui->mMessageList->setItemWidget(item, widget);
        if(!checkHandleInboundEdits(msg))
        {
            if (color)
                widget->fadeIn(*color);
        }
        return item;
    }
    void addMsgEdit(const chatd::Message& msg, bool first, QColor* color=nullptr)
    {
        assert(msg.edits);
        auto idx = mMessages->msgIndexFromId(msg.edits);
        if (idx == CHATD_IDX_INVALID) //maybe message is not loaded in buffer? Ignore it then
        {
            auto it = mNotLinkedEdits.find(msg.edits);
            if ((it == mNotLinkedEdits.end()) || !first) //we are adding an oldest message, can't be a more recent edit, ignore
                mNotLinkedEdits[msg.edits] = &msg;
            msg.userp = nullptr;
            CHATD_LOG_DEBUG("Can't find original message of edit, adding to map");
            return;
        }
        auto& targetMsg = mMessages->at(idx);
        assert(targetMsg.id == msg.edits);
        addMsgEdit(msg, targetMsg, color);
    }
    void addMsgEdit(const chatd::Message& edited, const chatd::Message& original, QColor* color=nullptr)
    {
        auto widget = widgetFromMessage(original);
//User pointer of referenced message by an edit can be null only if it's an edit itself. But edits
//can't point to other edits, they must reference only the original message and can't be chained
        if (!widget)
        {
            CHATD_LOG_ERROR("Edit message %lld points to another edit message %lld, ignoring message", edited.id, original.id);
            return;
        }
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
        ui->mMessageEdit->setText(QString().fromUtf8(widget.mMessage->buf(), widget.mMessage->dataSize()));
        ui->mMessageEdit->moveCursor(QTextCursor::End);
    }
    bool checkHandleInboundEdits(const chatd::Message& msg)
    {
        assert(msg.userp); //message must be in the GUI already
        auto it = mNotLinkedEdits.find(msg.id);
        if (it == mNotLinkedEdits.end())
            return false;
        CHATD_LOG_DEBUG("Loaded message that had edits pending");
        auto widget = widgetFromMessage(msg);
        auto updated = widget->mMessage = it->second;
        widget->setText(*updated).setEdited();
        mNotLinkedEdits.erase(it);
        return true;
    }
    chatd::Listener* listenerInterface() { return static_cast<chatd::Listener*>(this); }
public:
    //chatd::Listener interface
    virtual void init(chatd::Messages* messages, chatd::MessageOutput* out,chatd::Id& oldestDbId,
                        chatd::Id& newestDbId, chatd::Idx& newestDbIdx)
    {
        mMessages = messages;
        mMessageOutput = out;
        SqliteStmt stmt(karere::db, "select min(idx), max(idx) from history where chatid=?1");
        stmt.bind(mMessages->chatId()).step(); //will always return a row, even if table empty
        auto minIdx = stmt.int64Col(0); //WARNING: the chatd implementation uses uint32_tvalues for idx.
        newestDbIdx = stmt.int64Col(1);
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) //no db history
        {
            CHATD_LOG_WARNING("App: No local history found");
            oldestDbId = 0; //no really need to zero the others
            newestDbId = newestDbIdx = 0;
            return;
        }
        SqliteStmt stmt2(karere::db, "select msgid from history where chatid=?1 and idx=?2");
        stmt2 << mMessages->chatId().val << (int64_t)minIdx;
        stmt2.stepMustHaveData();
        oldestDbId = stmt2.uint64Col(0);
        stmt2.reset().bind(2, (int64_t)newestDbIdx);
        stmt2.stepMustHaveData();
        newestDbId = stmt2.uint64Col(0);
        if (!newestDbId)
        {
            CHATD_LOG_WARNING("App: Newest msgid in db is null, telling chatd we don't have local history");
            oldestDbId = 0;
        }
        printf("app range: %lld - % lld\n", oldestDbId.val, newestDbId.val);
    }
    virtual void onDestroy(){ close(); }
    virtual void onRecvNewMessage(chatd::Idx idx, const chatd::Message& msg, chatd::Message::Status status)
    {
        SqliteStmt stmt(karere::db, "insert into history(idx, chatid, msgid, userid, ts, edits, data) values(?1,?2,?3,?4,?5,?6,?7);");
        stmt << (int64_t)idx << mMessages->chatId() << msg.id << msg.userid << msg.ts << msg.edits << msg;
        stmt.step();
        if (msg.edits)
            addMsgEdit(msg, false);
        else
            addMsgWidget(msg, status, false);
        mMessages->setMessageSeen(idx);
        ui->mMessageList->scrollToBottom();
    }
    virtual void onRecvHistoryMessage(chatd::Idx idx, const chatd::Message& msg, chatd::Message::Status status, bool isFromDb)
    {
        assert(idx != CHATD_IDX_INVALID); assert(msg.id);
        if (mHistFetchUi)
        {
            mHistFetchUi->progressBar()->setValue(mMessages->lastHistFetchCount());
            mHistFetchUi->progressBar()->repaint();
        }
        if (!isFromDb)
        {
            SqliteStmt stmt(karere::db, "insert into history(idx, chatid, msgid, userid, ts, edits, data) values(?1, ?2, ?3, ?4, ?5, ?6, ?7);");
            stmt << idx << mMessages->chatId().val << msg.id << msg.userid << msg.ts << msg.edits << msg;
            stmt.step();
        }
        if (msg.edits)
            addMsgEdit(msg, true);
        else
            addMsgWidget(msg, status, true);
//        ui->mMessageList->scrollToBottom();
    }
    virtual void onHistoryDone(bool isFromDb)
    {
        mHistFetchUi.reset();
        if (!mMessages->lastHistFetchCount())
            return;

        auto& list = *ui->mMessageList;
        auto idx = list.indexAt(QPoint(list.rect().left()+10, list.rect().bottom()-2));
        if (!idx.isValid() && mMessages->histFetchState() != chatd::kHistNoMore)
        {
            fetchMoreHistory();
            return;
        }
        int last = (idx.isValid())?std::min((unsigned)idx.row(), mMessages->lastHistFetchCount()):list.count()-1;
        for (int i=0; i<=last; i++)
            qobject_cast<MessageWidget*>(list.itemWidget(list.item(i)))->fadeIn(QColor(250,250,250));
    }
    virtual void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message& msg)
    {
        auto widget = widgetFromMessage(msg);
        if (widget)
            widget->updateStatus(newStatus);
    }
    virtual void onMessageConfirmed(const chatd::Id& msgxid, const chatd::Id& msgid, chatd::Idx idx)
    {
        // add to history, message was just created at the server
        assert(msgxid); assert(msgid); assert(idx != CHATD_IDX_INVALID);
        auto& msg = mMessages->at(idx);
        (SqliteStmt(karere::db, "insert into history(idx, msgid, chatid, userid, ts, edits, data) values(?,?,?,?,?,?,?)")
          << (int64_t)idx << msgid << mMessages->chatId() << msg.userid << msg.ts << msg.edits << msg).step();

        // delete from temporary storage in sending table
        (SqliteStmt(karere::db, "delete from sending where rowid = ?2")
          << mMessages->chatId() << msgxid).step();

        auto widget = widgetFromMessage(msg);
        if (widget)
            widget->updateStatus(chatd::Message::kServerReceived);
    }
    virtual void onOnlineStateChange(chatd::ChatState state)
    {
        ui->mOnlineStateDisplay->setText(chatd::chatStateToStr(state));
        if (state != chatd::kChatStateOnline)
            return;
        //we are online - we need to have fetched all new messages to be able to send unsent ones,
        //because the crypto layer needs to have received the most recent keys
        if (!mUnsentChecked)
        {
            mUnsentChecked = true;
            SqliteStmt stmt(karere::db, "select rowid, edited, data from sending where chatid=? order by rowid asc");
            stmt << mMessages->chatId();
            while(stmt.step())
            {
                chatd::Id msgxid = stmt.uint64Col(0);
                chatd::Id edited = stmt.uint64Col(1);
                Buffer buf;
                stmt.blobCol(2, buf);
                if (edited)
                {
                    auto msg = mMessageOutput->msgModify(edited, buf.buf(), buf.dataSize(), msgxid, nullptr);
                    addMsgEdit(*msg, false);
                }
                else
                {
                    auto msg = mMessageOutput->msgSubmit(buf.buf(), buf.dataSize(), msgxid, nullptr);
                    auto item = addMsgWidget(*msg, chatd::Message::kSending, false);
                    msg->userp = item;
                }
            }
        }
        ui->mMessageList->scrollToBottom();
    }
    virtual void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(karere::db, "select msgid, userid, ts, data, idx, edits from history where chatid=?1 and idx <= ?2 order by idx desc limit ?3");
        stmt << mMessages->chatId().val << (int64_t)idx << count;
        int i = 0;
        while(stmt.step())
        {
            i++;
            chatd::Id msgid(stmt.uint64Col(0));
            chatd::Id userid(stmt.uint64Col(1));
            unsigned ts = stmt.uintCol(2);
            Buffer buf;
            stmt.blobCol(3, buf);
            auto idx = stmt.uint64Col(4);
            assert(idx == mMessages->lownum()-1-messages.size());
            auto msg = new chatd::Message(msgid, userid, ts, std::move(buf));
            msg->edits = stmt.uint64Col(5);
            messages.push_back(msg);
        }
    }
};

#endif // CHATWINDOW_H
