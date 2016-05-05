#ifndef CHATD_DB_H
#define CHATD_DB_H

#include "db.h"
#include "chatd.h"
extern sqlite3* db;

class ChatdSqliteDb: public chatd::DbInterface
{
protected:
    sqlite3* mDb;
    chatd::Chat& mMessages;
    std::string mSendingTblName;
    std::string mHistTblName;
public:
    ChatdSqliteDb(chatd::Chat& msgs, sqlite3* db, const std::string& sendingTblName="sending", const std::string& histTblName="history")
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
    void assertAffectedRowCount(int count, const char* opname=nullptr)
    {
        auto actual = sqlite3_changes(mDb);
        if (actual == count)
            return;
        std::string msg;
        if (opname)
            msg.append(opname).append(": ");
        msg.append(": unexpected number of rows affected: expected ")
           .append(std::to_string(count)).append(", actual ")
           .append(std::to_string(actual));
        throw std::runtime_error(msg);
    }
    void saveMsgToSending(chatd::Chat::SendingItem& item)
    {
        assert(item.msg);
        assert(item.isMessage());
        auto msg = item.msg;
        Buffer rcpts;
        item.recipients.save(rcpts);
        sqliteQuery(mDb, "insert into sending (chatid, opcode, ts, msgid, msg, "
                         "recipients, msg_cmd, key_cmd) values(?,?,?,?,?,?,?,?)",
            (uint64_t)mMessages.chatId(), item.opcode(), (int)time(NULL), msg->id(),
            *msg, rcpts,
            item.msgCmd ? (*item.msgCmd) : StaticBuffer(nullptr, 0),
            item.keyCmd ? (*item.keyCmd) : StaticBuffer(nullptr, 0));
        item.rowid = sqlite3_last_insert_rowid(mDb);
    }
    virtual void updateMsgInSending(const chatd::Chat::SendingItem& item)
    {
        assert(item.msg);
        assert(item.msgCmd);
        sqliteQuery(mDb, "update sending set msg = ?, msg_cmd = ? where rowid = ?",
            *item.msg, *item.msgCmd, item.rowid);
        assertAffectedRowCount(1, "updateMsgInSending");
    }
    virtual void confirmKeyOfSendingItem(uint64_t rowid, chatd::KeyId keyid)
    {
        sqliteQuery(mDb, "update sending set keyid = ?, keyCmd = NULL where rowid = ?",
                    keyid, rowid);
        assertAffectedRowCount(1, "confirmKeyOfSendingItem");
    }

    virtual void addBlobsToSendingItem(uint64_t rowid,
                    const chatd::MsgCommand* msgCmd, const chatd::Command* keyCmd)
    {
        sqliteQuery(mDb, "update sending set msg_cmd=?, key_cmd=? where rowid=?",
            msgCmd?*msgCmd:StaticBuffer(nullptr, 0),
            keyCmd?*keyCmd:StaticBuffer(nullptr, 0), rowid);
        assertAffectedRowCount(1,"addCommandBlobToSendingItem");
    }
    virtual void deleteItemFromSending(uint64_t rowid)
    {
        sqliteQuery(mDb, "delete from sending where rowid = ?1", rowid);
        assertAffectedRowCount(1, "deleteItemFromSending");
    }
    virtual void updateMsgPlaintextInSending(uint64_t rowid, const StaticBuffer& data)
    {
        sqliteQuery(mDb, "update sending set msg = ? where rowid = ?", data, rowid);
        assertAffectedRowCount(1, "updateMsgPlaintextInSending");
    }

    virtual void updateMsgKeyIdInSending(uint64_t rowid, chatd::KeyId keyid)
    {
        sqliteQuery(mDb, "update sending set keyid = ? where rowid = ?", keyid, rowid);
        assertAffectedRowCount(1, "updateMsgKeyIdInSending");
    }
    virtual void addMsgToHistory(const chatd::Message& msg, chatd::Idx idx)
    {
        printf("============== idx = %d\n", idx);

#if 1
        SqliteStmt stmt(mDb, "select min(idx), max(idx) from history where chatid = ?");
        stmt << mMessages.chatId();
        stmt.step();
        if ((idx != stmt.intCol(0)-1) && (idx != stmt.intCol(1)+1))
            throw std::runtime_error("addMsgToHistory: index of added msg is not adjacent to neither end of db history: add idx="
              +std::to_string(idx)+", histlow="+std::to_string(stmt.intCol(0))
              +", histhigh="+std::to_string(stmt.intCol(1)));
#endif

        sqliteQuery(mDb, "insert or replace into history"
            "(idx, chatid, msgid, keyid, type, userid, ts, data) "
            "values(?,?,?,?,?,?,?,?)", idx, mMessages.chatId(), msg.id(), msg.keyid,
            msg.type, msg.userid, msg.ts, msg);
    }
    virtual void updateMsgInHistory(chatd::Id msgid, const StaticBuffer& msg)
    {
        sqliteQuery(mDb, "update history set data = ? where msgid = ?", msg, msgid);
        assertAffectedRowCount(1, "updateMsgInHistory");
    }
    virtual void loadSendQueue(chatd::Chat::OutputQueue& queue)
    {
        SqliteStmt stmt(mDb, "select rowid, opcode, msgid, keyid, msg, type, "
            "ts, msg_cmd, key_cmd from sending where chatid=? order by rowid asc");
        stmt << mMessages.chatId();
        queue.clear();
        while(stmt.step())
        {
            uint8_t opcode = stmt.intCol(1);
            chatd::MsgCommand* msgCmd;
            if (stmt.hasBlobCol(7))
            {
                msgCmd = new chatd::MsgCommand;
                stmt.blobCol(7, *msgCmd);
                assert(msgCmd->opcode() == opcode);
            }
            else
            {
                msgCmd = nullptr;
            }

            assert((opcode == chatd::OP_NEWMSG) || (opcode == chatd::OP_MSGUPD)
                   || (opcode == chatd::OP_MSGUPDX));

            auto msg = new chatd::Message(stmt.int64Col(2), mMessages.client().userId(),
                    stmt.intCol(6), 0, nullptr, 0, true, (chatd::KeyId)stmt.intCol(3),
                    (chatd::Message::Type)stmt.intCol(5));
            stmt.blobCol(4, *msg);

            chatd::Command* keyCmd;
            if (stmt.hasBlobCol(8)) //key_cmd
            {
                keyCmd = new chatd::Command;
                stmt.blobCol(8, *keyCmd);
                assert(keyCmd->opcode() == chatd::OP_NEWKEY);
            }
            else
            {
                keyCmd = nullptr;
            }
            Buffer recpts;
            stmt.blobCol(9, recpts);
            queue.emplace_back(opcode, msg, msgCmd, keyCmd, recpts, stmt.intCol(0));
        }
    }
    virtual void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(mDb, "select msgid, userid, ts, type, data, idx from history "
            "where chatid = ?1 and idx <= ?2 order by idx desc limit ?3");
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
#ifndef NDEBUG
            auto idx = stmt.intCol(5);
            if(idx != mMessages.lownum()-1-(int)messages.size()) //we go backward in history, hence the -messages.size()
            {
                CHATD_LOG_ERROR("chatid %" PRId64 ": fetchDbHistory: Buffer discontinuity detected: "
                    "expected idx %d, retrieved from db:%d", mMessages.chatId().val,
                    mMessages.lownum()-1-(int)messages.size(), idx);
                abort();
            }
#endif
            auto msg = new chatd::Message(msgid, userid, ts, 0, std::move(buf),
                (chatd::Message::Type)stmt.intCol(3));
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
        std::string sql = "select count(*) from history where (chatid = ?)"
                "and (userid != ?)";
        if (idx != CHATD_IDX_INVALID)
            sql+=" and (idx > ?)";

        SqliteStmt stmt(mDb, sql);
        stmt << mMessages.chatId() << mMessages.client().userId();
        if (idx != CHATD_IDX_INVALID)
            stmt << idx;
        stmt.step();
        return stmt.intCol(0);
    }
};

#endif
