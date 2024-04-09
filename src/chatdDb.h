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
    void getHistoryInfo(chatd::ChatDbInfo& info) override
    {
        SqliteStmt stmt(mDb, "select min(idx), max(idx) from history where chatid=?1");
        stmt.bind(mChat.chatId()).step(); //will always return a row, even if table empty
        chatd::Idx minIdx = stmt.integralCol<chatd::Idx>(0);
        info.setNewestDbIdx(stmt.integralCol<int>(1));
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) //no db history
        {
            info.reset(); // reset ChatDbInfo values to invalid/default ones
            return;
        }
        info.setOldestDbIdx(minIdx);
        SqliteStmt stmt2(mDb, "select msgid from "+mHistTblName+" where chatid=?1 and idx=?2");
        stmt2 << mChat.chatId() << minIdx;
        stmt2.stepMustHaveData();
        info.setOldestDbId(stmt2.integralCol<uint64_t>(0));
        stmt2.reset().bind(2, info.getNewestDbIdx());
        stmt2.stepMustHaveData();
        info.setNewestDbId(stmt2.integralCol<uint64_t>(0));
        if (info.getNewestDbId().isNull())
        {
            assert(false);  // if there's an oldest message, there should be always a newest message, even if it's the same one
            CHATD_LOG_WARNING("Db: Newest msgid in db is null, telling chatd we don't have local history");
            info.setOldestDbId(karere::Id::null());
        }
        SqliteStmt stmt3(mDb, "select last_seen, last_recv from chats where chatid=?");
        stmt3 << mChat.chatId();
        stmt3.stepMustHaveData();
        info.setLastSeenId(stmt3.integralCol<uint64_t>(0));
        info.setLastRecvId(stmt3.integralCol<uint64_t>(1));
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

    void addMessage(const chatd::Message& msg, chatd::Idx idx, const std::string& table)
    {
#ifndef NDEBUG
        std::string checkQuery = "select min(idx), max(idx), count(*) from " + table + " where chatid = ?";
        SqliteStmt stmt(mDb, checkQuery.c_str());
        stmt << mChat.chatId();
        stmt.step();
        int low = stmt.integralCol<int>(0);
        int high = stmt.integralCol<int>(1);
        int count = stmt.integralCol<int>(2);
        if ((count > 0) && (idx != low-1) && (idx != high+1))
        {
            CHATD_LOG_ERROR("chatid %s: addMsgToHistory: %s discontinuity detected: "
                "index of added msg %s is not adjacent to neither end of db history: "
                "add idx=%d, histlow=%d, histhigh=%d, histcount= %d",
                table.c_str(), mChat.chatId().toString().c_str(), msg.id().toString().c_str(),
                idx, low, high, count);
            assert(false);
        }
#endif
        std::string query = "insert into " + table + " (idx, chatid, msgid, keyid, type, userid, ts, updated, data, backrefid, is_encrypted) " +
                                                     "values(?,?,?,?,?,?,?,?,?,?,?)";
        mDb.query(query.c_str(), idx, mChat.chatId(), msg.id(), msg.keyid,
            msg.type, msg.userid, msg.ts, msg.updated, msg, msg.backRefId, msg.isEncrypted());
    }

    void addSendingItem(chatd::Chat::SendingItem& item) override
    {
        assert(item.msg);
        uint8_t opcode = item.opcode();
        assert((opcode == chatd::OP_NEWMSG)
               || (opcode == chatd::OP_NEWNODEMSG)
               || (opcode == chatd::OP_MSGUPD)
               || (opcode == chatd::OP_MSGUPDX));

        chatd::Message* msg = item.msg;
        Buffer rcpts;
        item.recipients.save(rcpts);

        mDb.query("insert into sending (chatid, opcode, ts, msgid, msg, type, updated, "
                         "recipients, backrefid, backrefs) values(?,?,?,?,?,?,?,?,?,?)",
            (uint64_t)mChat.chatId(), opcode, msg->ts, msg->id(),
            *msg, msg->type, msg->updated, rcpts, msg->backRefId, msg->backrefBuf());

        // assign the given rowid to the SendingItem
        item.rowid = sqlite3_last_insert_rowid(mDb);
    }

    int updateSendingItemsKeyid(chatd::KeyId localkeyid, chatd::KeyId keyid) override
    {
        mDb.query("update sending set keyid = ?, key_cmd = ? where keyid = ? and chatid = ?",
                  keyid, StaticBuffer(nullptr, 0), localkeyid, mChat.chatId());

        return sqlite3_changes(mDb);
    }

    void addBlobsToSendingItem(uint64_t rowid,
        const chatd::MsgCommand* msgCmd, const chatd::KeyCommand* keyCmd, chatd::KeyId keyid) override
    {
        // possible values of `keyid`:
        // - NEWMSG/MSGUPDX: local keyxid = rowid of the KeyCmd related to this MsgCmd
        // - MSGUPD: chat keyid (already confirmed)
        mDb.query("update sending set keyid=?, msg_cmd=?, key_cmd=? where rowid=?",
                  keyid, msgCmd->msg(),
                  keyCmd ? keyCmd->keyblob() : StaticBuffer(nullptr, 0),
                  rowid);
        assertAffectedRowCount(1,"addBlobsToSendingItem");
    }

    int updateSendingItemsMsgidAndOpcode(karere::Id msgxid, karere::Id msgid) override
    {
        mDb.query(
            "update sending set opcode=?, msgid=? where chatid=? and opcode=? and msgid=?",
            chatd::OP_MSGUPD, msgid, mChat.chatId(), chatd::OP_MSGUPDX, msgxid);
        return sqlite3_changes(mDb);
    }

    void deleteSendingItem(uint64_t rowid) override
    {
        mDb.query("delete from sending where rowid = ?1", rowid);
        assertAffectedRowCount(1, "deleteSendingItem");
    }
    int updateSendingItemsContentAndDelta(const chatd::Message& msg) override
    {
        mDb.query("update sending set msg = ? where msgid = ? and chatid = ?",
                  msg, msg.id(), mChat.chatId());
        return sqlite3_changes(mDb);
    }
    void addMsgToHistory(const chatd::Message& msg, chatd::Idx idx) override
    {
        addMessage(msg, idx, "history");
    }
    void updateMsgInHistory(karere::Id msgid, const chatd::Message& msg) override
    {
        if (msg.type == chatd::Message::kMsgTruncate)
        {
            mDb.query("update history set type = ?, data = ?, ts = ?, updated = 0, userid = ?, keyid = ? where chatid = ? and msgid = ?",
                msg.type, msg, msg.ts, msg.userid, msg.keyid, mChat.chatId(), msgid);
        }
        else    // "updated" instead of "ts"
        {
            mDb.query("update history set type = ?, data = ?, updated = ?, userid = ?, is_encrypted = ? where chatid = ? and msgid = ?",
                msg.type, msg, msg.updated, msg.userid, msg.isEncrypted(), mChat.chatId(), msgid);
        }
        assertAffectedRowCount(1, "updateMsgInHistory");
    }

    void getMessageDelta(const karere::Id& msgid, uint16_t *updated) override
    {
        SqliteStmt stmt3(mDb, "select updated from history where chatid = ? and msgid = ?");
        stmt3 << mChat.chatId() << msgid;
        stmt3.stepMustHaveData();
        *updated = stmt3.integralCol<uint16_t>(0);
    }

    void getMessageUserKeyId(const karere::Id &msgid, karere::Id &userid, uint32_t &keyid) override
    {
        SqliteStmt stmt(mDb, "select userid, keyid from history where msgid = ?");
        stmt << msgid;
        stmt.stepMustHaveData("getMessageUserKeyId");
        userid = stmt.integralCol<uint64_t>(0);
        keyid = stmt.integralCol<uint32_t>(1);
    }

    void loadSendQueue(chatd::Chat::OutputQueue& queue) override
    {
        SqliteStmt stmt(mDb, "select rowid, opcode, msgid, keyid, msg, type, "
            "ts, updated, backrefid, backrefs, recipients, msg_cmd, key_cmd "
            "from sending where chatid=? order by rowid asc");
        stmt << mChat.chatId();

        // Fill the sending queue with SendingItems from DB
        queue.clear();
        while(stmt.step())
        {
            int rowid = stmt.integralCol<int>(0);
            uint8_t opcode = stmt.integralCol<uint8_t>(1);
            karere::Id msgid = stmt.integralCol<uint64_t>(2);
            karere::Id userid = mChat.client().myHandle();
            chatd::KeyId keyid = stmt.integralCol<chatd::KeyId>(3);
            unsigned char type = stmt.integralCol<unsigned char>(5);
            uint32_t ts = stmt.integralCol<uint32_t>(6);
            uint16_t updated = stmt.integralCol<uint16_t>(7);

            assert((opcode == chatd::OP_NEWMSG)
                   || (opcode == chatd::OP_NEWNODEMSG)
                   || (opcode == chatd::OP_MSGUPD)
                   || (opcode == chatd::OP_MSGUPDX));

            auto msg = new chatd::Message(msgid, userid, ts, updated, nullptr, 0, true, keyid, type);
            stmt.blobCol(4, *msg);  // set plain-text content
            msg->backRefId = stmt.integralCol<uint64_t>(8);
            if (stmt.hasBlobCol(9))
            {
                Buffer refs;
                stmt.blobCol(9, refs);
                refs.read(0, msg->backRefs);
            }

            Buffer recpts;
            stmt.blobCol(10, recpts);
            karere::SetOfIds recipients;
            recipients.load(recpts);

            // add the SendingItem to the OutputQueue
            queue.emplace_back(opcode, msg, recipients, rowid);

            // if message was already encrypted, restore the MsgCommand
            if (stmt.hasBlobCol(11))
            {
                chatd::MsgCommand *msgCmd = new chatd::MsgCommand(opcode, mChat.chatId(), userid, msgid, ts, updated, keyid);
                Buffer buf;
                stmt.blobCol(11, buf);
                msgCmd->setMsg(buf.buf(), buf.dataSize());

                queue.back().msgCmd = msgCmd;
            }

            // it message had a new key attached, restore the KeyCommand
            if (stmt.hasBlobCol(12))
            {
                assert(queue.back().msgCmd);    // a NEWKEY must always indicate there's an encrypted NEWMSG
                assert(opcode == chatd::OP_NEWMSG || opcode == chatd::OP_NEWNODEMSG);

                chatd::KeyCommand *keyCmd = new chatd::KeyCommand(mChat.chatId(), keyid);
                Buffer buf;
                stmt.blobCol(12, buf);
                keyCmd->setKeyBlobs(buf.buf(), buf.dataSize());

                queue.back().keyCmd = keyCmd;
            }
        }
    }
    void fetchDbHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages) override
    {
        loadMessages(count, idx, messages, "history");
    }

    virtual chatd::Idx getIdxOfMsgid(karere::Id msgid, const std::string &table)
    {
        std::string query = "select idx from " + table + " where chatid = ? and msgid = ?";
        SqliteStmt stmt(mDb, query.c_str());
        stmt << mChat.chatId() << msgid;
        return (stmt.step()) ? stmt.integralCol<chatd::Idx>(0) : CHATD_IDX_INVALID;
    }

    chatd::Idx getIdxOfMsgidFromHistory(const karere::Id& msgid) override
    {
        return getIdxOfMsgid(msgid, "history");
    }
    chatd::Idx getUnreadMsgCountAfterIdx(chatd::Idx idx) override
    {
        // get the unread messages count --> conditions should match the ones in Message::isValidUnread()
        std::string sql = "select count(*) from history where (chatid = ?1)"
                "and (userid != ?2)"
                "and not (updated != 0 and length(data) = 0)"
                "and (is_encrypted = ?3 or is_encrypted = ?4 or is_encrypted = ?5)"
                "and (type = ?6 or type = ?7 or type = ?8 or type = ?9 or type = ?10)";
        if (idx != CHATD_IDX_INVALID)
            sql+=" and (idx > ?11)";

        SqliteStmt stmt(mDb, sql);
        stmt << mChat.chatId() << mChat.client().myHandle()   // skip own messages
             << chatd::Message::kNotEncrypted               // include decrypted messages
             << chatd::Message::kEncryptedMalformed         // include encrypted messages due to malformed payload
             << chatd::Message::kEncryptedSignature         // include encrypted messages due to invalid signature
             << chatd::Message::kMsgNormal                  // include only known type of messages
             << chatd::Message::kMsgAttachment
             << chatd::Message::kMsgContact
             << chatd::Message::kMsgContainsMeta
             << chatd::Message::kMsgVoiceClip;
        if (idx != CHATD_IDX_INVALID)
            stmt << idx;
        stmt.stepMustHaveData("get peer msg count");
        int32_t unReadCount = stmt.integralCol<int32_t>(0);

        sql = "select data from history where (chatid = ?1)"
                "and (userid != ?2 )"
                "and (ts > ?3)"
                "and (type = ?4)";
        if (idx != CHATD_IDX_INVALID)
            sql+=" and (idx > ?5)";

        SqliteStmt stmtEndCAll(mDb, sql);
        stmtEndCAll << mChat.chatId() << mChat.client().myHandle() // skip own messages
                    << chatd::kTsMissingCallUnread // skip messages older than kTsMissingCallUnread
                    << chatd::Message::kMsgCallEnd;                // include only End call messages
        if (idx != CHATD_IDX_INVALID)
            stmtEndCAll << idx;

        while(stmtEndCAll.step())
        {
            Buffer buffer;
            stmtEndCAll.blobCol(0, buffer);
            uint8_t termCode = chatd::Message::extractTermCodeEndCall(buffer);
            if (termCode == chatd::CallDataReason::kNoAnswer || termCode == chatd::CallDataReason::kCancelled)
            {
                unReadCount ++;
            }
        }

        return unReadCount;
    }
    void saveItemToManualSending(const chatd::Chat::SendingItem& item, int reason) override
    {
        auto& msg = *item.msg;
        mDb.query("insert into manual_sending(chatid, rowid, msgid, type, "
            "ts, updated, msg, opcode, reason) values(?,?,?,?,?,?,?,?,?)",
            mChat.chatId(), item.rowid, item.msg->id(), msg.type, msg.ts,
            msg.updated, msg, item.opcode(), reason);
    }
    void loadManualSendItems(std::vector<chatd::Chat::ManualSendItem>& items) override
    {
        SqliteStmt stmt(mDb, "select rowid, msgid, type, ts, updated, msg, opcode, "
            "reason from manual_sending where chatid=? order by rowid asc");
        stmt << mChat.chatId();
        while(stmt.step())
        {
            Buffer buf;
            stmt.blobCol(5, buf);
            auto msg = new chatd::Message(stmt.integralCol<uint64_t>(1), mChat.client().myHandle(),
                stmt.integralCol<uint32_t>(3), stmt.integralCol<uint16_t>(4), std::move(buf), true,
                CHATD_KEYID_INVALID, stmt.integralCol<unsigned char>(2));
            items.emplace_back(msg,
                               stmt.integralCol<uint64_t>(0),
                               stmt.integralCol<uint8_t>(6),
                               stmt.integralCol<chatd::ManualSendReason>(7));
        }
    }
    bool deleteManualSendItem(uint64_t rowid) override
    {
        mDb.query("delete from manual_sending where rowid = ?", rowid);
        return sqlite3_changes(mDb) != 0;
    }
    void loadManualSendItem(uint64_t rowid, chatd::Chat::ManualSendItem& item) override
    {
        SqliteStmt stmt(mDb, "select msgid, type, ts, updated, msg, opcode, "
            "reason from manual_sending where chatid=? and rowid=?");
        stmt << mChat.chatId() << rowid;
        stmt.stepMustHaveData("load manual sending item");

        Buffer buf;
        stmt.blobCol(4, buf);
        auto msg = new chatd::Message(stmt.integralCol<uint64_t>(0), mChat.client().myHandle(),
                                      stmt.integralCol<uint32_t>(2), stmt.integralCol<uint16_t>(3), std::move(buf), true,
                                      CHATD_KEYID_INVALID, stmt.integralCol<unsigned char>(1));
        item.msg = msg;
        item.rowid = rowid;
        item.opcode = stmt.integralCol<uint8_t>(5);
        item.reason = stmt.integralCol<chatd::ManualSendReason>(6);
    }
    void truncateHistory(const chatd::Message& msg) override
    {
        auto idx = getIdxOfMsgidFromHistory(msg.id());
        if (idx == CHATD_IDX_INVALID)
            throw std::runtime_error("dbInterface::truncateHistory: msgid "+msg.id().toString()+" does not exist in db");
        mDb.query("delete from history where chatid = ? and idx < ?", mChat.chatId(), idx);

        cleanReactions(msg.id());
        cleanPendingReactions(msg.id());

#ifndef NDEBUG
        SqliteStmt stmt(mDb, "select type from history where chatid=? and msgid=?");
        stmt << mChat.chatId() << msg.id();
        stmt.step();
        if (stmt.integralCol<chatd::Message::Type>(0) != chatd::Message::kMsgTruncate)
            throw std::runtime_error("DbInterface::truncateHistory: Truncate message type is not 'truncate'");
#endif
    }
    chatd::Idx getOldestIdx() override
    {
        SqliteStmt stmt(mDb, "select min(idx) from history where chatid = ?");
        stmt << mChat.chatId();
        stmt.stepMustHaveData(__FUNCTION__);
        return stmt.integralCol<chatd::Idx>(0);
    }

    uint32_t getOldestMsgTs() override
    {
        SqliteStmt stmt(mDb, "select min(ts) from history where chatid = ?");
        stmt << mChat.chatId();
        stmt.stepMustHaveData(__FUNCTION__);
        return stmt.integralCol<uint32_t>(0);
    }

    void setLastSeen(const karere::Id& msgid) override
    {
        mDb.query("update chats set last_seen=? where chatid=?", msgid, mChat.chatId());
        assertAffectedRowCount(1, "setLastSeen");
    }
    void setLastReceived(const karere::Id& msgid) override
    {
        mDb.query("update chats set last_recv=? where chatid=?", msgid, mChat.chatId());
        assertAffectedRowCount(1);
    }

    void setHaveAllHistory(bool haveAllHistory) override
    {
        mDb.query(
            "insert or replace into chat_vars(chatid, name, value) "
            "values(?, 'have_all_history', ?)", mChat.chatId(), haveAllHistory ? 1 : 0);
        assertAffectedRowCount(1, "setHaveAllHistory");
    }
    bool haveAllHistory()
    {
        SqliteStmt stmt(mDb,
            "select value from chat_vars where chatid=? and name='have_all_history' and value='1'");
        stmt << mChat.chatId();
        return stmt.step();
    }

    void getLastTextMessage(chatd::Idx from, chatd::LastTextMsgState& msg, uint32_t& lastTs) override
    {
        SqliteStmt stmt(mDb,
            "select type, idx, data, msgid, userid, ts from history where chatid=?1 and "
            "(length(data) > 0 OR type = ?2) and type != ?3  and type != ?4 and (idx <= ?5)"
            "order by idx desc limit 1");
        stmt << mChat.chatId()
             << chatd::Message::kMsgTruncate
             << chatd::Message::kMsgRevokeAttachment
             << chatd::Message::kMsgInvalid     // exclude (still) encrypted messages (theorically, they should not be stored in DB)
             << from;
        if (!stmt.step())
        {

            CHATD_LOG_WARNING("chatid %s: getLastTextMessage cannot find any candidate for last-message", mChat.chatId().toString().c_str());

            msg.clear();    // any existing last-msg is now obsolete

            // reset the last-ts to the chat creation's ts
            SqliteStmt stmt(mDb, "select ts_created from chats where chatid=?");
            stmt << mChat.chatId();
            stmt.stepMustHaveData();
            lastTs = stmt.integralCol<uint32_t>(0);
            return;
        }
        Buffer buf(128);
        stmt.blobCol(2, buf);
        msg.assign(buf, stmt.integralCol<uint8_t>(0), stmt.integralCol<uint64_t>(3), stmt.integralCol<int>(1), stmt.integralCol<uint64_t>(4));
        lastTs = stmt.integralCol<uint32_t>(5);
    }

    //Insert a new chat var related to a chat. This function receives as parameters the var name and it's value
    void setChatVar(const char *name, bool value) override
    {
        mDb.query(
            "insert or replace into chat_vars(chatid, name, value) "
            "values(?, ?, ?)", mChat.chatId(), name, value ? 1 : 0);
        assertAffectedRowCount(1);
    }

    //Returns if chat var related to a chat exists
    bool chatVar(const char *name) override
    {
        SqliteStmt stmt(mDb,
            "select value from chat_vars where chatid=? and name=? and value='1'");
        stmt << mChat.chatId()
             << name;
        return stmt.step();
    }

    //Remove a chat var related to a chat
    bool removeChatVar(const char *name) override
    {
        SqliteStmt stmt(mDb,
            "delete from chat_vars where chatid = ? and name = ?");
        stmt << mChat.chatId()
             << name;
        return stmt.step();
    }

    void clearHistory() override
    {
        mDb.query("delete from history where chatid = ?", mChat.chatId());
        setHaveAllHistory(false);
    }

    void addMsgToNodeHistory(const chatd::Message& msg, chatd::Idx &idx) override
    {
        chatd::Idx idxOnDb = getIdxOfMsgid(msg.id(), "node_history");
        if (idxOnDb == CHATD_IDX_INVALID)
        {
            addMessage(msg, idx, "node_history");
            assertAffectedRowCount(1, "addMsgToNodeHistory");
        }
        else
        {
            idx = idxOnDb;
        }
    }

    void deleteMsgFromNodeHistory(const chatd::Message& msg) override
    {
        mDb.query("update node_history set data = ?, updated = ?, type = ? where chatid = ? and msgid = ?",
                  msg, msg.updated, msg.type, mChat.chatId(), msg.id());
        assertAffectedRowCount(1, "deleteMsgFromNodeHistory");
    }

    bool isValidReactedMessage(const karere::Id &msgid, chatd::Idx &idx) override
    {
        SqliteStmt stmt(mDb, "select type, userid, keyid, idx from history where msgid = ?");
        stmt << msgid;
        if (!stmt.step())
        {
            idx = CHATD_IDX_INVALID;
            return false;
        }

        idx = stmt.integralCol<int>(3);
        return !((stmt.integralCol<uint32_t>(0) >= chatd::Message::kMsgManagementLowest
                    && stmt.integralCol<uint32_t>(0) <= chatd::Message::kMsgManagementHighest)
               || (stmt.integralCol<uint64_t>(1) == karere::Id::COMMANDER() && stmt.integralCol<uint32_t>(2) == 0));
    }

    void truncateNodeHistory(const karere::Id& id) override
    {
        auto idx = getIdxOfMsgid(id, "node_history");
        mDb.query("delete from node_history where chatid = ? and idx <= ?", mChat.chatId(), idx);
    }

    void clearNodeHistory() override
    {
        mDb.query("delete from node_history where chatid = ?", mChat.chatId());
    }

    void getNodeHistoryInfo(chatd::Idx &newest, chatd::Idx &oldest) override
    {
        SqliteStmt stmt(mDb, "select min(idx), max(idx), count(*) from node_history where chatid=?1");
        stmt.bind(mChat.chatId()).step(); //will always return a row, even if table empty

        int count = stmt.integralCol<int>(2);

        oldest = count ? stmt.integralCol<int>(0) : 0;
        newest = count ? stmt.integralCol<int>(1) : -1;
    }

    void fetchDbNodeHistory(chatd::Idx idx, unsigned count, std::vector<chatd::Message*>& messages) override
    {
        loadMessages(count, idx, messages, "node_history");
    }

    chatd::Idx getIdxOfMsgidFromNodeHistory(const karere::Id& msgid) override
    {
        return getIdxOfMsgid(msgid, "node_history");
    }

    void loadMessages(int count, chatd::Idx idx, std::vector<chatd::Message*>& messages, const std::string &table)
    {
        std::string query = "select msgid, userid, ts, type, data, idx, keyid, backrefid, updated, is_encrypted from " + table +
                            " where chatid = ?1 and idx <= ?2 order by idx desc limit ?3";

        SqliteStmt stmt(mDb, query.c_str());
        stmt << mChat.chatId() << idx << count;
        while(stmt.step())
        {
            karere::Id msgid(stmt.integralCol<uint64_t>(0));
            karere::Id userid(stmt.integralCol<uint64_t>(1));
            unsigned ts = stmt.integralCol<unsigned>(2);
            chatd::KeyId keyid = stmt.integralCol<chatd::KeyId>(6);
            Buffer buf;
            stmt.blobCol(4, buf);
#ifndef NDEBUG
            auto tableIdx = stmt.integralCol<int>(5);
            if(tableIdx != idx - (int)messages.size()) //we go backward in history, hence the -messages.size()
            {
                CHATD_LOG_ERROR("chatid %s: loadMessages from table %s: History discontinuity detected: "
                    "expected idx %d, retrieved from db:%d", mChat.chatId().toString().c_str(), table.c_str(),
                    idx - (int)messages.size(), tableIdx);
                assert(false);
            }
#endif
            auto msg = new chatd::Message(msgid, userid, ts, stmt.integralCol<uint16_t>(8), std::move(buf),
                false, keyid, stmt.integralCol<unsigned char>(3));
            msg->backRefId = stmt.integralCol<uint64_t>(7);
            msg->setEncrypted(stmt.integralCol<uint8_t>(9));
            messages.push_back(msg);
        }
    }

    const std::string getReactionSn() const override
    {
        SqliteStmt stmt(mDb, "select rsn from chats where chatid = ?");
        stmt << mChat.chatId();
        stmt.stepMustHaveData(__FUNCTION__);
        return stmt.stringCol(0);
    }

    void setReactionSn(const std::string &rsn) override
    {
        mDb.query("update chats set rsn = ? where chatid = ?", rsn, mChat.chatId());
        assertAffectedRowCount(1);
    }

    void cleanReactions(const karere::Id& msgId) override
    {
        mDb.query("delete from chat_reactions where chatid = ? and msgId = ?", mChat.chatId(), msgId);
    }

    void cleanPendingReactions(const karere::Id& msgId) override
    {
        mDb.query("delete from chat_pending_reactions where chatid = ? and msgId = ?", mChat.chatId(), msgId);
    }

    void addReaction(const karere::Id& msgId, const karere::Id& userId, const std::string &reaction) override
    {
        mDb.query("insert or replace into chat_reactions(chatid, msgid, userid, reaction)"
                  "values(?,?,?,?)", mChat.chatId(), msgId, userId, reaction);
    }

    void addPendingReaction(const karere::Id& msgId, const std::string &reaction, const std::string &encReaction, uint8_t status) override
    {
        mDb.query("insert or replace into chat_pending_reactions(chatid, msgid, reaction, encReaction, status)"
                  "values(?,?,?,?,?)", mChat.chatId(), msgId, reaction, encReaction, status);
    }

    void delReaction(const karere::Id& msgId, const karere::Id& userId, const std::string &reaction) override
    {
        mDb.query("delete from chat_reactions where chatid = ? and msgid = ? and userid = ? and reaction = ?",
            mChat.chatId(), msgId, userId, reaction);
    }

    void delPendingReaction(const karere::Id& msgId, const std::string &reaction) override
    {
        mDb.query("delete from chat_pending_reactions where chatid = ? and msgid = ? and reaction = ?",
            mChat.chatId(), msgId, reaction);
    }

    void getReactions(const karere::Id& msgId, std::vector<std::pair<std::string, karere::Id>> &reactions) const override
    {
        SqliteStmt stmt(mDb, "select _rowid_, reaction, userid from chat_reactions where chatid = ? and msgid = ? ORDER BY `_rowid_` ASC");
        stmt << mChat.chatId();
        stmt << msgId;
        while (stmt.step())
        {
            reactions.emplace_back(std::pair<std::string, karere::Id>(stmt.stringCol(1), karere::Id(stmt.integralCol<uint64_t>(2))));
        }
    }

    void getPendingReactions(std::vector<chatd::Chat::PendingReaction>& reactions) const override
    {
        SqliteStmt stmt(mDb, "select _rowid_, reaction, encReaction, msgid, status from chat_pending_reactions where chatid = ? ORDER BY `_rowid_` ASC");
        stmt << mChat.chatId();
        while (stmt.step())
        {
            reactions.emplace_back(chatd::Chat::PendingReaction(stmt.stringCol(1), stmt.stringCol(2), stmt.integralCol<uint64_t>(3), stmt.integralCol<uint8_t>(4)));
        }
    }

    bool hasPendingReactions() override
    {
        SqliteStmt stmt(mDb, "select count(*) from chat_pending_reactions where chatid = ?");
        stmt << mChat.chatId();
        stmt.stepMustHaveData(__FUNCTION__);
        return stmt.integralCol<int>(0);
    }

    chatd::Idx getIdxByRetentionTime(const time_t ts) override
    {
        // Find the most recent msg affected by retention time if any
        SqliteStmt stmt(mDb, "select MAX(ts), MAX(idx) from history where chatid = ? and ts <= ?");
        stmt << mChat.chatId() << static_cast<uint32_t>(ts);
        return (stmt.step() && sqlite3_column_type(stmt, 1) != SQLITE_NULL) ? stmt.integralCol<int>(1) : CHATD_IDX_INVALID;
    }

    void retentionHistoryTruncate(const chatd::Idx idx) override
    {
        if (idx != CHATD_IDX_INVALID)
        {
            // reactions and pending reactions in DB are removed along with messages (FK delete on cascade)
            mDb.query("delete from history where chatid = ? and idx <= ?", mChat.chatId(), idx);
        }
    }
};

#endif
