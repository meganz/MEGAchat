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
                  static_cast<int64_t>(sm.flags()->getNumericValue()),
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
                  static_cast<int64_t>(sm.flags()->getNumericValue()));
    }
}

void ChatClientSqliteDb::removeSchedMeetingByChatId(const karere::Id& id)
{
    mDb.query("delete from scheduledMeetings where chatid = ?", id);
}

void ChatClientSqliteDb::removeSchedMeetingBySchedId(const karere::Id& id)
{
    mDb.query("delete from scheduledMeetings where schedid = ?", id);
}

std::vector<std::unique_ptr<KarereScheduledMeeting>> ChatClientSqliteDb::getSchedMeetingsByChatId(const karere::Id& id)
{
    return loadSchedMeetings(id);
}

void ChatClientSqliteDb::insertOrUpdateSchedMeetingOcurr(const KarereScheduledMeetingOccurr& sm)
{
    mDb.query("insert or replace into scheduledMeetingsOccurr(schedid, startdatetime, enddatetime) values(?,?,?)",
              sm.schedId(),
              sm.startDateTime().size() ? sm.startDateTime().c_str() : nullptr,
              sm.endDateTime().size() ? sm.endDateTime().c_str() : nullptr);

}

void ChatClientSqliteDb::clearSchedMeetingOcurrByChatid(const karere::Id& id)
{
    mDb.query("delete from scheduledMeetingsOccurr where schedid IN (select schedid from scheduledMeetings where chatid = ?)", id);
}

std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>> ChatClientSqliteDb::getSchedMeetingsOccurByChatId(const karere::Id& id)
{
    return loadSchedMeetingsOccurr(id);
}

std::vector<std::unique_ptr<KarereScheduledMeeting>> ChatClientSqliteDb::loadSchedMeetings(const karere::Id& id)
{
    SqliteStmt stmt(mDb, "select schedid, chatid, organizerid, parentschedid, timezone, startdatetime, enddatetime, title, description, attributes, overrides, cancelled, "
                         "flags, rules from scheduledMeetings where chatid = ?");
    stmt << id;

    std::vector<std::unique_ptr<KarereScheduledMeeting>> v;
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
       if (sqlite3_column_type(stmt, 13) != SQLITE_NULL)
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

std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>> ChatClientSqliteDb::loadSchedMeetingsOccurr(const karere::Id& id)
{
    std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>> v;
    std::string query = "select scheduledMeetings.chatid, scheduledMeetings.timezone, scheduledMeetings.cancelled, scheduledMeetingsOccurr.schedid, scheduledMeetingsOccurr.startdatetime, scheduledMeetingsOccurr.enddatetime "
                        "FROM scheduledMeetings "
                        "INNER JOIN scheduledMeetingsOccurr ON scheduledMeetings.schedid=scheduledMeetingsOccurr.schedid where scheduledMeetings.chatid = ?";

    SqliteStmt stmt(mDb, query.c_str());
    stmt << id;

    while (stmt.step())
    {
       std::string timeZone(stmt.stringCol(1));
       int cancelled = stmt.intCol(2);
       karere::Id schedId = stmt.int64Col(3) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(3));
       std::string startDateTime(stmt.stringCol(4));
       std::string endDateTime(stmt.stringCol(5));

       KarereScheduledMeetingOccurr* aux = new KarereScheduledMeetingOccurr(schedId, timeZone, startDateTime, endDateTime, cancelled);
       v.emplace_back(std::move(aux));
    }

    return v;
}
