#ifndef CHATD_DB_H
#define CHATD_DB_H

#include "db.h"
#include "chatd.h"
//extern sqlite3* db;

class ChatdSqliteDb: public chatd::DbInterface
{
protected:
    SqliteDb& mDb;
    chatd::Chat& mChat;
    std::string mSendingTblName;
    std::string mHistTblName;
public:
    ChatdSqliteDb(chatd::Chat& chat, SqliteDb& db, const std::string& sendingTblName="sending", const std::string& histTblName="history")
        :mDb(db), mChat(chat), mSendingTblName(sendingTblName), mHistTblName(histTblName){}
    virtual void getHistoryInfo(chatd::ChatDbInfo& info)
    {
        SqliteStmt stmt(mDb, "select min(idx), max(idx) from history where chatid=?1");
        stmt.bind(mChat.chatId()).step(); //will always return a row, even if table empty
        auto minIdx = stmt.intCol(0); //WARNING: the chatd implementation uses uint32_t values for idx.
        info.newestDbIdx = stmt.intCol(1);
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) //no db history
        {
            memset(&info, 0, sizeof(info)); //actually need to zero only oldestDbId
            return;
        }
        SqliteStmt stmt2(mDb, "select msgid from "+mHistTblName+" where chatid=?1 and idx=?2");
        stmt2 << mChat.chatId() << minIdx;
        stmt2.stepMustHaveData();
        info.oldestDbId = stmt2.uint64Col(0);
        stmt2.reset().bind(2, info.newestDbIdx);
        stmt2.stepMustHaveData();
        info.newestDbId = stmt2.uint64Col(0);
        if (!info.newestDbId)
        {
            assert(false);  // if there's an oldest message, there should be always a newest message, even if it's the same one
            CHATD_LOG_WARNING("Db: Newest msgid in db is null, telling chatd we don't have local history");
            info.oldestDbId = 0;
        }
        SqliteStmt stmt3(mDb, "select last_seen, last_recv from chats where chatid=?");
        stmt3 << mChat.chatId();
        stmt3.stepMustHaveData();
        info.lastSeenId = stmt3.uint64Col(0);
        info.lastRecvId = stmt3.uint64Col(1);
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
        mDb.query("insert into sending (chatid, opcode, ts, msgid, msg, type, updated, "
                         "recipients, backrefid, backrefs) values(?,?,?,?,?,?,?,?,?,?)",
            (uint64_t)mChat.chatId(), item.opcode(), msg->ts, msg->id(),
            *msg, msg->type, msg->updated, rcpts, msg->backRefId, msg->backrefBuf());
        item.rowid = sqlite3_last_insert_rowid(mDb);
    }
    virtual void updateMsgInSending(const chatd::Chat::SendingItem& item)
    {
        assert(item.msg);
        mDb.query("update sending set msg = ?, updated = ? where rowid = ?",
            *item.msg, item.msg->updated, item.rowid);
        assertAffectedRowCount(1, "updateMsgInSending");
    }
    virtual void confirmKeyOfSendingItem(uint64_t rowid, chatd::KeyId keyid)
    {
        mDb.query("update sending set keyid = ? where rowid = ?",
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
        mDb.query("update sending set msg_cmd=?, key_cmd=? where rowid=?",
            msgCmd?static_cast<StaticBuffer>(*msgCmd):StaticBuffer(nullptr, 0),
            keyCmd?static_cast<StaticBuffer>(*keyCmd):StaticBuffer(nullptr, 0), rowid);
        assertAffectedRowCount(1,"addCommandBlobToSendingItem");
    }
    virtual void sendingItemMsgupdxToMsgupd(const chatd::Chat::SendingItem& item, karere::Id msgid)
    {
        assert(item.opcode() == chatd::OP_MSGUPDX);
        mDb.query(
            "update sending set opcode=?, msgid=? where chatid=? and rowid=? and opcode=? and msgid=?",
            chatd::OP_MSGUPD, msgid, mChat.chatId(), item.rowid, chatd::OP_MSGUPDX, item.msg->id());
        assertAffectedRowCount(1, "updateSendingItemMsgidAndOpcode");
    }
    virtual void deleteItemFromSending(uint64_t rowid)
    {
        mDb.query("delete from sending where rowid = ?1", rowid);
        assertAffectedRowCount(1, "deleteItemFromSending");
    }
    virtual void updateMsgPlaintextInSending(uint64_t rowid, const StaticBuffer& data)
    {
        mDb.query("update sending set msg = ? where rowid = ?", data, rowid);
        assertAffectedRowCount(1, "updateMsgPlaintextInSending");
    }
    virtual void updateMsgKeyIdInSending(uint64_t rowid, chatd::KeyId keyid)
    {
        mDb.query("update sending set keyid = ? where rowid = ?", keyid, rowid);
        assertAffectedRowCount(1, "updateMsgKeyIdInSending");
    }
    virtual void addMsgToHistory(const chatd::Message& msg, chatd::Idx idx)
    {
#if 1
        SqliteStmt stmt(mDb, "select min(idx), max(idx), count(*) from history where chatid = ?");
        stmt << mChat.chatId();
        stmt.step();
        int low = stmt.intCol(0);
        int high = stmt.intCol(1);
        int count = stmt.intCol(2);
        if ((count > 0) && (idx != low-1) && (idx != high+1))
        {
            CHATD_LOG_ERROR("chatid %s: addMsgToHistory: history discontinuity detected: "
                "index of added msg %s is not adjacent to neither end of db history: "
                "add idx=%d, histlow=%d, histhigh=%d, histcount= %d, fwdStart=%d, lownum=%d, highnum=%d",
                mChat.chatId().toString().c_str(), msg.id().toString().c_str(),
                idx, low, high, count, mChat.forwardStart(), mChat.lownum(), mChat.highnum());
            assert(false);
        }
#endif
        mDb.query("insert into history"
            "(idx, chatid, msgid, keyid, type, userid, ts, updated, data, backrefid, is_encrypted) "
            "values(?,?,?,?,?,?,?,?,?,?,?)", idx, mChat.chatId(), msg.id(), msg.keyid,
            msg.type, msg.userid, msg.ts, msg.updated, msg, msg.backRefId, msg.isEncrypted());
    }
    virtual void updateMsgInHistory(karere::Id msgid, const chatd::Message& msg)
    {
        if (msg.type == chatd::Message::kMsgTruncate)
        {
            mDb.query("update history set type = ?, data = ?, ts = ?, userid = ? where chatid = ? and msgid = ?",
                msg.type, msg, msg.ts, msg.userid, mChat.chatId(), msgid);
        }
        else    // "updated" instead of "ts"
        {
            mDb.query("update history set type = ?, data = ?, updated = ?, userid = ?, is_encrypted = ? where chatid = ? and msgid = ?",
                msg.type, msg, msg.updated, msg.userid, msg.isEncrypted(), mChat.chatId(), msgid);
        }
        assertAffectedRowCount(1, "updateMsgInHistory");
    }

    virtual void getMessageDelta(karere::Id msgid, uint16_t *updated)
    {
        SqliteStmt stmt3(mDb, "select updated from history where chatid = ? and msgid = ?");
        stmt3 << mChat.chatId() << msgid;
        stmt3.stepMustHaveData();
        *updated = stmt3.intCol(0);
    }

    virtual void loadSendQueue(chatd::Chat::OutputQueue& queue)
    {
        SqliteStmt stmt(mDb, "select rowid, opcode, msgid, keyid, msg, type, "
            "ts, updated, backrefid, backrefs, recipients from sending where chatid=? order by rowid asc");
        stmt << mChat.chatId();
        queue.clear();
        while(stmt.step())
        {
            uint8_t opcode = stmt.intCol(1);

            assert((opcode == chatd::OP_NEWMSG) || (opcode == chatd::OP_MSGUPD)
                   || (opcode == chatd::OP_MSGUPDX));

            auto msg = new chatd::Message(stmt.int64Col(2), mChat.client().userId(),
                    stmt.intCol(6), stmt.intCol(7), nullptr, 0, true, (chatd::KeyId)stmt.intCol(3),
                    (unsigned char)stmt.intCol(5));
            stmt.blobCol(4, *msg);
            msg->backRefId = stmt.uint64Col(8);
            if (stmt.hasBlobCol(9))
            {
                Buffer refs;
                stmt.blobCol(9, refs);
                refs.read(0, msg->backRefs);
            }
            Buffer recpts;
            stmt.blobCol(10, recpts);
            queue.emplace_back(opcode, msg, recpts, stmt.intCol(0));
        }
    }
    virtual void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages)
    {
        SqliteStmt stmt(mDb, "select msgid, userid, ts, type, data, idx, keyid, backrefid, updated, is_encrypted from history "
            "where chatid = ?1 and idx <= ?2 order by idx desc limit ?3");
        stmt << mChat.chatId() << idx << count;
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
            if(idx != mChat.lownum()-1-(int)messages.size()) //we go backward in history, hence the -messages.size()
            {
                CHATD_LOG_ERROR("chatid %s: fetchDbHistory: History discontinuity detected: "
                    "expected idx %d, retrieved from db:%d", mChat.chatId().toString().c_str(),
                    mChat.lownum()-1-(int)messages.size(), idx);
                assert(false);
            }
#endif
            auto msg = new chatd::Message(msgid, userid, ts, stmt.intCol(8), std::move(buf),
                false, keyid, (unsigned char)stmt.intCol(3));
            msg->backRefId = stmt.uint64Col(7);
            msg->setEncrypted(stmt.intCol(9));
            messages.push_back(msg);
        }
    }
    virtual chatd::Idx getIdxOfMsgid(karere::Id msgid)
    {
        SqliteStmt stmt(mDb, "select idx from history where chatid = ? and msgid = ?");
        stmt << mChat.chatId() << msgid;
        return (stmt.step()) ? stmt.int64Col(0) : CHATD_IDX_INVALID;
    }
    virtual chatd::Idx getUnreadMsgCountAfterIdx(chatd::Idx idx)
    {
        // get the unread messages count --> conditions should match the ones in Message::isValidUnread()
        std::string sql = "select count(*) from history where (chatid = ?1)"
                "and (userid != ?2)"
                "and not (updated != 0 and length(data) = 0)"
                "and (is_encrypted = ?3 or is_encrypted = ?4 or is_encrypted = ?5)"
                "and (type = ?6 or type = ?7 or type = ?8 or type = ?9)";
        if (idx != CHATD_IDX_INVALID)
            sql+=" and (idx > ?)";

        SqliteStmt stmt(mDb, sql);
        stmt << mChat.chatId() << mChat.client().userId()   // skip own messages
             << chatd::Message::kNotEncrypted               // include decrypted messages
             << chatd::Message::kEncryptedMalformed         // include encrypted messages due to malformed payload
             << chatd::Message::kEncryptedSignature         // include encrypted messages due to invalid signature
             << chatd::Message::kMsgNormal                  // include only known type of messages
             << chatd::Message::kMsgAttachment
             << chatd::Message::kMsgContact
             << chatd::Message::kMsgContainsMeta;
        if (idx != CHATD_IDX_INVALID)
            stmt << idx;
        stmt.stepMustHaveData("get peer msg count");
        return stmt.intCol(0);
    }
    virtual void saveItemToManualSending(const chatd::Chat::SendingItem& item, int reason)
    {
        auto& msg = *item.msg;
        mDb.query("insert into manual_sending(chatid, rowid, msgid, type, "
            "ts, updated, msg, opcode, reason) values(?,?,?,?,?,?,?,?,?)",
            mChat.chatId(), item.rowid, item.msg->id(), msg.type, msg.ts,
            msg.updated, msg, item.opcode(), reason);
    }
    virtual void loadManualSendItems(std::vector<chatd::Chat::ManualSendItem>& items)
    {
        SqliteStmt stmt(mDb, "select rowid, msgid, type, ts, updated, msg, opcode, "
            "reason from manual_sending where chatid=? order by rowid asc");
        stmt << mChat.chatId();
        while(stmt.step())
        {
            Buffer buf;
            stmt.blobCol(5, buf);
            auto msg = new chatd::Message(stmt.uint64Col(1), mChat.client().userId(),
                stmt.int64Col(3), stmt.intCol(4), std::move(buf), true,
                CHATD_KEYID_INVALID, (unsigned char)stmt.intCol(2));
            items.emplace_back(msg, stmt.uint64Col(0), stmt.intCol(6), (chatd::ManualSendReason)stmt.intCol(7));
        }
    }
    virtual bool deleteManualSendItem(uint64_t rowid)
    {
        mDb.query("delete from manual_sending where rowid = ?", rowid);
        return sqlite3_changes(mDb) != 0;
    }
    virtual void loadManualSendItem(uint64_t rowid, chatd::Chat::ManualSendItem& item)
    {
        SqliteStmt stmt(mDb, "select msgid, type, ts, updated, msg, opcode, "
            "reason from manual_sending where chatid=? and rowid=?");
        stmt << mChat.chatId() << rowid;
        stmt.stepMustHaveData("load manual sending item");

        Buffer buf;
        stmt.blobCol(4, buf);
        auto msg = new chatd::Message(stmt.uint64Col(0), mChat.client().userId(),
                                      stmt.int64Col(2), stmt.intCol(3), std::move(buf), true,
                                      CHATD_KEYID_INVALID, (unsigned char)stmt.intCol(1));
        item.msg = msg;
        item.rowid = rowid;
        item.opcode = stmt.intCol(5);
        item.reason = (chatd::ManualSendReason)stmt.intCol(6);
    }
    virtual void truncateHistory(const chatd::Message& msg)
    {
        auto idx = getIdxOfMsgid(msg.id());
        if (idx == CHATD_IDX_INVALID)
            throw std::runtime_error("dbInterface::truncateHistory: msgid "+msg.id().toString()+" does not exist in db");
        mDb.query("delete from history where chatid = ? and idx < ?", mChat.chatId(), idx);
#if 1
        SqliteStmt stmt(mDb, "select type from history where chatid=? and msgid=?");
        stmt << mChat.chatId() << msg.id();
        stmt.step();
        if (stmt.intCol(0) != chatd::Message::kMsgTruncate)
            throw std::runtime_error("DbInterface::truncateHistory: Truncate message type is not 'truncate'");
#endif
    }
    virtual chatd::Idx getOldestIdx()
    {
        SqliteStmt stmt(mDb, "select min(idx) from history where chatid = ?");
        stmt << mChat.chatId();
        stmt.stepMustHaveData(__FUNCTION__);
        return stmt.uint64Col(0);
    }
    virtual void setLastSeen(karere::Id msgid)
    {
        mDb.query("update chats set last_seen=? where chatid=?", msgid, mChat.chatId());
        assertAffectedRowCount(1);
    }
    virtual void setLastReceived(karere::Id msgid)
    {
        mDb.query("update chats set last_recv=? where chatid=?", msgid, mChat.chatId());
        assertAffectedRowCount(1);
    }
    virtual void setHaveAllHistory(bool haveAllHistory)
    {
        mDb.query(
            "insert or replace into chat_vars(chatid, name, value) "
            "values(?, 'have_all_history', ?)", mChat.chatId(), haveAllHistory ? 1 : 0);
        assertAffectedRowCount(1);
    }
    virtual bool haveAllHistory()
    {
        SqliteStmt stmt(mDb,
            "select value from chat_vars where chatid=? and name='have_all_history' and value='1'");
        stmt << mChat.chatId();
        return stmt.step();
    }
    virtual void getLastTextMessage(chatd::Idx from, chatd::LastTextMsgState& msg)
    {
        SqliteStmt stmt(mDb,
            "select type, idx, data, msgid, userid from history where chatid=?1 and "
            "(length(data) > 0 OR type = ?2) and type != ?3  and type != ?4 and (idx <= ?5)"
            "order by idx desc limit 1");
        stmt << mChat.chatId()
             << chatd::Message::kMsgTruncate
             << chatd::Message::kMsgRevokeAttachment
             << chatd::Message::kMsgInvalid     // exclude (still) encrypted messages (theorically, they should not be stored in DB)
             << from;
        if (!stmt.step())
        {
            msg.clear();
            return;
        }
        Buffer buf(128);
        stmt.blobCol(2, buf);
        msg.assign(buf, stmt.intCol(0), stmt.uint64Col(3), stmt.intCol(1), stmt.uint64Col(4));
    }

    virtual void clearHistory()
    {
        mDb.query("delete from history where chatid = ?", mChat.chatId());
        setHaveAllHistory(false);
    }
};

#endif
