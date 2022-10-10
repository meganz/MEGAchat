#ifndef CHATCLIENTSQLITEDB_H
#define CHATCLIENTSQLITEDB_H
#include "db.h"
#include "chatClient.h"
using namespace karere;

namespace karere
{
class ChatClientSqliteDb: public DbClientInterface
{
public:
    ChatClientSqliteDb(SqliteDb& db);
    virtual ~ChatClientSqliteDb();

     // DbClientInterface methods
     void insertOrUpdateSchedMeeting(const KarereScheduledMeeting* sm) override;

protected:
    SqliteDb& mDb;
};
}

#endif // CHATCLIENTSQLITEDB_H
