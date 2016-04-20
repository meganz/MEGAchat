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
    virtual uint64_t getNextRowId()
    {
        return sqlite3_last_insert_rowid(mDb);
    }
    virtual void saveMsgToSending(chatd::Message& msg, uint8_t opcode, const StaticBuffer& output)
    {
        //we don't supply a Command object for the output command parameter,
        //as it should be possible to pass an empty (NULL) buffer, which is not possible
        //with Command
        sqliteQuery(mDb, "insert into sending (chatid, opcode, ts, msgid, data, out_cmd)"
            "values(?,?,?,?,?,?,NULL)",
            mMessages.chatId(), opcode, time(NULL), msg.id(), msg, output);
    }
    virtual void addCommandBlobToSendingItem(uint64_t rowid, const Command& command)
    {
        sqliteQuery(mDb, "update sending set out_cmd=? where rowid=?", command, rowid);
        if (sqlite3_changes(mDb) != 1)
            throw std::runtime_error("addCommandBlobToSendingItem: No sending item with specified rowid found");
    }
    virtual void saveCommandToSending(const chatd::Command& cmd, uint64_t& rowid)
    {
        sqliteQuery(mDb, "insert into sending(chatid, opcode, ts, msgid, data, out_cmd)"
            "values(?,?,?,0,NULL,?)", mMessages.chatId(), cmd.opcode(), time(NULL), cmd);
        rowid = sqlite3_last_insert_rowid(mDb);
    }
    virtual void deleteItemFromSending(uint64_t rowid)
    {
        sqliteQuery(mDb, "delete from "+mSendingTblName+" where rowid = ?1", rowid);
        if (sqlite_changes(mDb) != 1)
            throw std::runtime_error("deleteItemFromSending: Unknown item rowid");
    }
    /*
    virtual void updateMsgInSending(const chatd::Message& msg)
    {
        SqliteStmt stmt(mDb, "update "+mSendingTblName+" set data = ?2, ts = ?3 where rowid = ?1");
        stmt << msg.id() << msg << msg.ts;
        stmt.step();
    }
    */
    virtual void addMsgToHistory(const chatd::Message& msg, chatd::Idx idx)
    {
        sqliteQuery(mDb, "insert or replace into history(idx, chatid, msgid,"
            "type, userid, ts, data) values(?,?,?,?,?,?,?)",
            idx, mMessages.chatId(), msg.id(), msg.type, msg.userid, msg.ts, msg);
    }
    virtual void addKey(const KeyWithId& key)
    {
        assert(key.id() != Key::kUnconfirmedId && key.id() != Key::kInvalidId);
        sqliteQuery(mDb, "insert into keys(id, data) values(?,?)", key.id(), StaticBuffer(key.data(), key.len()));
    }

    virtual void updateMsgInHistory(const chatd::Message& msg, chatd::Idx idx)
    {
        sqliteQuery(mDb, "update history set data = ? where idx = ?", msg, idx);
        if (sqlite3_changes(mDb) != 1)
            throw std::runtime_error("updateMsgInHistory: Message with specific msgid not found");
    }
    virtual void loadSendQueue(chatd::Chat::OutputQueue& queue)
    {
        SqliteStmt stmt(mDb, "select rowid, opcode, data_id, data, type, ts, out_cmd from sending"
            "where chatid=? order by rowid asc");
        stmt << mMessages.chatId();
        queue.clear();
        while(stmt.step())
        {
            uint8_t opcode = stmt.intCol(1);
            chatd::Command* cmd;
            if (stmt.hasBlobCol(6))
            {
                cmd = new chatd::Command;
                stmt.blobCol(6, *cmd);
                assert(cmd->opcode() == opcode);
            }
            else
            {
                cmd = nullptr;
            }
            void* data;
            if (stmt.hasBlobCol(3)) //data
            {
                if (opcode == NEWKEY)
                {
                    auto key = new chatd::Key(stmt.uint64Col(2), 0);
                    key->len = stmt.blobCol(3, key->data, chatd::Key::kMaxLen);
                    data = key;
                }
                else if (opcode == NEWMSG || opcode == MSGUPD || opcode == MSGUPDX)
                {
                    Buffer buf;
                    stmt.blobCol(3, buf);
                    auto msg = new chatd::Message(stmt.int64Col(2), mMessage.userId(),
                        stmt.intCol(4), time(NULL), buf, stmt.intCol(5), nullptr, true);
                    data = msg;
                }
                else
                    throw std::runtime_error("Don't know how to handle send item with opcode "+std::to_string(opcode));
            }
            auto item = new chatd::Chat::SendingItem(stmt.intCol(1), stmt.intCol(0), cmd, data);
            queue.push_back(item);
        }
    }
    virtual void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(mDb, "select msgid, userid, ts, type, data, idx, from history "
            "where chatid=?1 and idx <= ?2 order by idx desc limit ?3");
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
            assert(idx == mMessages.lownum()-1-(int)messages.size());
#endif
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
