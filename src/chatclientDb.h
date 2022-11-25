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

     // DbClientInterface methods for scheduled meetings
     void insertOrUpdateSchedMeeting(const KarereScheduledMeeting& sm) override;
     void removeSchedMeetingBySchedId(const karere::Id& id) override;
     void removeSchedMeetingByChatId(const karere::Id& id) override;
     std::vector<std::unique_ptr<KarereScheduledMeeting>> getSchedMeetingsByChatId(const karere::Id& id) override;

     // DbClientInterface methods for scheduled meetings occurrences
     void insertOrUpdateSchedMeetingOcurr(const KarereScheduledMeetingOccurr& sm) override;
     void clearSchedMeetingOcurrByChatid(const karere::Id& id) override;
     std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>> getSchedMeetingsOccurByChatId(const karere::Id& id) override;

protected:
    SqliteDb& mDb;
    std::vector<std::unique_ptr<KarereScheduledMeeting>> loadSchedMeetings(const karere::Id& id);
    std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>> loadSchedMeetingsOccurr(const karere::Id& id);
};
}

#endif // CHATCLIENTSQLITEDB_H
