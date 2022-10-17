#include "chatclientDb.h"

ChatClientSqliteDb::ChatClientSqliteDb(SqliteDb& db)
    :mDb(db)
{
}

ChatClientSqliteDb::~ChatClientSqliteDb()
{
}

void ChatClientSqliteDb::insertOrUpdateSchedMeeting(const KarereScheduledMeeting* sm)
{
    if (sm->rules())
    {
        Buffer rulesBuf;
        if (!sm->rules()->serialize(rulesBuf))
        {
            assert(false);
            KR_LOG_ERROR("Error serializing a scheduled meeting");
            return;
        }

        mDb.query("insert or replace into scheduledMeetings(schedmeetingid, chatid, organizerid, parentid, timezone, start_date_time, end_date_time, "
              "title, description, attributes, overrides, cancelled, flags, rules)"
              "values(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
              sm->callid(), sm->chatid(), sm->organizerUserid(), sm->parentCallid(), sm->timezone(), sm->startDateTime(), sm->endDateTime(),
              sm->title(), sm->description(), sm->attributes(), sm->overrides(), sm->cancelled(), sm->flags()->getNumericValue(), rulesBuf);
    }
    else
    {
        mDb.query("insert or replace into scheduledMeetings(schedmeetingid, chatid, organizerid, parentid, timezone, start_date_time, end_date_time, "
              "title, description, attributes, overrides, cancelled, flags)"
              "values(?,?,?,?,?,?,?,?,?,?,?,?,?)",
              sm->callid(), sm->chatid(), sm->organizerUserid(), sm->parentCallid(), sm->timezone(), sm->startDateTime(), sm->endDateTime(),
              sm->title(), sm->description(), sm->attributes(), sm->overrides(), sm->cancelled(), sm->flags()->getNumericValue());
    }
}

void ChatClientSqliteDb::removeSchedMeetingByChatId(karere::Id id)
{
    mDb.query("delete from scheduledMeetings where chatid = ?", id);
}

void ChatClientSqliteDb::removeSchedMeetingBySchedId(karere::Id id)
{
    mDb.query("delete from scheduledMeetings where schedmeetingid = ?", id);
}

std::vector<std::unique_ptr<KarereScheduledMeeting>> ChatClientSqliteDb::getSchedMeetingsByChatId(karere::Id id)
{
    return loadSchedMeetings(id, false /*loadingOccurr*/);
}

void ChatClientSqliteDb::insertOrUpdateSchedMeetingOcurr(const KarereScheduledMeeting* sm)
{
    mDb.query("insert or replace into scheduledMeetingsOccurr(schedmeetingid, chatid, organizerid, parentid, timezone, start_date_time, end_date_time, "
          "title, description, attributes, overrides, cancelled, flags)"
          "values(?,?,?,?,?,?,?,?,?,?,?,?,?)",
          sm->callid(), sm->chatid(), sm->organizerUserid(), sm->parentCallid(), sm->timezone(), sm->startDateTime(), sm->endDateTime(),
          sm->title(), sm->description(), sm->attributes(), sm->overrides(), sm->cancelled(), sm->flags()->getNumericValue());
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
    std::string query = "select schedmeetingid, chatid, organizerid, parentid, timezone, start_date_time, end_date_time, title, description, attributes, overrides, cancelled, ";

    loadingOccurr
            ? query.append("flags from scheduledMeetingsOccurr where chatid = ?")   // load scheduled meeting occurrences
            : query.append("flags, rules from scheduledMeetings where chatid = ?"); // load scheduled meeting

    SqliteStmt stmt(mDb, query.c_str());
    stmt << id;

    while (stmt.step())
    {
       karere::Id schedmeetingid = stmt.int64Col(0) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(0));
       karere::Id chatid = stmt.int64Col(1) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(1));
       karere::Id organizerid = stmt.int64Col(2) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(2));
       karere::Id parentid = stmt.int64Col(3) == -1 ? karere::Id::inval().val : static_cast<uint64_t>(stmt.int64Col(3));
       std::string timezone(stmt.stringCol(4));
       std::string start_date_time(stmt.stringCol(5));
       std::string end_date_time(stmt.stringCol(6));
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

       KarereScheduledMeeting* aux = new KarereScheduledMeeting(chatid, organizerid, timezone.c_str(), start_date_time.c_str(), end_date_time.c_str(), title.c_str(),
                                                                description.c_str(), schedmeetingid,
                                                                parentid, cancelled, attributes.c_str(),
                                                                overrides.c_str(), flags.get(), rules.get());
       v.emplace_back(std::move(aux));
    }

    return v;
}
