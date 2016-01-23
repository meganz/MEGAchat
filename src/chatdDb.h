#ifndef CHATD_DB_H
#define CHATD_DB_H

#include "db.h"
#include "chatd.h"

extern sqlite3* db;

class ChatdSqliteDb: public chatd::DbInterface
{
protected:
    sqlite3* mDb;
    chatd::Messages& mMessages;
    std::string mSendingTblName;
    std::string mHistTblName;
public:
    ChatdSqliteDb(chatd::Messages& msgs, sqlite3* db, const std::string& sendingTblName="sending", const std::string& histTblName="history")
        :mDb(db), mMessages(msgs), mSendingTblName(sendingTblName), mHistTblName(histTblName){}
    virtual void getHistoryInfo(chatd::Id& oldestDbId, chatd::Id& newestDbId, chatd::Idx& newestDbIdx)
    {
        SqliteStmt stmt(mDb, "select min(idx), max(idx) from "+mHistTblName+" where chatid=?1");
        stmt.bind(mMessages.chatId()).step(); //will always return a row, even if table empty
        auto minIdx = stmt.intCol(0); //WARNING: the chatd implementation uses uint32_t values for idx.
        newestDbIdx = stmt.intCol(1);
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) //no db history
        {
            oldestDbId = 0; //no really need to zero the others
            newestDbId = newestDbIdx = 0;
            return;
        }
        SqliteStmt stmt2(mDb, "select msgid from "+mHistTblName+" where chatid=?1 and idx=?2");
        stmt2 << mMessages.chatId() << minIdx;
        stmt2.stepMustHaveData();
        oldestDbId = stmt2.uint64Col(0);
        stmt2.reset().bind(2, newestDbIdx);
        stmt2.stepMustHaveData();
        newestDbId = stmt2.uint64Col(0);
        if (!newestDbId)
        {
            CHATD_LOG_WARNING("Db: Newest msgid in db is null, telling chatd we don't have local history");
            oldestDbId = 0;
        }
    }
    virtual void saveMsgToSending(chatd::Message& msg)
    {
        SqliteStmt stmt(mDb, "insert into "+mSendingTblName+
            "(type, edits, chatid, ts, data, edits_is_xid) values(?,?,?,?,?,?)");
        stmt << msg.type << msg.edits() << mMessages.chatId() << (uint32_t)time(NULL) << msg << msg.editsIsXid();
        stmt.step();
        msg.setId(sqlite3_last_insert_rowid(mDb), true);
    }
    virtual void deleteMsgFromSending(const chatd::Id& msgxid)
    {
        SqliteStmt stmt(mDb, "delete from "+mSendingTblName+" where rowid = ?1");
        stmt << msgxid;
        stmt.step();
    }
    virtual void updateMsgInSending(const chatd::Message& msg)
    {
        SqliteStmt stmt(mDb, "update "+mSendingTblName+" set data = ?2, ts = ?3 where rowid = ?1");
        stmt << msg.id() << msg << msg.ts;
        stmt.step();
    }
    virtual void updateSendingEditId(const chatd::Id& msgxid, const chatd::Id& msgid)
    {
        SqliteStmt stmt(mDb, "update "+mSendingTblName+" set edits=?2 where edits=?1");
        stmt << msgxid << msgid;
        stmt.step();
    }

    virtual void addMsgToHistory(const chatd::Message& msg, chatd::Idx idx)
    {
        SqliteStmt stmt(mDb, "insert or replace into "+mHistTblName+
            " (idx, chatid, msgid, encrypted, type, userid, ts, edits, data) values(?,?,?,?,?,?,?,?,?);");
        stmt << idx << mMessages.chatId() << msg.id() << msg.isEncrypted << msg.type
             << msg.userid << msg.ts << msg.edits() << msg;
        stmt.step();
    }

    virtual void loadSendingTable(std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(mDb, "select rowid, type, edits, data, edits_is_xid from "+
                        mSendingTblName+" where chatid=? order by rowid asc");
        stmt << mMessages.chatId();
        while(stmt.step())
        {
            Buffer buf;
            stmt.blobCol(3, buf);
            auto msg = new chatd::Message(stmt.uint64Col(0), mMessages.client().userId(),
                0, std::move(buf), false, (chatd::Message::Type)stmt.intCol(1), nullptr, true);
            msg->setEdits(stmt.uint64Col(2), stmt.intCol(4));
            messages.push_back(msg);
        }
    }
    virtual void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(mDb, "select msgid, userid, ts, type, data, idx, edits, encrypted from "+
            mHistTblName+" where chatid=?1 and idx <= ?2 order by idx desc limit ?3");
        stmt << mMessages.chatId() << idx << count;
        int i = 0;
        while(stmt.step())
        {
            i++;
            chatd::Id msgid(stmt.uint64Col(0));
            chatd::Id userid(stmt.uint64Col(1));
            unsigned ts = stmt.uintCol(2);
            Buffer buf;
            stmt.blobCol(4, buf);
            auto idx = stmt.intCol(5);
            assert(idx == mMessages.lownum()-1-messages.size());
            auto msg = new chatd::Message(msgid, userid, ts, std::move(buf),
                stmt.intCol(7), (chatd::Message::Type)stmt.intCol(3));
            msg->setEdits(stmt.uint64Col(6), false);
            messages.push_back(msg);
        }
    }
    virtual chatd::Idx getIdxOfMsgid(chatd::Id msgid)
    {
        SqliteStmt stmt(mDb, "select idx from history where chatid = ? and msgid = ?");
        stmt << mMessages.chatId() << msgid;
        return (stmt.step()) ? stmt.int64Col(0) : CHATD_IDX_INVALID;
    }
    virtual chatd::Idx getPeerMsgCountAfterIdx(chatd::Idx idx)
    {
        SqliteStmt stmt(mDb, "select count(*) from history where"
            "(chatid = ?) and (userid != ?) and (idx > ?) and edits = 0");
        stmt << mMessages.chatId() << mMessages.client().userId() << idx;
        stmt.step();
        return stmt.intCol(0);
    }
};

#endif
