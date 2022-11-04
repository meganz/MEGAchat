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
     void removeSchedMeetingBySchedId(karere::Id id) override;
     void removeSchedMeetingByChatId(karere::Id id) override;
     std::vector<std::unique_ptr<KarereScheduledMeeting>> getSchedMeetingsByChatId(karere::Id id) override;

     // DbClientInterface methods for scheduled meetings occurrences
     void insertOrUpdateSchedMeetingOcurr(const KarereScheduledMeeting& sm) override;
     void clearSchedMeetingOcurrByChatid(karere::Id id) override;
     std::vector<std::unique_ptr<KarereScheduledMeeting>> getSchedMeetingsOccurByChatId(karere::Id id) override;

protected:
    SqliteDb& mDb;
    std::vector<std::unique_ptr<KarereScheduledMeeting>> loadSchedMeetings(const Id &id, bool loadingOccurr);
};
}

#endif // CHATCLIENTSQLITEDB_H
