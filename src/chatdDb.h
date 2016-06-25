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
    virtual void getHistoryInfo(karere::Id& oldestDbId, karere::Id& newestDbId, chatd::Idx& newestDbIdx)
    {
        SqliteStmt stmt(mDb, "select min(idx), max(idx) from history where chatid=?1");
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
                         "recipients, backrefid, backrefs, msg_cmd, key_cmd) values(?,?,?,?,?,?,?,?,?,?)",
            (uint64_t)mMessages.chatId(), item.opcode(), (int)time(NULL), msg->id(),
            *msg, rcpts, msg->backRefId, msg->backrefBuf(),
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
        sqliteQuery(mDb, "update sending set keyid = ?, key_cmd = NULL where rowid = ?",
                    keyid, rowid);
        assertAffectedRowCount(1, "confirmKeyOfSendingItem");
    }

    virtual void addBlobsToSendingItem(uint64_t rowid,
                    const chatd::MsgCommand* msgCmd, const chatd::Command* keyCmd)
    {
        //WARNING: Must cast *msgCmd and *keyCmd to StaticBuffer, otherwise
        //compiler (at least clang on MacOS) seems not able to properly determine
        //the argument type for the template parameter to sqlQuery(), which
        //compiles without any warning, but results is corrupt data written to the db!
        sqliteQuery(mDb, "update sending set msg_cmd=?, key_cmd=? where rowid=?",
            msgCmd?static_cast<StaticBuffer>(*msgCmd):StaticBuffer(nullptr, 0),
            keyCmd?static_cast<StaticBuffer>(*keyCmd):StaticBuffer(nullptr, 0), rowid);
        assertAffectedRowCount(1,"addCommandBlobToSendingItem");
    }
    virtual void sendingItemMsgupdxToMsgupd(const chatd::Chat::SendingItem& item, karere::Id msgid)
    {
        assert(item.opcode() == chatd::OP_MSGUPDX);
        sqliteQuery(mDb,
            "update sending set opcode=?, msgid=? where chatid=? and rowid=? and opcode=? and msgid=?",
            chatd::OP_MSGUPD, msgid, mMessages.chatId(), item.rowid, chatd::OP_MSGUPDX, item.msg->id());
        assertAffectedRowCount(1, "updateSendingItemMsgidAndOpcode");
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
            "(idx, chatid, msgid, keyid, type, userid, ts, data, backrefid, backrefs) "
            "values(?,?,?,?,?,?,?,?,?,?)", idx, mMessages.chatId(), msg.id(), msg.keyid,
            msg.type, msg.userid, msg.ts, msg, msg.backRefId, msg.backrefBuf());
    }
    virtual void updateMsgInHistory(karere::Id msgid, const StaticBuffer& msg)
    {
        sqliteQuery(mDb, "update history set data = ? where chatid = ? and msgid = ?",
            msg, mMessages.chatId(), msgid);
        assertAffectedRowCount(1, "updateMsgInHistory");
    }
    virtual void loadSendQueue(chatd::Chat::OutputQueue& queue)
    {
        SqliteStmt stmt(mDb, "select rowid, opcode, msgid, keyid, msg, type, "
            "ts, backrefid, backrefs, msg_cmd, key_cmd, recipients from sending where chatid=? order by rowid asc");
        stmt << mMessages.chatId();
        queue.clear();
        while(stmt.step())
        {
            uint8_t opcode = stmt.intCol(1);
            chatd::MsgCommand* msgCmd;
            if (stmt.hasBlobCol(9))
            {
                msgCmd = new chatd::MsgCommand;
                stmt.blobCol(9, *msgCmd);
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
            msg->backRefId = stmt.uint64Col(7);
            if (stmt.hasBlobCol(8))
            {
                Buffer refs;
                stmt.blobCol(8, refs);
                refs.read(0, msg->backRefs);
            }
            chatd::KeyCommand* keyCmd;
            if (stmt.hasBlobCol(10)) //key_cmd
            {
                keyCmd = new chatd::KeyCommand;
                stmt.blobCol(10, *keyCmd);
                assert(keyCmd->opcode() == chatd::OP_NEWKEY);
            }
            else
            {
                keyCmd = nullptr;
            }
            Buffer recpts;
            stmt.blobCol(11, recpts);
            queue.emplace_back(opcode, msg, msgCmd, keyCmd, recpts, stmt.intCol(0));
        }
    }
    virtual void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(mDb, "select msgid, userid, ts, type, data, idx, keyid, backrefid, backrefs from history "
            "where chatid = ?1 and idx <= ?2 order by idx desc limit ?3");
        stmt << mMessages.chatId() << idx << count;
        int i = 0;
        while(stmt.step())
        {
            i++;
            karere::Id msgid(stmt.uint64Col(0));
            karere::Id userid(stmt.uint64Col(1));
            unsigned ts = stmt.uintCol(2);
            chatd::KeyId keyid = stmt.uintCol(6);
            Buffer buf;
            stmt.blobCol(4, buf);
#ifndef NDEBUG
            auto idx = stmt.intCol(5);
            if(idx != mMessages.lownum()-1-(int)messages.size()) //we go backward in history, hence the -messages.size()
            {
                CHATD_LOG_ERROR("chatid %" PRId64 ": fetchDbHistory: History discontinuity detected: "
                    "expected idx %d, retrieved from db:%d", mMessages.chatId().val,
                    mMessages.lownum()-1-(int)messages.size(), idx);
                abort();
            }
#endif
            auto msg = new chatd::Message(msgid, userid, ts, 0, std::move(buf),
                false, keyid, (chatd::Message::Type)stmt.intCol(3));
            msg->backRefId = stmt.uint64Col(7);
            buf.clear();
            if (stmt.hasBlobCol(8))
            {
                stmt.blobCol(8, buf);
                buf.read(0, msg->backRefs);
            }
            messages.push_back(msg);
        }
    }
    virtual chatd::Idx getIdxOfMsgid(karere::Id msgid)
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
    virtual void saveItemToManualSending(const chatd::Chat::SendingItem& item, int reason)
    {
        auto& msg = *item.msg;
        sqliteQuery(mDb, "insert into manual_sending(chatid, rowid, msgid, type, "
            "ts, updated, msg, opcode, reason) values(?,?,?,?,?,?,?,?,?)",
            mMessages.chatId(), item.rowid, item.msg->id(), msg.type, msg.ts,
            msg.updated, msg, item.opcode(), reason);
    }
    virtual void loadManualSendItems(std::vector<chatd::Chat::ManualSendItem>& items)
    {
        SqliteStmt stmt(mDb, "select rowid, msgid, type, ts, updated, msg, opcode, "
            "reason from manual_sending where chatid=? order by rowid asc");
        stmt << mMessages.chatId();
        while(stmt.step())
        {
            Buffer buf;
            stmt.blobCol(5, buf);
            auto msg = new chatd::Message(stmt.uint64Col(1), 0,
                stmt.int64Col(3), stmt.intCol(4), std::move(buf), true,
                CHATD_KEYID_INVALID, (chatd::Message::Type)stmt.intCol(2));
            items.emplace_back(msg, stmt.uint64Col(0), stmt.intCol(6), stmt.intCol(7));
        }
    }
    virtual bool deleteManualSendItem(uint64_t rowid)
    {
        sqliteQuery(mDb, "delete from manual_sending where rowid = ?", rowid);
        return sqlite3_changes(mDb) != 0;
    }
    virtual void truncateHistory(karere::Id msgid)
    {
        auto idx = getIdxOfMsgid(msgid);
        if (idx == CHATD_IDX_INVALID)
            throw std::runtime_error("dbInterface::truncateHistory: msgid "+msgid.toString()+" does not exist in db");
        sqliteQuery(mDb, "delete from history where chatid = ? and idx < ?", mMessages.chatId(), idx);
    }
    virtual karere::Id getOldestMsgid()
    {
        SqliteStmt stmt(mDb, "select msgid from history where chatid = ?1 and "
            "idx = (select min(idx) from history where chatid=?1)");
        stmt << mMessages.chatId();
        stmt.stepMustHaveData(__FUNCTION__);
        return stmt.uint64Col(0);
    }
    virtual void addUser(karere::Id userid, chatd::Priv priv)
    {
        sqliteQuery(mDb, "insert ot replace into chat_peers(chatid, userid, priv) values(?,?,?)",
            mMessages.chatId(), userid, priv);
        assertAffectedRowCount(1);
    }
    virtual void removeUser(karere::Id userid)
    {
        sqliteQuery(mDb, "delete from chat_peers where chatid=? and userid=?",
            mMessages.chatId(), userid);
    }
};

#endif
