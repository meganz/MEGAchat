#include "chatclientDb.h"

ChatClientSqliteDb::ChatClientSqliteDb(SqliteDb& db)
    :mDb(db)
{
}

ChatClientSqliteDb::~ChatClientSqliteDb()
{
}

void ChatClientSqliteDb::insertOrUpdateSchedMeeting(const KarereScheduledMeeting& sm)
{
    if (sm.rules())
    {
        Buffer rulesBuf;
        if (!sm.rules()->serialize(rulesBuf))
        {
            assert(false);
            KR_LOG_ERROR("Error serializing a scheduled meeting");
            return;
        }

        // JCHECK remove if and do this  rulesBuf ? rulesBuf : nullptr);


        mDb.query("insert or replace into scheduledMeetings(schedid, chatid, organizerid, parentschedid, timezone, startdatetime, enddatetime, "
              "title, description, attributes, overrides, cancelled, flags, rules)"
              "values(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                  sm.schedId(),
                  sm.chatid(),
                  sm.organizerUserid(),
                  sm.parentSchedId(),
                  sm.timezone().size() ? sm.timezone().c_str() : nullptr,
                  sm.startDateTime().size() ? sm.startDateTime().c_str() : nullptr,
                  sm.endDateTime().size() ? sm.endDateTime().c_str() : nullptr,
                  sm.title().size() ? sm.title().c_str() : nullptr,
                  sm.description().size() ? sm.description().c_str() : nullptr,
                  sm.attributes().size() ? sm.attributes().c_str() : nullptr,
                  sm.overrides().size() ? sm.overrides().c_str() : nullptr,
                  sm.cancelled(),
                  sm.flags()->getNumericValue(),
                  rulesBuf);
    }
    else
    {
        mDb.query("insert or replace into scheduledMeetings(schedid, chatid, organizerid, parentschedid, timezone, startdatetime, enddatetime, "
              "title, description, attributes, overrides, cancelled, flags)"
              "values(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                  sm.schedId(),
                  sm.chatid(),
                  sm.organizerUserid(),
                  sm.parentSchedId(),
                  sm.timezone().size() ? sm.timezone().c_str() : nullptr,
                  sm.startDateTime().size() ? sm.startDateTime().c_str() : nullptr,
                  sm.endDateTime().size() ? sm.endDateTime().c_str() : nullptr,
                  sm.title().size() ? sm.title().c_str() : nullptr,
                  sm.description().size() ? sm.description().c_str() : nullptr,
                  sm.attributes().size() ? sm.attributes().c_str() : nullptr,
                  sm.overrides().size() ? sm.overrides().c_str() : nullptr,
                  sm.cancelled(),
                  sm.flags()->getNumericValue());
    }
}

void ChatClientSqliteDb::removeSchedMeetingByChatId(karere::Id id)
{
    mDb.query("delete from scheduledMeetings where chatid = ?", id);
}

void ChatClientSqliteDb::removeSchedMeetingBySchedId(karere::Id id)
{
    mDb.query("delete from scheduledMeetings where schedid = ?", id);
}

std::vector<std::unique_ptr<KarereScheduledMeeting>> ChatClientSqliteDb::getSchedMeetingsByChatId(karere::Id id)
{
    return loadSchedMeetings(id, false /*loadingOccurr*/);
}

void ChatClientSqliteDb::insertOrUpdateSchedMeetingOcurr(const KarereScheduledMeeting& sm)
{
    mDb.query("insert or replace into scheduledMeetingsOccurr(schedid, chatid, organizerid, parentschedid, timezone, startdatetime, enddatetime, "
          "title, description, attributes, overrides, cancelled, flags)"
          "values(?,?,?,?,?,?,?,?,?,?,?,?,?)",
              sm.schedId(),
              sm.chatid(),
              sm.organizerUserid(),
              sm.parentSchedId(),
              sm.timezone().size() ? sm.timezone().c_str() : nullptr,
              sm.startDateTime().size() ? sm.startDateTime().c_str() : nullptr,
              sm.endDateTime().size() ? sm.endDateTime().c_str() : nullptr,
              sm.title().size() ? sm.title().c_str() : nullptr,
              sm.description().size() ? sm.description().c_str() : nullptr,
              sm.attributes().size() ? sm.attributes().c_str() : nullptr,
              sm.overrides().size() ? sm.overrides().c_str() : nullptr,
              sm.cancelled(),
}

void ChatClientSqliteDb::clearSchedMeetingOcurrByChatid(karere::Id id)
{
    mDb.query("delete from scheduledMeetingsOccurr where chatid = ?", id);
}

std::vector<std::unique_ptr<KarereScheduledMeeting>> ChatClientSqliteDb::getSchedMeetingsOccurByChatId(karere::Id id)
{
    return loadSchedMeetings(id, true /*loadingOccurr*/);
}

std::vector<std::unique_ptr<KarereScheduledMeeting>> ChatClientSqliteDb::loadSchedMeetings(const karere::Id& id, bool loadingOccurr)
{
    std::vector<std::unique_ptr<KarereScheduledMeeting>> v;
    std::string query = "select schedid, chatid, organizerid, parentschedid, timezone, startdatetime, enddatetime, title, description, attributes, overrides, cancelled, ";

    loadingOccurr
            ? query.append("flags from scheduledMeetingsOccurr where chatid = ?")   // load scheduled meeting occurrences
            : query.append("flags, rules from scheduledMeetings where chatid = ?"); // load scheduled meeting

    SqliteStmt stmt(mDb, query.c_str());
    stmt << id;

    while (stmt.step())
    {
       karere::Id schedId = stmt.int64Col(0) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(0));
       karere::Id chatid = stmt.int64Col(1) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(1));
       karere::Id organizerid = stmt.int64Col(2) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(2));
       karere::Id parentSchedid = stmt.int64Col(3) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(3));
       std::string timezone(stmt.stringCol(4));
       std::string startDateTime(stmt.stringCol(5));
       std::string endDateTime(stmt.stringCol(6));
       std::string title(stmt.stringCol(7));
       std::string description(stmt.stringCol(8));
       std::string attributes = stmt.stringCol(9);
       std::string overrides  = stmt.stringCol(10);
       int cancelled = stmt.intCol(11);
       std::unique_ptr <KarereScheduledFlags> flags(new KarereScheduledFlags(static_cast<unsigned long>(stmt.intCol(12))));
       std::unique_ptr <KarereScheduledRules> rules;
       if (!loadingOccurr && sqlite3_column_type(stmt, 13) != SQLITE_NULL)
       {
           Buffer buf;
           stmt.blobCol(13, buf);
           rules.reset(KarereScheduledRules::unserialize(buf));
       }

       KarereScheduledMeeting* aux = new KarereScheduledMeeting(chatid, organizerid, timezone, startDateTime, endDateTime, title,
                                                                description, schedId, parentSchedid, cancelled, attributes, overrides,
                                                                flags.get(), rules.get());
       v.emplace_back(std::move(aux));
    }

    return v;
}
