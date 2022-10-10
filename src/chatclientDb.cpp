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
}
