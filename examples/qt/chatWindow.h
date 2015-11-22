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
#include <msgInputBox.h>
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
    void check(int code, const char* opname="(unknown)")
    {
        if (code != SQLITE_OK)
            throw std::runtime_error(getLastErrorMsg());
    }
    std::string getLastErrorMsg(const char* opname="(unknown)")
    {
        const char* errMsg = sqlite3_errmsg(mDb);
        std::string msg("SqliteStmt error on operation '");
        msg.append(opname).append("': ").append(errMsg?errMsg:"(no error message");
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
        if (!mStmt)
            return;
        int ret = sqlite3_finalize(mStmt);
        if (ret != SQLITE_OK)
        {
            fprintf(stderr, "WARN: %s", getLastErrorMsg("finalize").c_str());
        }
    }
    operator sqlite3_stmt*() { return mStmt; }
    const operator sqlite3_stmt*() const {return mStmt; }
    SqliteStmt& bind(int col, int val) { check(sqlite3_bind_int(mStmt, col, val)); return *this; }
    SqliteStmt& bind(int col, const int64_t& val) { check(sqlite3_bind_int64(mStmt, col, val)); return *this; }
    SqliteStmt& bind(int col, const std::string& val) { check(sqlite3_bind_text(mStmt, col, val.c_str(), (int)val.size(), SQLITE_STATIC)); return *this; }
    SqliteStmt& bind(int col, const char* val, size_t size) { check(sqlite3_bind_text(mStmt, col, val, size, SQLITE_STATIC)); return *this; }
    SqliteStmt& bind(int col, const void* val, size_t size) { check(sqlite3_bind_blob(mStmt, col, val, size, SQLITE_STATIC)); return *this; }
    SqliteStmt& bind(int col, const Buffer& buf) { check(sqlite3_bind_blob(mStmt, col, buf.buf(), buf.dataSize(), SQLITE_STATIC)); return *this; }
    SqliteStmt& bind(int col, const uint64_t& val) { check(sqlite3_bind_int64(mStmt, col, (int64_t)val)); return *this; }
    SqliteStmt& bind(int col, unsigned int val) { check(sqlite3_bind_int(mStmt, col, (int)val)); return *this; }
    SqliteStmt& clearBind() { mLastBindCol = 0; check(sqlite3_clear_bindings(mStmt)); return *this; }
    SqliteStmt& reset() { check(sqlite3_reset(mStmt)); return *this; }
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
            throw std::runtime_error(getLastErrorMsg("step"));
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
class ColorChangeAnimator: public QObject
{
    Q_OBJECT
protected:
    int mStep;
    QTimer* mTimer;
    QColor mEndColor;
    QPalette::ColorRole mPaletteIdx;
    double mDeltaR, mDeltaG, mDeltaB;
    double mRed;
    double mGreen;
    double mBlue;
    void setColor(const QColor& col)
    {
        auto widget = qobject_cast<QWidget*>(parent());
        auto pal = widget->palette();
        pal.setColor(mPaletteIdx, col);
        widget->setPalette(pal);
    }
public:
    ColorChangeAnimator(QWidget* parent, QPalette::ColorRole paletteIdx, const QColor& startColor,
      const QColor& endColor, int steps=10, int dur=200)
        :QObject(parent), mStep(steps), mTimer(new QTimer), mEndColor(endColor),
         mPaletteIdx(paletteIdx),
         mDeltaR(double(mEndColor.red() - startColor.red()) / steps),
         mDeltaG(double(mEndColor.green() - startColor.green()) / steps),
         mDeltaB(double(mEndColor.blue() - startColor.blue()) / steps),
         mRed(startColor.red()), mGreen(startColor.green()), mBlue(startColor.blue())
    {
        setColor(startColor);
        mTimer->setSingleShot(false);
        connect(mTimer, SIGNAL(timeout()), this, SLOT(onTimer()));
        mTimer->start(dur/steps);
    }
    ~ColorChangeAnimator() { delete mTimer; }
protected slots:
    void onTimer()
    {
        if (mStep == 0)
        {
            setColor(mEndColor);
            delete this;
            return;
        }
        assert(mStep > 0);
        mStep--;
        mRed+=mDeltaR;
        mGreen+=mDeltaG;
        mBlue+=mDeltaB;
        setColor(QColor(mRed, mGreen, mBlue));
    }
};
enum {kHistBatchSize = 32};

class ChatWindow;
class MessageWidget: public QWidget
{
    Q_OBJECT
protected:
    std::unique_ptr<Ui::ChatMessage> ui;
    size_t mChatdIdx; //we only need this for the popup menu - Qt doesn't give the index of the clicked item, only a pointer to it
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
    MessageWidget(QWidget* parent, size_t chatdIdx, const chatd::Message& msg, chatd::Message::Status status, const chatd::Messages& chatdMsgs)
    : QWidget(parent), ui(new Ui::ChatMessage), mChatdIdx(chatdIdx), mIsMine(msg.userid == chatdMsgs.client().userId())
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
        mEditCount++;
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
    MessageWidget& confirmEdit(const QString& newText)
    {
        disableEditGui();
        ui->mEditDisplay->setText(tr("(edited)"));
        ui->mMsgDisplay->setText(newText);
        return *this;
    }
    MessageWidget& cancelEdit()
    {
        mEditCount--;
        disableEditGui();
        ui->mEditDisplay->setText(mEditCount?tr("(edited)"): QString());
        return *this;
    }
    void disableEditGui()
    {
        fadeIn(QColor(Qt::yellow));
        auto header = ui->mHeader->layout();
        auto btn = header->itemAt(2)->widget();
        header->removeWidget(btn);
        ui->mEditDisplay->show();
        ui->mStatusDisplay->show();
        delete btn;
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
struct HistFetchState: public QProgressBar
{
    HistFetchState(QWidget* parent): QProgressBar(parent) { setRange(0, kHistBatchSize); }
    QProgressBar* progressBar() { return this; }
};

class ChatWindow: public QDialog, public chatd::Listener
{
    Q_OBJECT
protected:
    Ui::ChatWindow* ui;
    std::map<chatd::Id, MessageWidget*> mChatdIdxToWidgetMap;
    chatd::Messages* mMessages = nullptr;
    chatd::MessageOutput* mMessageOutput = nullptr;
    size_t mEditingChatdIdx = 0; ///saves the chatd idx of the message being edited. Also signals whether we are editing or writing a new message (0 - new msg, editing otherwise)
    int mLastHistFetchCount = 0;
    std::unique_ptr<HistFetchState> mHistFetchState;
public slots:
    void onMsgSendBtn()
    {
        auto msg = ui->mMessageEdit->toPlainText().toUtf8();
        if (mEditingChatdIdx)
        {
            auto idx = mEditingChatdIdx; //just in case somethong throws
            mEditingChatdIdx = 0;
            mMessageOutput->msgModify(idx, msg.data(), msg.size());
//save to 'sending' table
            auto& msg = mMessages->at(idx);
            SqliteStmt stmt(karere::db, "insert into sending(idx, flags, chatid, msgid, userid, ts, data) values(?,?,?,?,?,?,?)");
            stmt << (unsigned)idx << (int)1 << mMessages->chatId() << msg.id << msg.userid << msg.ts << msg;
            stmt.step();
//===
            msgWidgetAt(idx).confirmEdit(ui->mMessageEdit->toPlainText());
        }
        else
        {
            if (msg.isEmpty())
                return;
            auto idx = mMessageOutput->msgSubmit(msg.data(), msg.size());
//save to 'sending' table
            auto& msg = mMessages->at(idx);
            SqliteStmt stmt(karere::db, "insert into sending(idx, flags, chatid, msgid, userid, ts, data) values(?,?,?,0,?,?,?)");
            stmt << (unsigned)idx << (int)0 << mMessages->chatId() << msg.userid << msg.ts << msg;
            stmt.step();
//===
            addMsgWidget(idx, mMessages->at(idx), chatd::Message::kSending, false);
            assert(idx - mMessages->lownum() == ui->mMessageList->count()-1);
            ui->mMessageList->scrollToBottom();
        }
        ui->mMessageEdit->setText(QString());
    }
    void onMessageCtxMenu(const QPoint& point)
    {
        auto msgWidget = qobject_cast<MessageWidget*>(QObject::sender()->parent());
        auto idx = msgWidget->mChatdIdx;
        //enable edit action only if the message is ours
        auto menu = msgWidget->ui->mMsgDisplay->createStandardContextMenu(point);

        if (mMessages->client().userId() == mMessages->at(idx).userid)
        {
            auto action = menu->addAction(tr("&Edit message"));
            action->setData((unsigned long long)idx);
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
        mEditingChatdIdx = (size_t)action->data().toULongLong();
        auto& msg = msgWidgetAt(mEditingChatdIdx);
        assert(msg.mChatdIdx == mEditingChatdIdx);
        startEditingMsgWidget(msg);
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
                mEditingChatdIdx = widget->mChatdIdx;
                startEditingMsgWidget(*widget);
                return;
            }
        }
    }
    void cancelMsgEdit()
    {
        assert(mEditingChatdIdx);
        msgWidgetAt(mEditingChatdIdx).cancelEdit();
        ui->mMessageEdit->setText(QString());
    }
    void onMsgListRequestHistory(int scrollDelta)
    {
        if (mHistFetchState) //Only checks for ongoing network fetch, we can't be already fetching db history, as that process is synchronous
            return;
        int fetchCount = scrollDelta / 10;
        if (fetchCount < 4)
            fetchCount = 4;
        else if (fetchCount > 10)
            fetchCount = 10;
        printf("delta: %d, fetch %d\n", scrollDelta, fetchCount);

        mLastHistFetchCount = 0;
        bool isRemote = mMessages->getHistory(fetchCount);
        if (isRemote)
        {
            mHistFetchState.reset(new HistFetchState(this));
            auto layout = qobject_cast<QBoxLayout*>(ui->titlebar->layout());
            layout->insertWidget(1, mHistFetchState->progressBar());
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
    MessageWidget& msgWidgetAt(size_t chatdIdx)
    {
        auto it = mChatdIdxToWidgetMap.find(chatdIdx);
        if (it == mChatdIdxToWidgetMap.end())
            throw std::runtime_error("msgWidgetAt: Unknown chatd index "+std::to_string(chatdIdx));

        auto& widget = *it->second;
        assert(widget.mChatdIdx == chatdIdx);
        return widget;
    }

    void addMsgWidget(size_t chatdIdx, const chatd::Message& msg, chatd::Message::Status status,
                      bool first, QColor* color=nullptr)
    {
        auto widget = new MessageWidget(this, chatdIdx, msg, status, *mMessages);
        connect(widget->ui->mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
//      connect(widget->ui->mAuthorDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMsgAuthorCtxMenu(const QPoint&)));

        auto* item = new QListWidgetItem;
        item->setSizeHint(widget->size());
        mChatdIdxToWidgetMap[chatdIdx] = widget;
        if (first)
        {
            ui->mMessageList->insertItem(0, item);
        }
        else
        {
            ui->mMessageList->addItem(item);
        }
        ui->mMessageList->setItemWidget(item, widget);
        if (color)
            widget->fadeIn(*color);
    }
    void startEditingMsgWidget(MessageWidget& msg)
    {
        auto cancelBtn = msg.startEditing();
        connect(cancelBtn, SIGNAL(clicked()), this, SLOT(cancelMsgEdit()));
        auto& chatdMsg = mMessages->at(mEditingChatdIdx);
        ui->mMessageEdit->setText(QString().fromUtf8(chatdMsg.buf(), chatdMsg.dataSize()));
        ui->mMessageEdit->moveCursor(QTextCursor::End);
    }

    chatd::Listener* listenerInterface() { return static_cast<chatd::Listener*>(this); }
public:
    //chatd::Listener interface
    virtual void onInit(chatd::Messages* messages, chatd::MessageOutput* out,chatd::Id& oldestDbId,
                        chatd::Id& newestDbId, size_t& newestDbIdx)
    {
        mMessages = messages;
        mMessageOutput = out;
        SqliteStmt stmt(karere::db, "select min(idx), max(idx) from history where chatid=?1");
        stmt.bind(mMessages->chatId()).step(); //will always return a row, even if table empty
        size_t minIdx = stmt.uintCol(0); //WARNING: the chatd implementation uses 32bit values for idx, even though it puts them in platform-dependent size_t variables. So we are guaranteed that these indexes will fit in size_t even for 32bit platforms
        newestDbIdx = stmt.uintCol(1);
        if (!minIdx) //no db history
        {
            oldestDbId = 0; //no really need to zero the others
            newestDbId = newestDbIdx = 0;
            return;
        }
        SqliteStmt stmt2(karere::db, "select msgid from history where chatid=?1 and idx=?2");
        stmt2 << mMessages->chatId().val << (unsigned)minIdx;
        stmt2.stepMustHaveData();
        oldestDbId = stmt2.uint64Col(0);
        stmt2.reset().bind(2, (unsigned)newestDbIdx);
        stmt2.stepMustHaveData();
        newestDbId = stmt2.uint64Col(0);
        if (!newestDbId)
        {
            oldestDbId = 0;
        }
    }
    virtual void onDestroy(){ close(); }
    virtual void onRecvNewMessage(size_t idx, const chatd::Message& msg, chatd::Message::Status status)
    {
        SqliteStmt stmt(karere::db, "insert into history(idx, chatid, msgid, userid, ts, data) values(?1, ?2, ?3, ?4, ?5, ?6);");
        stmt << (unsigned)idx << mMessages->chatId() << msg.id << msg.userid << msg.ts << msg;
        stmt.step();
        addMsgWidget(idx, msg, status, false);
        ui->mMessageList->scrollToBottom();
    }
    virtual void onRecvHistoryMessage(size_t idx, const chatd::Message& msg, chatd::Message::Status status, bool isFromDb)
    {
        mLastHistFetchCount++;
        if (mHistFetchState)
        {
            mHistFetchState->progressBar()->setValue(mLastHistFetchCount);
            mHistFetchState->progressBar()->repaint();
        }
        if (!isFromDb)
        {
            SqliteStmt stmt(karere::db, "insert into history(idx, chatid, msgid, userid, ts, data) values(?1, ?2, ?3, ?4, ?5, ?6);");
            stmt << (unsigned)idx << mMessages->chatId() << msg.id << msg.userid << msg.ts << msg;
            stmt.step();
        }
        addMsgWidget(idx, msg, status, true);
    }
    virtual void onHistoryDone()
    {
        mHistFetchState.reset();
        if (!mLastHistFetchCount)
            return;
        auto& list = *ui->mMessageList;
        auto idx = list.indexAt(QPoint(list.rect().left()+10, list.rect().bottom()-2));
        int last = (idx.isValid())?std::min(idx.row(), mLastHistFetchCount):mLastHistFetchCount;
        for (int i=0; i<=last; i++)
            qobject_cast<MessageWidget*>(list.itemWidget(list.item(i)))->fadeIn(QColor(250,250,250));
        printf("history done %d\n", last);
    }
    virtual void onMessageStatusChange(size_t idx, chatd::Message::Status newStatus, unsigned flags)
    {
        if (newStatus != chatd::Message::kServerReceived)
            return;
        auto& msg = mMessages->at(idx);
        if (flags & chatd::Message::kCreated)
        { // add to history, message was just created at the server
            SqliteStmt stmt(karere::db, "insert into history(idx, chatid, userid, ts, data) values(?1, ?2, ?3, ?4, ?5)");
            stmt << (unsigned)idx << mMessages->chatId() << msg.userid << msg.ts << msg;
            stmt.step();
        }
        else
        { // message update was confirmed by server, update it in history db as well
            SqliteStmt stmt(karere::db, "update history set data = ?3 where chatid = ?2 and idx = ?1");
            stmt << (unsigned)idx << mMessages->chatId() << msg;
            stmt.step();
        }
        // delete from temporary storage in sending table
        SqliteStmt stmt(karere::db, "delete from sending where chatid=?1 and idx = ?2");
        stmt << mMessages->chatId() << (unsigned)idx;
        stmt.step();

        msgWidgetAt(idx).updateStatus(newStatus);
    }
    virtual void onMessageEdited(size_t idx, chatd::Message& newmsg)
    {
        //update in history db
        SqliteStmt stmt(karere::db, "update history set data = ?3 where chatid = ?1 and idx = ?2");
        stmt << mMessages->chatId() << (unsigned)idx << mMessages->at(idx);
        stmt.step();
        msgWidgetAt(idx).setText(newmsg).setTimestamp(newmsg.ts).setEdited().fadeIn(Qt::yellow);
    }
    virtual void onOnlineStateChange(chatd::ChatState state)
    {
        ui->mOnlineStateDisplay->setText(chatd::chatStateToStr(state));
        if (state == chatd::kChatStateOnline)
        {
            ui->mMessageList->scrollToBottom();
        }
    }
    virtual bool fetchDbHistory(size_t idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
      try
      {
        SqliteStmt stmt(karere::db, "select msgid, userid, ts, data, idx from history where chatid=?1 and idx <= ?2 order by idx desc limit ?3");
        stmt << mMessages->chatId().val << (unsigned)idx << count;
        int i = 0;
        while(stmt.step())
        {
            i++;
            chatd::Id msgid(stmt.uint64Col(0));
            chatd::Id userid(stmt.uint64Col(1));
            unsigned ts = stmt.uintCol(2);
            Buffer buf;
            stmt.blobCol(3, buf);
            unsigned idx = stmt.uintCol(4);
            printf("count: %d, idx matches: %d\n", count, idx == mMessages->lownum()-1-messages.size());
            messages.push_back(new chatd::Message(msgid, userid, ts, std::move(buf)));
        }
        return (i >= count);
      }
      catch(std::exception& e)
      {
          CHATD_LOG_ERROR("Exception in fetchDbHistory callback: %s", e.what());
          return false;
      }
    }
};

#endif // CHATWINDOW_H
