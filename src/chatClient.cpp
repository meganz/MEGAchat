#include "chatClient.h"
#ifdef _WIN32
#include <sys/timeb.h>

#include <direct.h>
#include <winsock2.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef KARERE_DISABLE_WEBRTC
    #include "rtcCrypto.h"
#endif
#include "base/services.h"
#include "base64url.h"
#include "chatclientDb.h"
#include "sdkApi.h"
#include "strongvelope/strongvelope.h"

#include <mega/tlv.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <asyncTools.h>
#include <autoHandle.h>
#include <buffer.h>
#include <chatd.h>
#include <chatdDb.h>
#include <codecvt> //for nonWhitespaceStr()
#include <db.h>
#include <locale>
#include <megaapi_impl.h>
#include <memory>

#ifdef __ANDROID__
    #include <sys/system_properties.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #include <resolv.h>
    #endif
#endif

#define _QUICK_LOGIN_NO_RTC
using namespace promise;


namespace karere
{
class ChatClientSqliteDb;

template <class T, class F>
void callAfterInit(T* self, F&& func, void* ctx);

std::string encodeFirstName(const std::string& first);

bool Client::anonymousMode() const
{
    return (mInitState == kInitAnonymousMode);
}

bool Client::isInBackground() const
{
    return mIsInBackground;
}

/* Warning - the database is not initialzed at construction, but only after
 * init() is called. Therefore, no code in this constructor should access or
 * depend on the database
 */
Client::Client(mega::MegaApi &sdk, WebsocketsIO *websocketsIO, IApp &aApp,
#ifndef KARERE_DISABLE_WEBRTC
               rtcModule::CallHandler &callHandler,
#endif
               ScheduledMeetingHandler& scheduledMeetingHandler,
               const std::string &appDir, uint8_t caps, void *ctx)
    : mAppDir(appDir),
      websocketIO(websocketsIO),
      appCtx(ctx),
      api(sdk, ctx),
      app(aApp),
      db(app),
      mDnsCache(db, chatd::Client::chatdVersion),
      mScheduledMeetingHandler(scheduledMeetingHandler),
#ifndef KARERE_DISABLE_WEBRTC
      mCallHandler(callHandler),
#endif
      mContactList(new ContactList(*this)),
      chats(new ChatRoomList(*this)),
      mPresencedClient(&api, this, *this, caps)
{
#ifndef KARERE_DISABLE_WEBRTC
// Create the rtc module
    rtc.reset(rtcModule::createRtcModule(api, mCallHandler, mDnsCache, *websocketIO, appCtx,
                                         new rtcModule::RtcCryptoMeetings(*this)));
#endif
    mClientDbInterface = std::unique_ptr<ChatClientSqliteDb>(new ChatClientSqliteDb(db));
}


KARERE_EXPORT const std::string& createAppDir(const char* dirname, const char *envVarName)
{
    static std::string path;
    if (!path.empty())
        return path;
    const char* dir = getenv(envVarName);
    if (dir)
    {
        path = dir;
    }
    else
    {
        const char* homedir = getenv(
            #ifndef _WIN32
                    "HOME"
            #else
                    "HOMEPATH"
            #endif
        );
        if (!homedir)
            throw std::runtime_error("Cant get HOME env variable");
        path = homedir;
        path.append("/").append(dirname);
    }
    struct stat info;
    auto ret = stat(path.c_str(), &info);
    if (ret == 0)
    {
        if ((info.st_mode & S_IFDIR) == 0)
            throw std::runtime_error("Application directory path is taken by a file");
    }
    else
    {
        ret = mkdir(path.c_str(), 0700);
        if (ret)
        {
            throw std::runtime_error("Error creating application directory.");
        }
    }
    return path;
}

std::string Client::dbPath(const std::string& sid) const
{
    std::string path = mAppDir;
    if (sid.empty())    // anonoymous-mode
    {
        path.reserve(20);
        path.append("/karere-").append("anonymous.db");
    }
    else
    {
        if (sid.size() < 50)
            throw std::runtime_error("dbPath: sid is too small");

        path.reserve(56);
        path.append("/karere-").append(sid.c_str()+44).append(".db");
    }

    return path;
}

bool Client::openDb(const std::string& sid)
{
    assert(!sid.empty());
    std::string path = dbPath(sid);
    struct stat info;
    const bool succeeded = (stat(path.c_str(), &info) == 0);
    if (!succeeded)
    {
        KR_LOG_WARNING("%sError accessing local cache (database) file info. %s",
                       getLoggingName(),
                       std::strerror(errno));

        api.callIgnoreResult(&::mega::MegaApi::sendEvent,
                             99019,
                             "Error accessing local cache (database) file info",
                             false,
                             static_cast<const char*>(nullptr));
        return false;
    }

    bool ok = db.open(path.c_str(), false);
    if (!ok)
    {
        KR_LOG_WARNING("%sError opening database", getLoggingName());
        return false;
    }

    std::string cachedVersion;
    std::string currentVersion;
    bool result;
    {
        SqliteStmt stmt(db, "select value from vars where name = 'schema_version'");
        result = stmt.step();
        if (result)
        {
            currentVersion.assign(gDbSchemaHash);
            currentVersion.append("_").append(gDbSchemaVersionSuffix);    // <hash>_<suffix>

            cachedVersion.assign(stmt.stringCol(0));
        }
    }
    if (!result)
    {
        db.close();
        KR_LOG_WARNING("%sCan't get local database version", getLoggingName());
        return false;
    }

    if (cachedVersion != currentVersion)
    {
        ok = false;

        // if only version suffix changed, we may be able to provide backwards compatibility without
        // forcing a full reload, but just porting/adapting data
        size_t cachedVersionSuffixPos = cachedVersion.find_last_of('_');
        if (cachedVersionSuffixPos != std::string::npos)
        {
            std::string cachedVersionSuffix = cachedVersion.substr(cachedVersionSuffixPos + 1);
            if (cachedVersionSuffix == "2" && (strcmp(gDbSchemaVersionSuffix, "3") == 0))
            {
                KR_LOG_WARNING("%sClearing history from cached chats...", getLoggingName());

                // clients with version 2 missed the call-history msgs, need to clear cached history
                // in order to fetch fresh history including the missing management messages
                db.query("delete from history");
                db.query("update chat_vars set value = 0 where name = 'have_all_history'");
                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();
                KR_LOG_WARNING("%sSuccessfully cleared cached history. Database version has been "
                               "updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);

                ok = true;
            }
            else if (cachedVersionSuffix == "3" && (strcmp(gDbSchemaVersionSuffix, "4") == 0))
            {
                // clients with version 3 need to force a full-reload of SDK's cache to retrieve
                // "deleted" chats from API, since it used to not return them. It should only be
                // done in case there's at least one chat.

                SqliteStmt stmt(db, "select count(*) from chats");
                stmt.stepMustHaveData("get chats count");
                if (stmt.integralCol<int>(0) > 0)
                {
                    KR_LOG_WARNING("%sForcing a reload of SDK and MEGAchat caches...",
                                   getLoggingName());
                    api.sdk.invalidateCache();
                }
                else    // no chats --> only invalidate MEGAchat cache (the schema has changed)
                {
                    KR_LOG_WARNING("%sForcing a reload of SDK and MEGAchat cache...",
                                   getLoggingName());
                }

                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "4" && (strcmp(gDbSchemaVersionSuffix, "5") == 0))
            {
                // clients with version 4 need to create a new table `node_history` and populate it with
                // node's attachments already in cache. Futhermore, the existing types for special messages
                // (node-attachments, contact-attachments and rich-links) will be updated to a different
                // range to avoid collissions with the types of upcoming management messages.

                // Update obsolete type of special messages
                db.query("update history set type=? where type=?", chatd::Message::Type::kMsgAttachment, 0x10);
                db.query("update history set type=? where type=?", chatd::Message::Type::kMsgRevokeAttachment, 0x11);
                db.query("update history set type=? where type=?", chatd::Message::Type::kMsgContact, 0x12);
                db.query("update history set type=? where type=?", chatd::Message::Type::kMsgContainsMeta, 0x13);

                // Create new table for node history
                db.simpleQuery("CREATE TABLE node_history(idx int not null, chatid int64 not null, msgid int64 not null,"
                               "    userid int64, keyid int not null, type tinyint, updated smallint, ts int,"
                               "    is_encrypted tinyint, data blob, backrefid int64 not null, UNIQUE(chatid,msgid), UNIQUE(chatid,idx))");

                // Populate new table with existing node-attachments
                db.query("insert into node_history select * from history where type=?", std::to_string(chatd::Message::Type::kMsgAttachment));
                int count = sqlite3_changes(db);

                // Update DB version number
                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();

                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
                KR_LOG_WARNING("%s%d messages added to node history", getLoggingName(), count);
                ok = true;
            }
            else if (cachedVersionSuffix == "5" && (strcmp(gDbSchemaVersionSuffix, "6") == 0))
            {
                // Clients with version 5 need to force a full-reload of SDK's in case there's at least one group chat.
                // Otherwise the cache schema must be updated to support public chats

                SqliteStmt stmt(db, "select count(*) from chats where peer == -1");
                stmt.stepMustHaveData("get chats count");
                if (stmt.integralCol<int>(0) > 0)
                {
                    KR_LOG_WARNING("%sForcing a reload of SDK and MEGAchat caches...",
                                   getLoggingName());
                    api.sdk.invalidateCache();
                }
                else
                {
                    // no chats --> only update cache schema
                    KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());
                    db.query("ALTER TABLE `chats` ADD mode tinyint");
                    db.query("ALTER TABLE `chats` ADD unified_key blob");
                    db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                    db.commit();
                    ok = true;
                    KR_LOG_WARNING("%sDatabase version has been updated to %s",
                                   getLoggingName(),
                                   gDbSchemaVersionSuffix);
                }
            }
            else if (cachedVersionSuffix == "6" && (strcmp(gDbSchemaVersionSuffix, "7") == 0))
            {
                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.query("update history set keyid=0 where type=?", chatd::Message::Type::kMsgTruncate);
                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "7" && (strcmp(gDbSchemaVersionSuffix, "8") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());

                // Add reactionsn to chats table
                db.query("ALTER TABLE `chats` ADD rsn blob");

                // Create new table for chat reactions
                db.simpleQuery("CREATE TABLE chat_reactions(chatid int64 not null, msgid int64 not null,"
                               "    userid int64 not null, reaction text,"
                               "    UNIQUE(chatid, msgid, userid, reaction),"
                               "    FOREIGN KEY(chatid, msgid) REFERENCES history(chatid, msgid) ON DELETE CASCADE)");

                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "8" && (strcmp(gDbSchemaVersionSuffix, "9") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());

                // Add dns_cache table
                db.simpleQuery("CREATE TABLE dns_cache(shard tinyint primary key, url text, ipv4 text, ipv6 text);");
                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "9" && (strcmp(gDbSchemaVersionSuffix, "10") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());

                // Create new table for chat pending reactions
                db.simpleQuery("CREATE TABLE chat_pending_reactions(chatid int64 not null, msgid int64 not null,"
                               "    reaction text, encReaction blob, status tinyint default 0,"
                               "    UNIQUE(chatid, msgid, reaction),"
                               "    FOREIGN KEY(chatid, msgid) REFERENCES history(chatid, msgid) ON DELETE CASCADE)");

                // Remove USER_ATTR_RSA_PUBKEY attr from cache
                db.query("delete from userattrs where type = 64");

                // Create temporary table and copy sendkeys content
                db.query("CREATE TABLE tempkeys(chatid int64 not null, userid int64 not null, keyid int32 not null, key blob not null,ts int not null, UNIQUE(chatid, userid, keyid));");
                db.query("INSERT INTO tempkeys(chatid, userid, keyid, key, ts) SELECT chatid, userid, keyid, key, ts FROM sendkeys");

                // Close and re-open db again to avoid SQLITE_LOCKED
                db.close();
                if (db.open(path.c_str(), false))
                {
                    // drop sendkeys table
                    db.query("DROP TABLE sendkeys");

                    // rename temp to sendkeys
                    db.query("ALTER TABLE tempkeys RENAME TO sendkeys");

                    // update cache schema version
                    db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                    db.commit();
                    ok = true;
                    KR_LOG_WARNING("%sDatabase version has been updated to %s",
                                   getLoggingName(),
                                   gDbSchemaVersionSuffix);
                }
            }
            else if (cachedVersionSuffix == "10" && (strcmp(gDbSchemaVersionSuffix, "11") == 0))
            {
                KR_LOG_WARNING("%sPurging oldest message per chat...", getLoggingName());
                SqliteStmt stmt(db, "select msgid, min(idx), c.chatid from history as h INNER JOIN chat_vars as c on h.chatid = c.chatid where c.name = 'have_all_history' GROUP BY c.chatid;");
                while (stmt.step())
                {
                   karere::Id msgid = stmt.integralCol<uint64_t>(0);
                   karere::Id chatid = stmt.integralCol<uint64_t>(2);
                   db.query("delete from history where chatid = ? and msgid = ?", chatid, msgid);
                   db.query("delete from chat_vars where chatid = ? and name = 'have_all_history'", chatid);
                }

                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "11" && (strcmp(gDbSchemaVersionSuffix, "12") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());

                // Add tls session blob to dns_cache table
                db.query("ALTER TABLE `dns_cache` ADD sess_data blob");
                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "12" && (strcmp(gDbSchemaVersionSuffix, "13") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());

                // We check if we have some pulic chat with creation ts higher than meeting release
                // in that case we invalidate the cache, because it could a meetings
                // ts -> 1625140800000 -> 1 July 2021 12:00 GTM
                SqliteStmt stmt(db, "select count(*) from chats where mode == 1 and ts_created > 1618488000");
                stmt.stepMustHaveData("get chats count");
                if (stmt.integralCol<int>(0) > 0)
                {
                    KR_LOG_WARNING("%sForcing a reload of SDK and MEGAchat caches...",
                                   getLoggingName());
                    api.sdk.invalidateCache();
                }
                else //
                {
                    // Add meeting to chats table
                    try
                    {
                        db.query("ALTER TABLE `chats` ADD meeting tinyint default 0");
                    }
                    catch (const std::runtime_error&)
                    {
                        // meeting column is already added
                    }

                    db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                    db.commit();
                    ok = true;
                    KR_LOG_WARNING("%sDatabase version has been updated to %s",
                                   getLoggingName(),
                                   gDbSchemaVersionSuffix);
                }
            }
            else if (cachedVersionSuffix == "13" && (strcmp(gDbSchemaVersionSuffix, "14") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());
                db.query("ALTER TABLE `chats` ADD chat_options tinyint default 0");
                db.query("update vars set value = ? where name = 'schema_version'", currentVersion);
                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
            else if (cachedVersionSuffix == "14" && (strcmp(gDbSchemaVersionSuffix, "15") == 0))
            {
                KR_LOG_WARNING("%sUpdating schema of MEGAchat cache...", getLoggingName());
                db.query("CREATE TABLE scheduledMeetings(schedid int64 unique primary key, chatid int64, organizerid int64, parentschedid int64, timezone text,"
                            "startdatetime int64, enddatetime int64, title text, description text, attributes text, overrides int64, cancelled tinyint default 0,"
                            "flags int64 default 0, rules blob, FOREIGN KEY(chatid) REFERENCES chats(chatid) ON DELETE CASCADE)");

                db.query("CREATE TABLE scheduledMeetingsOccurr(schedid int64, startdatetime int64, enddatetime int64, PRIMARY KEY (schedid, startdatetime), "
                         "FOREIGN KEY(schedid) REFERENCES scheduledMeetings(schedid) ON DELETE CASCADE)");

                db.commit();
                ok = true;
                KR_LOG_WARNING("%sDatabase version has been updated to %s",
                               getLoggingName(),
                               gDbSchemaVersionSuffix);
            }
        }
    }

    if (!ok)
    {
        db.close();
        KR_LOG_WARNING(
            "%sDatabase schema version is not compatible with app version, will rebuild it",
            getLoggingName());
        return false;
    }

    mSid = sid;
    return true;
}

void Client::createDbSchema()
{
    mMyHandle = Id::inval();
    db.simpleQuery(gDbSchema); //db.query() uses a prepared statement and will execute only the first statement up to the first semicolon
    std::string ver(gDbSchemaHash);
    ver.append("_").append(gDbSchemaVersionSuffix);
    // not replaced by saveVarsValue encapsulation because this is a direct INSERT, thus the fallback is an ABORT not a REPLACE
    db.query("insert into vars(name, value) values('schema_version', ?)", ver);
    db.commit();
}

int Client::importMessages(const char *externalDbPath)
{
    SqliteDb dbExternal(app);
    if (!dbExternal.open(externalDbPath, false))
    {
        KR_LOG_ERROR("%simportMessages: failed to open external DB (%s)",
                     getLoggingName(),
                     externalDbPath);
        return -1;
    }
    const mega::MrProper cleanUp(
        [&dbExternal]()
        {
            dbExternal.close();
        });

    // check external DB uses the same DB schema than the app
    try // SqliteStmt constructor can throw
    {

    SqliteStmt stmtVersion(dbExternal, "select value from vars where name = 'schema_version'");
    if (!stmtVersion.step())
    {
        KR_LOG_ERROR("%simportMessages: failed to get external DB version", getLoggingName());
        return -2;
    }
    // check external DB uses the same DB version than the app
    std::string currentVersion(gDbSchemaHash);
    currentVersion.append("_").append(gDbSchemaVersionSuffix);    // <hash>_<suffix>
    std::string cachedVersion(stmtVersion.stringCol(0));
    if (cachedVersion != currentVersion)
    {
        KR_LOG_ERROR("%simportMessages: external DB version is too old", getLoggingName());
        return -3;
    }

    } // try
    catch (const std::runtime_error& error)
    {
        const char* dbgWhat = error.what();
        KR_LOG_ERROR("%simportMessages: failed to create SQL statement:\n%s",
                     getLoggingName(),
                     dbgWhat);
        return -5;
    }

    // check external DB is for the same user than the app's DB
    SqliteStmt stmtMyHandle(dbExternal, "select value from vars where name = 'my_handle'");
    if (!stmtMyHandle.step() || stmtMyHandle.integralCol<uint64_t>(0) != myHandle())
    {
        KR_LOG_ERROR("%simportMessages: external DB of a different user", getLoggingName());
        return -4;
    }

    // avoid to write each imported message to disk individually
    bool oldCommitMode = commitEach();
    setCommitMode(false);

    // for every chat, check messages to be added and/or updated
    int countAdded = 0;
    int countUpdated = 0;
    for (auto& it : *chats)
    {
        // find the newest message in the app
        karere::ChatRoom *chatroom = it.second;
        chatd::Chat &chat = chatroom->chat();
        karere::Id chatid = chatroom->chatid();
        uint32_t retentionTime = chat.getRetentionTime();

        // get id of last message seen from external db
        Id lastSeenId;
        SqliteStmt stmtLastSeen(dbExternal, "select last_seen from chats where chatid=?");
        stmtLastSeen << chatid;
        if (stmtLastSeen.step())
        {
            time_t lastSeenTs = 0;
            time_t expireRetentionTs = 0;
            lastSeenId = stmtLastSeen.integralCol<uint64_t>(0);
            if (retentionTime)
            {
                // check last seen message ts
                SqliteStmt stmtLastSeenTs(dbExternal, "select ts from history where chatid=? and msgid=?");
                stmtLastSeenTs << chatid;
                stmtLastSeenTs << lastSeenId;
                if (stmtLastSeenTs.step())
                {
                    lastSeenTs = stmtLastSeen.integralCol<time_t>(0);
                    expireRetentionTs = time(nullptr) - retentionTime;
                }
            }

            if (!retentionTime || lastSeenTs > expireRetentionTs)
            {
                chat.seenImport(lastSeenId);  // call seen import if no retention time is set or it hasn't expired
            }
        }
        else    // no SEEN pointer for this chat on external cache (or chat not found)
        {
            KR_LOG_WARNING("%simportMessages: SEEN not imported because chatid not found in "
                           "external db (chatid: %s)",
                           getLoggingName(),
                           chatid.toString().c_str());
        }

        chatd::Idx newestAppIdx = CHATD_IDX_INVALID;
        karere::Id newestAppMsgid(Id::inval());
        chatd::Message *newestAppMsg = nullptr;
        chatd::Idx firstIdxToImport = CHATD_IDX_INVALID;    // may not match the idx from app (even for same msgid)
        karere::Id firstMsgidToImport(Id::inval());
        uint32_t editableMsgsTs = 0;

        std::string query;
        if (!chat.empty())
        {
            newestAppIdx = chat.highnum();
            newestAppMsg = &chat.at(newestAppIdx);
            newestAppMsgid = firstMsgidToImport = newestAppMsg->id();

            // find the newest message known by the app in the external DB
            query = "select idx from history where chatid = ?1 and msgid = ?2";
            SqliteStmt stmt1(dbExternal, query.c_str());
            stmt1 << chatid << firstMsgidToImport;
            if (stmt1.step())
            {
                firstIdxToImport = stmt1.integralCol<int>(0);

                // ts of oldest message in app that could have been updated/deleted
                editableMsgsTs = newestAppMsg->ts - CHATD_MAX_EDIT_AGE;
            }
            else    // not found
            {
                // check if a truncate in external DB has cleared this message (idx greater than newest app msg)
                query = "select msgid, idx, type from history where chatid = ?1 and idx > ?2";
                SqliteStmt stmt2(dbExternal, query.c_str());
                stmt2 << chatid << newestAppIdx;
                if (stmt2.step())
                {
                    assert(stmt2.integralCol<chatd::Message::Type>(2) == chatd::Message::kMsgTruncate);
                    firstMsgidToImport = stmt2.integralCol<uint64_t>(0);
                    firstIdxToImport = stmt2.integralCol<int>(1);

                    KR_LOG_DEBUG(
                        "%simportMessages: truncate detected in chatid: %s msgid: %s idx: %d",
                        getLoggingName(),
                        chatid.toString().c_str(),
                        firstMsgidToImport.toString().c_str(),
                        firstIdxToImport);
                }
                else
                {
                    // (it means app is ahead of external DB for this chat, so nothing to import)
                    KR_LOG_DEBUG("%simportMessages: no messages to import for chatid: %s",
                                 getLoggingName(),
                                 chatid.toString().c_str());
                    continue;
                }
            }
        }
        else    // chat history is empty in the app
        {
            // find the oldest message in external DB: first msgid to import
            query = "select min(idx), msgid, idx from history where chatid = ?1";
            SqliteStmt stmt(dbExternal, query.c_str());
            stmt << chatid;
            if (stmt.step())
            {
                firstMsgidToImport = stmt.integralCol<uint64_t>(1);
                firstIdxToImport = stmt.integralCol<int>(2);
            }
            else
            {
                // chatroom has no history in external DB either
                continue;
            }
        }

        // for every newer message in external DB, add them to the app's history
        // (also consider the newest app message to update history in case of truncate)
        query = "select userid, ts, type, data, idx, keyid, backrefid, updated, is_encrypted, msgid from history"
                            " where chatid = ?1 and idx >= ?2";
        SqliteStmt stmtMsg(dbExternal, query.c_str());
        stmtMsg << chatroom->chatid() << firstIdxToImport;
        while (stmtMsg.step())
        {
            // restore Message from external DB
            std::unique_ptr<chatd::Message> msg;
            karere::Id userid(stmtMsg.integralCol<uint64_t>(0));
            karere::Id msgid(stmtMsg.integralCol<uint64_t>(9));
            uint32_t ts = stmtMsg.integralCol<uint32_t>(1);
            unsigned char type = stmtMsg.integralCol<unsigned char>(2);
            uint16_t updated = stmtMsg.integralCol<uint16_t>(7);
            chatd::KeyId keyid = stmtMsg.integralCol<chatd::KeyId>(5);
            Buffer buf;
            stmtMsg.blobCol(3, buf);
            msg.reset(new chatd::Message(msgid, userid, ts, updated, std::move(buf), false, keyid, type));
            msg->backRefId = stmtMsg.integralCol<uint64_t>(6);
            msg->setEncrypted(stmtMsg.integralCol<uint8_t>(8));

            bool isUpdate = false;
            if (msgid == newestAppMsgid)
            {
                // first message, if not updated or truncated, msg can be skipped
                isUpdate = (newestAppMsg->type != msg->type && msg->type == chatd::Message::kMsgTruncate)      // become a truncate
                        || (msg->type == chatd::Message::kMsgTruncate && msg->ts > newestAppMsg->ts)    // truncate a truncate
                        || (msg->updated > newestAppMsg->updated);  // edited/deleted

                if (!isUpdate)
                {
                    KR_LOG_DEBUG("%simportMessages: newest message not changed. Skipping... "
                                 "(chatid: %s msgid: %s)",
                                 getLoggingName(),
                                 chatid.toString().c_str(),
                                 msgid.toString().c_str());
                    continue;
                }
            }

            if (keyid != CHATD_KEYID_INVALID)   // keyid is invalid for mngt msgs and public chats
            {
                // restore the SendKey of the message from external DB
                std::string queryKey = "select key from sendkeys "
                        " where chatid = ?1 and userid = ?2 and keyid = ?3";
                SqliteStmt stmtKey(dbExternal, queryKey.c_str());
                stmtKey << chatroom->chatid() << userid << keyid;
                if (!stmtKey.step())
                {
                    KR_LOG_ERROR("%simportMessages: key not found. chatid: %s msgid: %s keyid %u",
                                 getLoggingName(),
                                 chatid.toString().c_str(),
                                 msgid.toString().c_str(),
                                 keyid);
                    continue;
                }
                Buffer key;
                stmtKey.blobCol(0, key);

                // import the corresponding key and the message itself
                chat.keyImport(keyid, userid, key.buf(), (uint16_t)key.dataSize());
            }

            if (retentionTime && ts <= time(nullptr) - retentionTime)
            {
                KR_LOG_DEBUG("%simportMessages: skipping msg with msgid %s that must be deleted "
                             "due to retention time policy",
                             getLoggingName(),
                             msg->id().toString().c_str());
                continue;
            }

            chat.msgImport(std::move(msg), isUpdate);
            (isUpdate) ? countUpdated++ : countAdded++;

            KR_LOG_DEBUG("%simportMessages: message added (chatid: %s msgid: %s)",
                         getLoggingName(),
                         chatid.toString().c_str(),
                         msgid.toString().c_str());
        }

        // finally, check if any older message has been updated
        if (editableMsgsTs) // 0 --> chat was empty or truncated
        {
            query = "select userid, ts, type, data, msgid, keyid, updated, backrefid, is_encrypted from history"
                                " where chatid = ?1 and ts > ?2 and updated > 0 and idx < ?3";

            SqliteStmt stmtMsgUpdated(dbExternal, query);
            stmtMsgUpdated << chatroom->chatid() << editableMsgsTs << firstIdxToImport;
            while (stmtMsgUpdated.step())
            {
                karere::Id msgid(stmtMsgUpdated.integralCol<uint64_t>(4));
                uint16_t updated = stmtMsgUpdated.integralCol<uint16_t>(6);

                // check if the edit in the external DB is newer than in app DB
                query = "select updated from history where chatid = ?1 and msgid = ?2";
                SqliteStmt stmtMsgAppUpdated(db, query);
                stmtMsgAppUpdated << chatroom->chatid() << msgid;
                if (!stmtMsgAppUpdated.step())
                {
                    KR_LOG_ERROR(
                        "%simportMessages: message not found in app's db (chatid: %s msgid: %s)",
                        getLoggingName(),
                        chatid.toString().c_str(),
                        msgid.toString().c_str());
                    continue;
                }
                uint16_t updatedApp = stmtMsgAppUpdated.integralCol<uint16_t>(0);
                if (updated <= updatedApp)
                {
                    KR_LOG_DEBUG("%simportMessages: edited message in external db is older. "
                                 "Skipping... (chatid: %s msgid: %s)",
                                 getLoggingName(),
                                 chatid.toString().c_str(),
                                 msgid.toString().c_str());
                    continue;
                }

                // restore Message from external DB
                std::unique_ptr<chatd::Message> msg;
                karere::Id userid(stmtMsgUpdated.integralCol<uint64_t>(0));
                uint32_t ts = stmtMsgUpdated.integralCol<uint32_t>(1);
                unsigned char type = stmtMsgUpdated.integralCol<unsigned char>(2);
                Buffer buf;
                stmtMsgUpdated.blobCol(3, buf);
                chatd::KeyId keyid = stmtMsgUpdated.integralCol<chatd::KeyId>(5);
                msg.reset(new chatd::Message(msgid, userid, ts, updated, std::move(buf), false, keyid, type));
                msg->backRefId = stmtMsgUpdated.integralCol<uint64_t>(7);
                msg->setEncrypted(stmtMsgUpdated.integralCol<uint8_t>(8));

                if (retentionTime && ts <= time(nullptr) - retentionTime)
                {
                    KR_LOG_DEBUG("%simportMessages: skipping msg (updated) with msgid %s that must "
                                 "be deleted due to retention time policy",
                                 getLoggingName(),
                                 msg->id().toString().c_str());
                    continue;
                }
                chat.msgImport(std::move(msg), true);
                countUpdated++;

                KR_LOG_DEBUG("%simportMessages: message updated (chatid: %s msgid: %s)",
                             getLoggingName(),
                             chatid.toString().c_str(),
                             msgid.toString().c_str());
            }
        }
    }

    // commit the transaction of importing msgs and restore previous mode
    setCommitMode(oldCommitMode);

    int total = countAdded + countUpdated;
    KR_LOG_DEBUG("%sImported messages: %d (added: %d, updated: %d)",
                 getLoggingName(),
                 total,
                 countAdded,
                 countUpdated);
    return total;
}

void Client::heartbeat()
{
    if (db.isOpen())
    {
        db.timedCommit();
    }

    if (mConnState != kConnected)
    {
        KR_LOG_WARNING("%sHeartbeat timer tick without being connected", getLoggingName());
        return;
    }

    mPresencedClient.heartbeat();
    if (mChatdClient)
    {
        mChatdClient->heartbeat();
    }
}

Client::~Client()
{
    assert(isTerminated());

#ifndef KARERE_DISABLE_WEBRTC
   rtc.reset();
#endif
}

void Client::retryPendingConnections(bool disconnect, bool refreshURL)
{
    if (mConnState == kDisconnected)  // already a connection attempt in-progress
    {
        KR_LOG_WARNING("%sRetry pending connections called without previous connect",
                       getLoggingName());
        return;
    }

    if (mChatdClient)
    {
        mChatdClient->retryPendingConnections(disconnect, refreshURL);
    }

#ifndef KARERE_DISABLE_WEBRTC
    if (rtc && !disconnect) // In case of disconnect, reconnection will be launched after chatd::Chat::setOnlineState
    {
        // force reconnect all SFU connections
        rtc->getSfuClient().retryPendingConnections(disconnect);
    }
#endif

    if (!anonymousMode())   // avoid to connect to presenced (no user, no peerstatus)
    {
        mPresencedClient.retryPendingConnection(disconnect, refreshURL);
    }
}

promise::Promise<void> Client::notifyUserStatus(bool background)
{
    bool oldStatus = mIsInBackground;
    mIsInBackground = background;
    if (mIsInBackground && !mInitStats.isCompleted())
    {
        mInitStats.onCanceled();
    }

    if (oldStatus == mIsInBackground)
    {
        return promise::_Void();
    }

    mPresencedClient.notifyUserStatus();
    if (!mChatdClient)
    {
        return promise::Error("Chatd client not initialized yet");
    }

    return mChatdClient->notifyUserStatus();
}

promise::Promise<void> Client::createSelfChat()
{
    if (chats->selfChat())
    {
        return promise::_Void();
    }
    auto wptr = getDelTracker();
    return api
        .call(&mega::MegaApi::createChat,
              false,
              nullptr,
              nullptr,
              mega::ChatOptions::kEmpty,
              nullptr)
        .then(
            [this, wptr](ReqResult result) -> Promise<void>
            {
                wptr.throwIfDeleted();
                auto& list = *result->getMegaTextChatList();
                if (list.size() < 1)
                {
                    return promise::Error("Empty chat list returned from API");
                }
                auto apiRoom = *list.get(0);
                if (apiRoom.isGroup())
                {
                    return promise::Error("API returned a group self-chat instead of 1on1");
                }
                if (apiRoom.getPeerList())
                {
                    return promise::Error("API returned a self-chat with more than 0 peers");
                }
                if (!chats->selfChat()) // if not created meanwhile
                {
                    auto chat = chats->addRoom(apiRoom);
                    assert(!chat->isGroup());
                    assert(chats->selfChat());
                    chat->connect();
                }
                else
                {
                    KR_LOG_DEBUG(
                        "%screateChat: Self-chat created by someone else created meanwhile",
                        getLoggingName());
                }
                return promise::_Void();
            });
}

promise::Promise<ReqResult> Client::openChatPreview(uint64_t publicHandle)
{
    auto wptr = weakHandle();
    return api.call(&::mega::MegaApi::getChatLinkURL, publicHandle);
}

void Client::createPublicChatRoom(uint64_t chatId, uint64_t ph, int shard, const std::string &decryptedTitle,
                                  std::shared_ptr<std::string> unifiedKey, const std::string &url, uint32_t ts,
                                  bool meeting, const ::mega::ChatOptions_t opts, const mega::MegaScheduledMeetingList* smList)
{
    GroupChatRoom* room = new GroupChatRoom(*chats, chatId, static_cast<unsigned char>(shard),
                                            chatd::Priv::PRIV_RO, ts, false, decryptedTitle, ph,
                                            unifiedKey, meeting, opts, smList);

    chats->emplace(chatId, room);
    if (!mDnsCache.hasRecord(shard))
    {
        // If DNS cache doesn't contains a record for this shard, addRecord otherwise skip.
        mDnsCache.addRecord(shard, url);    // the URL has been already pre-fetched
    }

    room->connect();
}

promise::Promise<KarereScheduledMeeting*> Client::createOrUpdateScheduledMeeting(const mega::MegaScheduledMeeting* scheduledMeeting, const char* chatTitle)
{
    auto wptr = getDelTracker();
    return api.call(&::mega::MegaApi::createOrUpdateScheduledMeeting, scheduledMeeting, chatTitle)
    .then([wptr](ReqResult result) -> promise::Promise<KarereScheduledMeeting*>
    {
        wptr.throwIfDeleted();
        if (!result->getMegaScheduledMeetingList() || result->getMegaScheduledMeetingList()->size() != 1)
        {
           return nullptr;
        }
        return new KarereScheduledMeeting (result->getMegaScheduledMeetingList()->at(0));
    });
}

promise::Promise<std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>>>
Client::fetchScheduledMeetingOccurrences(uint64_t chatid, ::mega::m_time_t since, ::mega::m_time_t until, unsigned int count)
{
    auto wptr = getDelTracker();
    return api.call(&::mega::MegaApi::fetchScheduledMeetingEvents, chatid, since, until, count)
    .then([wptr, this](ReqResult result) -> promise::Promise<std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>>>
    {
        wptr.throwIfDeleted();
        std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>> out;
        const mega::MegaScheduledMeetingList* l = result->getMegaScheduledMeetingList();
        if (l)
        {
            bool unordered = false;
            ::mega::m_time_t prevTs = ::mega::mega_invalid_timestamp;
            for (unsigned long i = 0; i < l->size(); i++)
            {
                if (!unordered
                        && prevTs != ::mega::mega_invalid_timestamp
                        && prevTs > l->at(i)->startDateTime())
                {
                    unordered = true;
                }
                out.emplace_back(new KarereScheduledMeetingOccurr(l->at(i)));
                prevTs = l->at(i)->startDateTime();
            }

            if (unordered)
            {
                KR_LOG_WARNING("%sUnordered occurrences list received from API", getLoggingName());
                sortOccurrences(out);
            }
        }
        return out;
    });
}

void Client::sortOccurrences(std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>>& occurrList) const
{
    auto cmp = [](std::shared_ptr<KarereScheduledMeetingOccurr>& a, std::shared_ptr<KarereScheduledMeetingOccurr>& b)
    {
        return a->startDateTime() < b->startDateTime();
    };

    std::sort(occurrList.begin(), occurrList.end(), cmp);
}

promise::Promise<void> Client::removeScheduledMeeting(uint64_t chatid, uint64_t schedId)
{
    auto wptr = getDelTracker();
    return api.call(&::mega::MegaApi::removeScheduledMeeting, chatid, schedId)
    .then([wptr](ReqResult) -> promise::Promise<void>
    {
        wptr.throwIfDeleted();
        return promise::_Void();
    });
}

promise::Promise<ReqResult> Client::ringIndividualInACall(const uint64_t chatid, const uint64_t userid)
{
    return api.call(&::mega::MegaApi::ringIndividualInACall, chatid, userid);
}

promise::Promise<std::string> Client::decryptChatTitle(uint64_t chatId, const std::string &key, const std::string &encTitle, const karere::Id& ph)
{
    std::shared_ptr<std::string> unifiedKey = std::make_shared<std::string>(key);
    Buffer buf(encTitle.size());

    try
    {
        size_t decLen = base64urldecode(encTitle.c_str(), encTitle.size(), buf.buf(), buf.bufSize());
        buf.setDataSize(decLen);

        //Create temporary strongvelope instance to decrypt chat title
        strongvelope::ProtocolHandler *auxCrypto = newStrongvelope(chatId, true, unifiedKey, false, ph);

        auto wptr = getDelTracker();
        promise::Promise<std::string> pms = auxCrypto->decryptChatTitleFromApi(buf);
        return pms
            .then(
                [wptr, auxCrypto](const std::string title)
                {
                    wptr.throwIfDeleted();
                    delete auxCrypto;
                    return title;
                })
            .fail(
                [wptr, chatId, auxCrypto, lname = std::string{getLoggingName()}](
                    const ::promise::Error& err)
                {
                    wptr.throwIfDeleted();
                    KR_LOG_ERROR("%sError decrypting chat title for chat link preview %s:\n%s",
                                 lname.c_str(),
                                 ID_CSTR(chatId),
                                 err.what());
                    delete auxCrypto;
                    return err;
                });
    }
    catch(std::exception&)
    {
        std::string err("Failed to base64-decode chat title for chat ");
        err.append(ID_CSTR(chatId)).append(": ");
        KR_LOG_ERROR("%s%s", getLoggingName(), err.c_str());
        return ::promise::Error(err);
    }
}

promise::Promise<void> Client::setPublicChatToPrivate(const karere::Id& chatid)
{
    GroupChatRoom *room = (GroupChatRoom *) chats->at(chatid);
    promise::Promise<std::shared_ptr<Buffer>> pms;
    if (room->hasTitle())
    {
        // encrypt chat-topic as in private mode, for same users (0), but creating a random key instead of using unified-key (true)
        pms = room->chat().crypto()->encryptChatTitle(room->titleString(), 0, true);
    }
    else
    {
        pms = promise::Promise<std::shared_ptr<Buffer>>();
        pms.resolve(std::make_shared<Buffer>());
    }

    auto wptr = weakHandle();
    return pms.then([wptr, this, chatid, room](const std::shared_ptr<Buffer>& encTitle)
    {
        wptr.throwIfDeleted();

        std::string auxbuf = base64urlencode(encTitle->buf(), encTitle->dataSize());
        const char *enctitleB64 = encTitle->dataSize() ? auxbuf.c_str() : NULL;

        return api.call(&::mega::MegaApi::chatLinkClose, chatid, enctitleB64)
        .then([room, wptr](ReqResult) -> promise::Promise<void>
        {
            if (wptr.deleted())
                return promise::_Void();

            room->setChatPrivateMode();
            return promise::_Void();
        });
    });
}

void Client::setSFUid(int sfuid)
{
    api.sdk.setSFUid(sfuid);
}

promise::Promise<uint64_t> Client::deleteChatLink(const karere::Id& chatid)
{
    return api.call(&::mega::MegaApi::chatLinkDelete, chatid)
    .then([](ReqResult) -> promise::Promise<uint64_t>
    {
        return Id::inval().val;
    });
}

promise::Promise<uint64_t> Client::getPublicHandle(const Id& chatid, bool createifmissing)
{
    ApiPromise pms;
    if (createifmissing)
    {
        pms = api.call(&::mega::MegaApi::chatLinkCreate, chatid);
    }
    else
    {
        pms = api.call(&::mega::MegaApi::chatLinkQuery, chatid);
    }

    auto wptr = weakHandle();
    return pms.then([wptr](ReqResult result) -> promise::Promise<uint64_t>
    {
        if (wptr.deleted())
            return Id::inval().val;

        return result->getParentHandle();
    });
}

void Client::onSyncReceived(const Id& chatid)
{
    if (mSyncCount <= 0)
    {
        KR_LOG_WARNING("%sUnexpected SYNC received for chat: %s",
                       getLoggingName(),
                       ID_CSTR(chatid));
        return;
    }

    mSyncCount--;
    if (mSyncCount == 0 && mSyncTimer)
    {
        cancelTimeout(mSyncTimer, appCtx);
        mSyncTimer = 0;

        mSyncPromise.resolve();
    }
}

bool Client::isChatRoomOpened(const Id& chatid)
{
    auto it = chats->find(chatid);
    if (it != chats->end())
    {
        return it->second->hasChatHandler();
    }
    return false;
}

void Client::saveDb()
{
    try
    {
        if (db.isOpen())
        {
            db.commit();
        }
    }
    catch(std::runtime_error& e)
    {
        KR_LOG_ERROR("%sError saving changes to local cache: %s", getLoggingName(), e.what());
        setInitState(kInitErrCorruptCache);
    }
}

void Client::connectLeanMode(Id chatId)
{
    assert(chatId.isValid());
    if (!mChatdClient->isChatLoggedIn(chatId) && mConnState != kDisconnected)
    {
        disconnectLeanMode();
    }

    // enable chatId and disable the rest of chats
    mChatdClient->enableChats(true /*enable*/, chatId);
    notifyUserStatus(true);

    // is mandatory to call connect() as we are in lean mode
    connect(false /*connectPresenced*/);
}

bool Client::isPendingPush()
{
    return mSyncTimer;
}

void Client::enableAllChats()
{
    mChatdClient->enableChats(true /*enable*/);
}

promise::Promise<void> Client::pushReceived(Id chatid)
{
    const bool iosPushReceived = chatid.isValid();
    promise::Promise<void> pms;
    ChatRoomList::const_iterator it = chats->find(chatid);
    ChatRoom *room = (it != chats->end()) ? it->second : NULL;
    if (!room || room->isArchived())
    {
        assert(!iosPushReceived);
        // room unknown or archived --> need to catchup wiht API to receive pending
        // actionpackets that may notify about a new chat or an existing chat being
        // unarchived (don't want notifications for archived)
        pms = api.callIgnoreResult(&::mega::MegaApi::catchup);
    }
    else
    {
        pms = Promise<void>();
        pms.resolve();
    }

    auto wptr = weakHandle();
    return pms.then([this, chatid, wptr]() -> promise::Promise<void>
    {
        if (wptr.deleted())
            return promise::Error("Up to date with API, but instance was removed");

        // if already sent SYNCs or we are not logged in right now...
        if (isPendingPush())
        {
            assert(!chatid.isValid()); // NSE should not have previous push in flight
            KR_LOG_WARNING("%spushReceived: a previous PUSH is being processed. Both will finish "
                           "at the same time",
                           getLoggingName());
            assert(!mSyncPromise.done());
            return mSyncPromise;
            // promise will resolve once logged in for all chats or after receive all SYNCs back
        }

        if (mSyncPromise.done())
        {
            KR_LOG_WARNING("%spushReceived: previous PUSH was already resolved. New promise to "
                           "track the progress",
                           getLoggingName());
            mSyncPromise = Promise<void>();
        }
        if (!mChatdClient || !mChatdClient->areAllChatsLoggedIn())
        {
            KR_LOG_WARNING("%spushReceived: not logged in into all chats", getLoggingName());
            return mSyncPromise;
        }

        // we are connected to all chats, sync with push received chatid or all (Android)
        // mSyncPromise is resolved when we are connected to all chats or when we receive sync from
        // chatd for chatid or all (Android)
        mSyncCount = 0;
        mSyncTimer = karere::setTimeout([this, wptr]()
        {
            if (wptr.deleted())
              return;

            assert(mSyncCount != 0);
            mSyncTimer = 0;
            mSyncCount = -1;

            mChatdClient->retryPendingConnections(true);

        }, chatd::kSyncTimeout, appCtx);

        if (chatid.isValid())
        {
            ChatRoom* chat = chats->at(chatid);
            if (!chat->chat().isDisabled())
            {
                mSyncCount++;
                if (mSyncCount != 1)
                {
                    KR_LOG_ERROR("%spushReceived (iOS): mSyncCount: %d (it should be 1)",
                                 getLoggingName(),
                                 mSyncCount);
                    assert(false);
                }
                chat->sendSync();
            }
            else
            {
                KR_LOG_ERROR("%spushReceived (iOS): chatid should be enabled %s",
                             getLoggingName(),
                             chatid.toString().c_str());
                assert(false);
            }
        }
        else
        {
            for (auto& item: *chats)
            {
                ChatRoom *chat = item.second;
                if (!chat->chat().isDisabled())
                {
                    mSyncCount++;
                    chat->sendSync();
                }
            }
        }

        return mSyncPromise;
    });
}

Client::InitState Client::initWithAnonymousSession()
{
    if (mInitState > kInitCreated)
    {
        KR_LOG_WARNING("%sinit: karere is already initialized. Current state: %s",
                       getLoggingName(),
                       initStateStr());
        return mInitState;
    }

    mInitStats.stageStart(InitStats::kStatsInit);
    setInitState(kInitAnonymousMode);
    mSid.clear();
    createDb();
    mMyHandle = Id::null(); // anonymous mode should use ownHandle set to all zeros
    mUserAttrCache.reset(new UserAttrCache(*this));
    mChatdClient.reset(new chatd::Client(this));
    connect();
    mInitStats.stageEnd(InitStats::kStatsInit);
    mInitStats.setInitState(mInitState);
    return mInitState;
}

bool Client::initWithNewSession(const char* sid, const std::string& scsn,
    mega::MegaUserList& contactList,
    mega::MegaTextChatList& chatList)
{
    assert(sid);

    mSid = sid ? sid : "";
    try
    {
        createDb();
    }
    catch (const std::runtime_error& e)
    {
        KR_LOG_ERROR("%sKarere log error: initWithNewSession: createDb() threw: %s",
                     getLoggingName(),
                     e.what());
        return false;
    }

// We have a complete snapshot of the SDK contact and chat list state.
// Commit it with the accompanying scsn
    mMyHandle = getMyHandleFromSdk();
    db.query("insert or replace into vars(name,value) values('my_handle', ?)", mMyHandle);

    mMyEmail = getMyEmailFromSdk();
    db.query("insert or replace into vars(name,value) values('my_email', ?)", mMyEmail);

    mMyIdentity = initMyIdentity();

    mUserAttrCache.reset(new UserAttrCache(*this));
    api.sdk.addGlobalListener(this);

    if (!loadOwnKeysFromApi())
    {
        return false;
    }

    // Add users from API
    mContactList->syncWithApi(contactList);
    mChatdClient.reset(new chatd::Client(this));
    assert(chats->empty());
    chats->onChatsUpdate(chatList);
    commit(scsn);

    // Get aliases from cache
    mAliasAttrHandle = mUserAttrCache->getAttr(mMyHandle,
    ::mega::MegaApi::USER_ATTR_ALIAS, this,
    [](Buffer *data, void *userp)
    {
        static_cast<Client*>(userp)->updateAliases(data);
    });

    return true;
}

void Client::setCommitMode(bool commitEach)
{
    db.setCommitMode(commitEach);
}

bool Client::commitEach()
{
    return db.commitEach();
}

void Client::commit(const std::string& scsn)
{
    if (scsn.empty())
    {
        KR_LOG_DEBUG("%sCommitting with empty scsn", getLoggingName());
        db.commit();
        return;
    }
    if (scsn == mLastScsn)
    {
        KR_LOG_DEBUG("%sCommitting with same scsn", getLoggingName());
        db.commit();
        return;
    }

    db.query("insert or replace into vars(name,value) values('scsn', ?)", scsn);
    db.commit();
    mLastScsn = scsn;
    KR_LOG_DEBUG("%sCommit with scsn %s", getLoggingName(), scsn.c_str());
}

void Client::onEvent(::mega::MegaApi* /*api*/, ::mega::MegaEvent* event)
{
    assert(event);
    int type = event->getType();
    switch (type)
    {
    case ::mega::MegaEvent::EVENT_COMMIT_DB:
    {
        const char *pscsn = event->getText();
        if (!pscsn)
        {
            KR_LOG_ERROR("%sEVENT_COMMIT_DB --> DB commit triggered by SDK without a valid scsn",
                         getLoggingName());
            return;
        }

        std::string scsn = pscsn;
        auto wptr = weakHandle();
        marshallCall([wptr, this, scsn]()
        {
            if (wptr.deleted())
            {
                return;
            }

            if (db.isOpen())
            {
                KR_LOG_DEBUG("%sEVENT_COMMIT_DB --> DB commit triggered by SDK", getLoggingName());
                commit(scsn);
            }

        }, appCtx);
        break;
    }

    default:
        break;
    }
}

void Client::initWithDbSession(const char* sid)
{
    try
    {
        assert(sid);
        if (!openDb(sid))
        {
            assert(mSid.empty());
            setInitState(kInitErrNoCache);
            return;
        }
        assert(db);
        assert(!mSid.empty());
        mUserAttrCache.reset(new UserAttrCache(*this));
        api.sdk.addGlobalListener(this);

        mMyHandle = getMyHandleFromDb();
        assert(mMyHandle && mMyHandle.isValid());

        mMyEmail = getMyEmailFromDb();

        mMyIdentity = getMyIdentityFromDb();
        mOwnNameAttrHandle =
            mUserAttrCache->getAttr(mMyHandle,
                                    USER_ATTR_FULLNAME,
                                    this,
                                    [](Buffer* buf, void* userp)
                                    {
                                        if (buf && !buf->empty())
                                        {
                                            static_cast<Client*>(userp)->setOwnName(*buf, false);
                                        }
                                    });

        loadOwnKeysFromDb();
        mDnsCache.loadFromDb();
        mContactList->loadFromDb();
        mChatdClient.reset(new chatd::Client(this));
        chats->loadFromDb();

#if WEBSOCKETS_TLS_SESSION_CACHE_ENABLED
        if (websocketIO && websocketIO->hasSessionCache())
        {
            auto&& sessions = mDnsCache.getTlsSessions();
            websocketIO->restoreSessions(std::move(sessions));
        }
#endif

        // Get aliases from cache
        mAliasAttrHandle = mUserAttrCache->getAttr(mMyHandle,
        ::mega::MegaApi::USER_ATTR_ALIAS, this,
        [](Buffer *data, void *userp)
        {
            static_cast<Client*>(userp)->updateAliases(data);
        });
    }
    catch(std::runtime_error& e)
    {
        KR_LOG_ERROR("%sinitWithDbSession: Error loading session from local cache: %s",
                     getLoggingName(),
                     e.what());
        setInitState(kInitErrCorruptCache);
        return;
    }

    setInitState(kInitHasOfflineSession);
    return;
}

void Client::setInitState(InitState newState)
{
    if (newState == mInitState)
        return;
    mInitState = newState;
    KR_LOG_DEBUG("%sClient reached init state %s", getLoggingName(), initStateStr());
    app.onInitStateChange(mInitState);
}

Client::InitState Client::init(const char* sid, bool waitForFetchnodesToConnect)
{
    if (mInitState > kInitCreated)
    {
        KR_LOG_WARNING("%sinit: karere is already initialized. Current state: %s",
                       getLoggingName(),
                       initStateStr());
        return mInitState;  // simply honor the current state
    }

    if (!waitForFetchnodesToConnect && !sid)
    {
        KR_LOG_ERROR("%sinit: sid required to initialize in Lean Mode", getLoggingName());
        return kInitErrGeneric;
    }

    mInitStats.stageStart(InitStats::kStatsInit);

    if (sid)
    {
        initWithDbSession(sid);
        if (mInitState == kInitErrNoCache ||    // not found, uncompatible db version, cannot open
                mInitState == kInitErrCorruptCache)
        {
            KR_LOG_DEBUG("%sKarere log debug: wipeDb() from Client::init()", getLoggingName());
            wipeDb(sid);
        }
    }
    else
    {
        assert(waitForFetchnodesToConnect);
        setInitState(kInitWaitingNewSession);
    }

    if (!waitForFetchnodesToConnect)
    {
        if (mInitState != kInitHasOfflineSession)
        {
            KR_LOG_ERROR("%sinit: failed to initialize Lean Mode. Current state: %s",
                         getLoggingName(),
                         initStateStr());
            return kInitErrGeneric;
        }

        mInitStats.onCanceled(); // do not collect stats for this initialization mode
    }

    mInitStats.stageEnd(InitStats::kStatsInit);
    mInitStats.setInitState(mInitState);
    api.sdk.addRequestListener(this);

    return mInitState;
}

void Client::onRequestStart(::mega::MegaApi* /*apiObj*/, ::mega::MegaRequest *request)
{
    int reqType = request->getType();
    switch (reqType)
    {
        case ::mega::MegaRequest::TYPE_CREATE_ACCOUNT:
        {
            if (request->getParamType() == 3)     // if creating E++ account...
            {
                mInitStats.stageStart(InitStats::kStatsCreateAccount);
            }
            break;
        }
        case ::mega::MegaRequest::TYPE_LOGIN:
        {
            mInitStats.stageStart(InitStats::kStatsLogin);
            break;
        }
        case ::mega::MegaRequest::TYPE_FETCH_NODES:
        {
            mInitStats.stageStart(InitStats::kStatsFetchNodes);
            break;
        }
        case ::mega::MegaRequest::TYPE_CONFIRM_ACCOUNT:
        {
            mInitStats.stageStart(InitStats::kStatsEphAccConfirmed);
            break;
        }
        default:    // no action to be taken for other type of requests
        {
            break;
        }
    }
}

void Client::onRequestFinish(::mega::MegaApi* /*apiObj*/, ::mega::MegaRequest *request, ::mega::MegaError* e)
{
    int reqType = request->getType();
    int errorCode = e->getErrorCode();
    if (errorCode != ::mega::MegaError::API_OK && reqType != ::mega::MegaRequest::TYPE_LOGOUT)
    {
        KR_LOG_ERROR("%sRequest %s finished with error %s",
                     getLoggingName(),
                     request->getRequestString(),
                     e->getErrorString());
        return;
    }

    switch (reqType)
    {
    case ::mega::MegaRequest::TYPE_LOGIN:
    {
        mInitStats.stageEnd(InitStats::kStatsLogin);
        break;
    }

    case ::mega::MegaRequest::TYPE_LOGOUT:
    {
        bool loggedOut = ((errorCode == ::mega::MegaError::API_OK || errorCode == ::mega::MegaError::API_ESID)
                          && (request->getFlag() || request->getParamType() == ::mega::MegaError::API_EBLOCKED));

        bool sessionExpired = request->getParamType() == ::mega::MegaError::API_ESID;       // SDK received ESID during login or any other request
        if (loggedOut)
            KR_LOG_DEBUG("%sLogout detected in the SDK. Closing MEGAchat session...",
                         getLoggingName());

        if (sessionExpired)
            KR_LOG_WARNING("%sExpired session detected. Closing MEGAchat session...",
                           getLoggingName());

        if (loggedOut || sessionExpired)
        {
            auto wptr = weakHandle();
            marshallCall([wptr, this]() // update state in the karere thread
            {
                if (wptr.deleted())
                    return;

                if (!isTerminated())
                {
                    setInitState(kInitErrSidInvalid);
                }
            }, appCtx);
            return;
        }
        break;
    }
    case ::mega::MegaRequest::TYPE_CREATE_ACCOUNT:  // fall-through
    case ::mega::MegaRequest::TYPE_FETCH_NODES:
    {
        if (reqType == ::mega::MegaRequest::TYPE_CREATE_ACCOUNT)  // if not creating E++ account, do nothing
        {
            if (request->getParamType() != ::mega::MegaApi::CREATE_EPLUSPLUS_ACCOUNT)     // if not creating E++ account, do nothing
            {
                break;
            }
            else    // -> create account E++ includes the fetchnodes (but not only, so new stage)
            {
                mInitStats.stageEnd(InitStats::kStatsCreateAccount);
            }
        }
        else
        {
            mInitStats.stageEnd(InitStats::kStatsFetchNodes);
        }

        api.sdk.pauseActionPackets();
        mInitStats.stageStart(InitStats::kStatsPostFetchNodes);

        auto oldState = mInitState;
        char* pscsn = api.sdk.getSequenceNumber();
        std::string scsn;
        if (pscsn)
        {
            scsn = pscsn;
            delete[] pscsn;
        }
        std::shared_ptr<::mega::MegaUserList> contactList(api.sdk.getContacts());
        std::shared_ptr<::mega::MegaTextChatList> chatList(api.sdk.getChatList());
        std::unique_ptr<char[]> sid(api.sdk.dumpSession());
        assert(sid);

#ifndef NDEBUG
        dumpContactList(*contactList);
#endif

        auto wptr = weakHandle();
        marshallCall(
            [wptr,
             this,
             lname = std::string{getLoggingName()},
             oldState,
             scsn,
             contactList,
             chatList,
             sess = std::move(sid)]()
            {
                if (wptr.deleted())
                    return;

                auto currentState = mInitState;
                if (oldState != currentState)
                {
                    KR_LOG_WARNING(
                        "%sOnrequestFinish(TYPE_FETCH_NODES): client state changed old: %d new: %d",
                        lname.c_str(),
                        oldState,
                        currentState);
                }

                switch (currentState)
                {
                    case kInitHasOfflineSession:
                        checkSyncWithSdkDb(scsn, *contactList, *chatList, false);
                        setInitState(kInitHasOnlineSession);
                        mInitStats.stageEnd(InitStats::kStatsPostFetchNodes);
                        api.sdk.resumeActionPackets();

                        connect();
                        break;
                    case kInitWaitingNewSession:
                    case kInitErrNoCache:
                        if (initWithNewSession(sess.get(), scsn, *contactList, *chatList))
                        {
                            setInitState(kInitHasOnlineSession);
                            mInitStats.stageEnd(InitStats::kStatsPostFetchNodes);
                            api.sdk.resumeActionPackets();

                            connect();
                        }
                        else
                        {
                            setInitState(kInitErrGeneric);
                            KR_LOG_ERROR("%sFailed to initialize MEGAchat", lname.c_str());
                            api.sdk.resumeActionPackets();
                        }
                        break;
                    case kInitHasOnlineSession:
                        // a full reload happened (triggered by API or by the user)
                        checkSyncWithSdkDb(scsn, *contactList, *chatList, true);
                        api.sdk.resumeActionPackets();
                        break;
                    case kInitTerminated:
                        KR_LOG_ERROR("%sOnrequestFinish(TYPE_FETCH_NODES): client state terminated",
                                     lname.c_str());
                        break;
                    case kInitErrCorruptCache:
                    case kInitErrGeneric:
                    case kInitCreated:
                    case kInitAnonymousMode:
                    case kInitErrSidInvalid:
                        KR_LOG_ERROR(
                            "%sOnrequestFinish(TYPE_FETCH_NODES): unexpected client state: %d",
                            lname.c_str(),
                            currentState);
                        api.callIgnoreResult(
                            &::mega::MegaApi::sendEvent,
                            99020,
                            "unexpected karere init state upon fetchnodes completion",
                            false,
                            static_cast<const char*>(nullptr));
                        break;
                }
            },
            appCtx);
        break;
    }

    case ::mega::MegaRequest::TYPE_SET_ATTR_USER:
    {
        int attrType = request->getParamType();
        int changeType;
        if (attrType == ::mega::MegaApi::USER_ATTR_FIRSTNAME)
        {
            changeType = ::mega::MegaUser::CHANGE_TYPE_FIRSTNAME;
        }
        else if (attrType == ::mega::MegaApi::USER_ATTR_LASTNAME)
        {
            changeType = ::mega::MegaUser::CHANGE_TYPE_LASTNAME;
        }
        else if (attrType == ::mega::MegaApi::USER_ATTR_ALIAS)
        {
            changeType = ::mega::MegaUser::CHANGE_TYPE_ALIAS;
        }
        else if (attrType == ::mega::MegaApi::USER_ATTR_RICH_PREVIEWS)
        {
            changeType = ::mega::MegaUser::CHANGE_TYPE_RICH_PREVIEWS;
        }
        else
        {
            return;
        }

        auto wptr = weakHandle();
        marshallCall([wptr, this, changeType]()
        {
            if (wptr.deleted())
                return;

            mUserAttrCache->onUserAttrChange(mMyHandle, changeType);
        }, appCtx);
        break;
    }

    case ::mega::MegaRequest::TYPE_CONFIRM_ACCOUNT:
    {
        std::string email = request->getEmail();
        // checking if there is a change in the e-mail we cover 2 use cases:
        //1) the confirmation of an ephemeral account ++ where there was no email
        //2) the confirmation of an account where the signing email is different than the one used
        //during the initial step of the account creation
        if (email != getMyEmail())
        {
            mInitStats.stageEnd(InitStats::kStatsEphAccConfirmed);

            setMyEmail(email);
            db.query("insert or replace into vars(name,value) values('my_email', ?)", email);
        }

        break;
    }

    default:    // no action to be taken for other type of requests
    {
        break;
    }
    }
}

//TODO: We should actually wipe the whole app dir, but the log file may
//be in that dir, and it is in use
void Client::wipeDb(const std::string& sid)
{
    db.close();
    std::string path = dbPath(sid);
    int removed = remove(path.c_str());
    KR_LOG_DEBUG("%sKarere log debug: db wipe: %d, %s", getLoggingName(), removed, path.c_str());
    struct stat info;
    if (stat(path.c_str(), &info) == 0)
        throw std::runtime_error("wipeDb: Could not delete old database file in "+mAppDir);
}

void Client::createDb()
{
    KR_LOG_DEBUG("%sKarere log debug: wipeDb() from Client::createDb()", getLoggingName());
    wipeDb(mSid);
    std::string path = dbPath(mSid);
    if (!db.open(path.c_str(), false))
        throw std::runtime_error("Can't access application database at "+mAppDir);
    createDbSchema(); //calls commit() at the end
}

bool Client::checkSyncWithSdkDb(const std::string& scsn,
    ::mega::MegaUserList& aContactList, ::mega::MegaTextChatList& chatList, bool forceReload)
{
    if (!forceReload)
    {
        // check if 'scsn' has changed
        SqliteStmt stmt(db, "select value from vars where name='scsn'");
        stmt.stepMustHaveData("get karere scsn");
        if (stmt.stringCol(0) == scsn)
        {
            KR_LOG_DEBUG("%sDb sync ok, karere scsn matches with the one from sdk",
                         getLoggingName());
            return true;
        }
        api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99012, "Karere db out of sync with sdk - scsn-s don't match", false, static_cast<const char*>(nullptr));
    }

    // We are not in sync, probably karere is one or more commits behind
    KR_LOG_WARNING(
        "%sKarere db out of sync with sdk - scsn-s don't match. Will reload all state from SDK",
        getLoggingName());

    // invalidate user attrib cache
    mUserAttrCache->invalidate();

    // sync contactlist first
    mContactList->clear();   // remove obsolete users, just in case, and add them fresh from SDK
    mContactList->syncWithApi(aContactList);

    // sync the chatroom list
    chats->onChatsUpdate(chatList, forceReload);

    // commit the snapshot
    commit(scsn);
    return false;
}

void Client::dumpChatrooms(::mega::MegaTextChatList& chatRooms)
{
    KR_LOG_DEBUG("%s=== Chatrooms received from API: ===", getLoggingName());
    for (int i=0; i<chatRooms.size(); i++)
    {
        auto& room = *chatRooms.get(i);
        if (room.isGroup())
        {
            KR_LOG_DEBUG("%s%s(group, ownPriv=%s):",
                         getLoggingName(),
                         ID_CSTR(room.getHandle()),
                         privToString((chatd::Priv)room.getOwnPrivilege()));
        }
        else
        {
            KR_LOG_DEBUG("%s%s(1on1)", getLoggingName(), ID_CSTR(room.getHandle()));
        }
        auto peers = room.getPeerList();
        if (!peers)
        {
            KR_LOG_DEBUG("%s  (room has no peers)", getLoggingName());
            continue;
        }
        for (int j = 0; j<peers->size(); j++)
            KR_LOG_DEBUG("%s  %s: %s",
                         getLoggingName(),
                         ID_CSTR(peers->getPeerHandle(j)),
                         privToString((chatd::Priv)peers->getPeerPrivilege(j)));
    }
    KR_LOG_DEBUG("%s=== Chatroom list end ===", getLoggingName());
}
void Client::dumpContactList(::mega::MegaUserList& clist)
{
    KR_LOG_DEBUG("%s== Contactlist received from API: ==", getLoggingName());
    for (int i=0; i< clist.size(); i++)
    {
        auto& user = *clist.get(i);
        auto visibility = user.getVisibility();
        if (visibility != ::mega::MegaUser::VISIBILITY_VISIBLE)
            KR_LOG_DEBUG("%s  %s (visibility = %d)",
                         getLoggingName(),
                         ID_CSTR(user.getHandle()),
                         visibility);
        else
            KR_LOG_DEBUG("%s  %s", getLoggingName(), ID_CSTR(user.getHandle()));
    }
    KR_LOG_DEBUG("%s== Contactlist end ==", getLoggingName());
}

void Client::disconnectLeanMode()
{
    assert(mConnState != kDisconnected);
    mInitStats.onCanceled();
    setConnState(kDisconnected);

    // stop heartbeats
    if (mHeartbeatTimer)
    {
        karere::cancelInterval(mHeartbeatTimer, appCtx);
        mHeartbeatTimer = 0;
    }

    mChatdClient->disconnect(true);
}

void Client::connect(const bool connectPresenced)
{
    // cancel stats if connection is done in background (not reliable times)
    if (mIsInBackground && !mInitStats.isCompleted())
    {
        mInitStats.onCanceled();
    }

    if (mConnState != kDisconnected)
    {
        KR_LOG_WARNING("%sconnect(): current state is %s",
                       getLoggingName(),
                       connStateToStr(mConnState));
        return;
    }

    KR_LOG_DEBUG("%sConnecting to account '%s'(%s)...",
                 getLoggingName(),
                 SdkString(api.sdk.getMyEmail()).c_str(),
                 mMyHandle.toString().c_str());
    mInitStats.stageStart(InitStats::kStatsConnection);
    setConnState(kConnecting);

    // notify user-attr cache
    mUserAttrCache->onLogin();

    connectToChatd();

    // start heartbeats
    auto wptr = weakHandle();
    assert(!mHeartbeatTimer);
    mHeartbeatTimer = karere::setInterval([this, wptr]()
    {
        if (wptr.deleted() || !mHeartbeatTimer)
        {
            return;
        }

        heartbeat();
    }, kHeartbeatTimeout, appCtx);

    if (anonymousMode())
    {
        // avoid to connect to presenced (no user, no peerstatus)
        // avoid to retrieve own user-attributes (no user, no attributes)
        setConnState(kConnected);
        return;
    }

    mOwnNameAttrHandle = mUserAttrCache->getAttr(mMyHandle, USER_ATTR_FULLNAME, this,
    [](Buffer* buf, void* userp)
    {
        if (!buf || buf->empty())
        {
            return;
        }
        auto& client = *static_cast<Client*>(userp);
        client.setOwnName(*buf, true);
        KR_LOG_DEBUG("%sOwn screen name is: '%s'",
                     client.getLoggingName(),
                     client.myName().c_str() + 1);
    });

    if (connectPresenced)
    {
        mPresencedClient.connect();
    }
    setConnState(kConnected);
}

void Client::setConnState(ConnState newState)
{
    mConnState = newState;
    KR_LOG_DEBUG("%sClient connection state changed to %s",
                 getLoggingName(),
                 connStateToStr(newState));
}

void Client::setOwnName(const Buffer& data, bool isInitial)
{
    assert(!data.empty());
    mMyName.assign(data.buf(), data.dataSize());
    if (chats->selfChat())
    {
        chats->selfChat()->updateTitle(std::string(mMyName.c_str() + 1, mMyName.size() - 1));
    }
}
void Client::sendStats()
{
    if (mInitStats.isCompleted())
    {
        return;
    }

    std::string stats = mInitStats.onCompleted(api.sdk.getNumNodes(), chats->size(), mContactList->size());
    KR_LOG_DEBUG("%sInit stats: %s", getLoggingName(), stats.c_str());
    api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99008, jsonUnescape(stats).c_str(), false, static_cast<const char*>(nullptr));
}

InitStats& Client::initStats()
{
    return mInitStats;
}

karere::Id Client::getMyHandleFromSdk()
{
    SdkString uh = api.sdk.getMyUserHandle();
    if (!uh.c_str() || !uh.c_str()[0])
        throw std::runtime_error("Could not get our own user handle from API");
    KR_LOG_INFO("%sOur user handle is %s", getLoggingName(), uh.c_str());
    karere::Id result(uh.c_str());
    if (result == Id::null() || result.val == ::mega::UNDEF)
        throw std::runtime_error("Own handle returned by the SDK is NULL");
    return result;
}

std::string Client::getMyEmailFromDb()
{
    SqliteStmt stmt(db, "select value from vars where name='my_email'");
    if (!stmt.step())
        throw std::runtime_error("No own email in database");

    std::string email = stmt.stringCol(0);

    return email;
}

std::string Client::getMyEmailFromSdk()
{
    SdkString myEmail = api.sdk.getMyEmail();
    if (!myEmail.c_str() || !myEmail.c_str()[0])
    {
        // For ephemeral accounts email isn't set
        return std::string("");
    }
    KR_LOG_INFO("%sOur email address is %s", getLoggingName(), myEmail.c_str());
    return myEmail.c_str();
}

karere::Id Client::getMyHandleFromDb()
{
    SqliteStmt stmt(db, "select value from vars where name='my_handle'");
    if (!stmt.step())
        throw std::runtime_error("No own user handle in database");

    karere::Id result = stmt.integralCol<uint64_t>(0);

    if (result == Id::null() || result.val == mega::UNDEF)
        throw std::runtime_error("loadOwnUserHandleFromDb: Own handle in db is invalid");
    return result;
}

uint64_t Client::getMyIdentityFromDb()
{
    uint64_t result = 0;

    SqliteStmt stmt(db, "select value from vars where name='clientid_seed'");
    if (!stmt.step())
    {
        KR_LOG_WARNING("%sclientid_seed not found in DB. Creating a new one", getLoggingName());
        result = initMyIdentity();
    }
    else
    {
        result = stmt.integralCol<uint64_t>(0);
        if (result == 0)
        {
            KR_LOG_WARNING("%sclientid_seed in DB is invalid. Creating a new one",
                           getLoggingName());
            result = initMyIdentity();
        }
    }
    return result;
}

void Client::resetMyIdentity()
{
   assert(mInitState == kInitWaitingNewSession || mInitState == kInitHasOfflineSession);
   KR_LOG_WARNING("%sReset clientid_seed", getLoggingName());
   mMyIdentity = initMyIdentity();
}

uint64_t Client::initMyIdentity()
{
    uint64_t result = (static_cast<uint64_t>(rand()) << 32) | ::mega::m_time();
    db.query("insert or replace into vars(name,value) values('clientid_seed', ?)", result);
    return result;
}

bool Client::loadOwnKeysFromApi()
{
    std::unique_ptr<char[]> prEd255(api.sdk.getPrivateKey(mega::MegaApi::PRIVATE_KEY_ED25519));
    std::unique_ptr<char[]> prCu255(api.sdk.getPrivateKey(mega::MegaApi::PRIVATE_KEY_CU25519));

    if (!prEd255 || !prCu255)
    {
        KR_LOG_ERROR("%sloadOwnKeysFromApi: failure loading keys from API", getLoggingName());
        return false;
    }

    auto b64len = strlen(prCu255.get());
    if (b64len != 43)
    {
        KR_LOG_ERROR("%sloadOwnKeysFromApi: Invalid size for private cu255 key", getLoggingName());
        return false;
    }

    base64urldecode(prCu255.get(), b64len, mMyPrivCu25519, sizeof(mMyPrivCu25519));

    b64len = strlen(prEd255.get());
    if (b64len != 43)
    {
        KR_LOG_ERROR("%sloadOwnKeysFromApi: Invalid size for private ed255 key", getLoggingName());
        return false;
    }

    base64urldecode(prEd255.get(), b64len, mMyPrivEd25519, sizeof(mMyPrivEd25519));

    // write to db
    db.query("insert or replace into vars(name,value) values('pr_cu25519', ?)", StaticBuffer(mMyPrivCu25519, sizeof(mMyPrivCu25519)));
    db.query("insert or replace into vars(name,value) values('pr_ed25519', ?)", StaticBuffer(mMyPrivEd25519, sizeof(mMyPrivEd25519)));
    KR_LOG_DEBUG("%sloadOwnKeysFromApi: success", getLoggingName());

    return true;
}

void Client::loadOwnKeysFromDb()
{
    SqliteStmt stmt(db, "select value from vars where name=?");
    stmt.reset().clearBind();
    stmt << "pr_cu25519";
    stmt.stepMustHaveData();
    auto len = stmt.blobCol(0, mMyPrivCu25519, sizeof(mMyPrivCu25519));
    if (len != sizeof(mMyPrivCu25519))
        throw std::runtime_error("Unexpected length of privCu25519 in database");
    stmt.reset().clearBind();
    stmt << "pr_ed25519";
    stmt.stepMustHaveData();
    len = stmt.blobCol(0, mMyPrivEd25519, sizeof(mMyPrivEd25519));
    if (len != sizeof(mMyPrivEd25519))
        throw std::runtime_error("Unexpected length of privEd2519 in database");
}

// presenced handlers
void Client::onPresenceChange(Id userid, Presence pres, bool inProgress)
{
    if (isTerminated())
    {
        return;
    }

    // Notify apps
    app.onPresenceChanged(userid, pres, inProgress);
}

void Client::onPresenceConfigChanged(const presenced::Config& state, bool pending)
{
    app.onPresenceConfigChanged(state, pending);
}

void Client::onPresenceLastGreenUpdated(Id userid)
{
    // This callback is received from presenced upon reception of LASTGREEN
    updateAndNotifyLastGreen(userid.val);
}

void Client::updateAndNotifyLastGreen(const Id& userid)
{
    mega::m_time_t lastGreenTs = mPresencedClient.getLastGreen(userid);
    if (!lastGreenTs)
    {
        KR_LOG_DEBUG("%sSkip notification, last-green not received yet", getLoggingName());
        return;
    }

    mega::m_time_t lastMsgTs = mChatdClient->getLastMsgTs(userid);

    // check what is newer: ts from chatd (messages) or ts from presenced (last-green response)
    mega::m_time_t lastGreen = (lastGreenTs >= lastMsgTs) ? lastGreenTs : lastMsgTs;

    // Update last green and notify apps, if required
    bool changed = mPresencedClient.updateLastGreen(userid.val, lastGreen);
    if (changed)
    {
        uint16_t lastGreenMinutes = static_cast<uint16_t>((time(NULL) - lastGreen) / 60);
        app.onPresenceLastGreenUpdated(userid, lastGreenMinutes);
    }
}

void Client::onConnStateChange(presenced::Client::ConnState /*state*/)
{

}

void Client::terminate(bool deleteDb)
{
    setInitState(kInitTerminated);

    api.sdk.removeRequestListener(this);
    api.sdk.removeGlobalListener(this);


    // pre-destroy chatrooms in preview mode (cleanup from DB + send HANDLELEAVE)
    // Otherwise, DB will be already closed when the GroupChatRoom dtor is called
    for (auto it = chats->begin(); it != chats->end();)
    {
        if (it->second->previewMode())
        {
            delete it->second;
            auto itToRemove = it;
            it++;
            chats->erase(itToRemove);
        }
        else
        {
            it++;
        }
    }

    if (mConnState != kDisconnected)
    {
        setConnState(kDisconnected);

        // stop syncing own-name and close user-attributes cache
        mUserAttrCache->removeCb(mOwnNameAttrHandle);
        mUserAttrCache->removeCb(mAliasAttrHandle);
        mUserAttrCache->onLogOut();
        mUserAttrCache.reset();

        // stop heartbeats
        if (mHeartbeatTimer)
        {
            karere::cancelInterval(mHeartbeatTimer, appCtx);
            mHeartbeatTimer = 0;
        }

        // disconnect from chatd shards and presenced
        mChatdClient->disconnect();
        mPresencedClient.disconnect();
    }

    // close or delete MEGAchat's DB file
    try
    {
        if (deleteDb)
        {
            KR_LOG_DEBUG("%sKarere log debug: wipeDb() from Client::terminate()", getLoggingName());
            wipeDb(mSid);
        }
        else if (db.isOpen())
        {
            KR_LOG_INFO("%sDoing final COMMIT to database", getLoggingName());
            db.commit();
            db.close();
        }
    }
    catch(std::runtime_error& e)
    {
        KR_LOG_ERROR("%sError saving changes to local cache during termination: %s",
                     getLoggingName(),
                     e.what());
    }
}

promise::Promise<void> Client::setPresence(Presence pres)
{
    if (pres == mPresencedClient.config().presence())
    {
        std::string err = "setPresence: tried to change online state to the current configured state (";
        err.append(pres.toString()).append(")");
        return ::promise::Error(err, kErrorArgs);
    }

    bool ret = mPresencedClient.setPresence(pres);
    if (!ret)
    {
        return ::promise::Error("setPresence: not connected", kErrorAccess);
    }

    return promise::_Void();
}

void Client::onUsersUpdate(mega::MegaApi* /*api*/, mega::MegaUserList *aUsers)
{
    if (!aUsers)
        return;

    std::shared_ptr<mega::MegaUserList> users(aUsers->copy());
    auto wptr = weakHandle();
    marshallCall([wptr, this, users]()
    {
        if (wptr.deleted())
        {
            return;
        }

        mContactList->syncWithApi(*users);

        // check changes for own user
        int count = users->size();
        for (int i = 0; i < count; i++)
        {
            ::mega::MegaUser &user = *users->get(i);
            if (user.getHandle() != myHandle()) continue;

            if (user.hasChanged(::mega::MegaUser::CHANGE_TYPE_EMAIL))
            {
                // Update our own email in client and caches
                std::string email = user.getEmail();
                setMyEmail(email);
                db.query("insert or replace into vars(name,value) values('my_email', ?)", email);
            }

            if (!user.isOwnChange())
            {
                userAttrCache().onUserAttrChange(user);
            }
        }
    }, appCtx);
}

promise::Promise<std::pair<karere::Id, std::shared_ptr<KarereScheduledMeeting>>>
Client::createGroupChat(std::vector<std::pair<uint64_t, chatd::Priv>> peers, bool publicchat, bool meeting, int options, const char* title, std::shared_ptr<::mega::MegaScheduledMeeting> sm)
{
    // prepare set of participants
    std::shared_ptr<mega::MegaTextChatPeerList> sdkPeers(mega::MegaTextChatPeerList::createInstance());
    std::shared_ptr<SetOfIds> users = std::make_shared<SetOfIds>();
    users->insert(mMyHandle);
    for (auto& peer: peers)
    {
        sdkPeers->addPeer(peer.first, peer.second);
        users->insert(peer.first);
    }

    // prepare unified key (if public chat)
    std::shared_ptr<std::string> unifiedKey;
    if (publicchat)
    {
        Buffer *buf = strongvelope::ProtocolHandler::createUnifiedKey();
        unifiedKey = std::make_shared<std::string>(buf->buf(), buf->dataSize());
        delete buf;
    }

    // create strongvelope for encryption of title/unified-key
    std::shared_ptr<strongvelope::ProtocolHandler> crypto;
    if (publicchat || title)
    {
        crypto = std::make_shared<strongvelope::ProtocolHandler>(mMyHandle,
                StaticBuffer(mMyPrivCu25519, 32), StaticBuffer(mMyPrivEd25519, 32),
                *mUserAttrCache, db, karere::Id::inval(), publicchat,
                unifiedKey, false, Id::inval(), appCtx);
        crypto->setUsers(users.get());  // ownership belongs to this method, it will be released after `crypto`
    }

    promise::Promise<std::shared_ptr<Buffer>> pms;
    if (title)
    {
        const std::string auxTitle(title);
        pms = crypto->encryptChatTitle(auxTitle);
    }
    else
    {
        pms = promise::Promise<std::shared_ptr<Buffer>>();
        pms.resolve(std::make_shared<Buffer>());
    }

    // capture `users`, since it's used at strongvelope for encryption of unified-key in public chats
    auto wptr = getDelTracker();
    return pms.then([wptr, this, crypto, users, sdkPeers, publicchat, meeting, options, sm](const std::shared_ptr<Buffer>& encTitle) -> promise::Promise<std::pair<karere::Id, std::shared_ptr<KarereScheduledMeeting>>>
    {
        if (wptr.deleted())
        {
            return ::promise::Error("Title encrypted successfully, but instance was removed");
        }

        std::string enctitleB64;
        if (!encTitle->empty())
        {
            enctitleB64 = base64urlencode(encTitle->buf(), encTitle->dataSize());
        }

        ApiPromise createChatPromise;

        if (publicchat)
        {
            createChatPromise = crypto->encryptUnifiedKeyForAllParticipants()
            .then([wptr, this, crypto, sdkPeers, enctitleB64, meeting, options, sm](chatd::KeyCommand *keyCmd) -> ApiPromise
            {
                mega::MegaStringMap *userKeyMap;
                userKeyMap = mega::MegaStringMap::createInstance();

                for (int i = 0; i < sdkPeers->size(); i++)
                {
                    //Get peer Handle in B64
                    Id peerHandle(sdkPeers->getPeerHandle(i));

                    //Get peer unified key
                    auto useruk = keyCmd->getKeyByUserId(peerHandle);

                    //Append [creatorhandle+uk]
                    std::string uKeyBin((const char*)&mMyHandle, sizeof(mMyHandle.val));
                    uKeyBin.append(useruk->buf(), useruk->size());

                    //Encode [creatorhandle+uk] to B64
                    std::string uKeyB64;
                    mega::Base64::btoa(uKeyBin, uKeyB64);

                    //Add entry to map
                    userKeyMap->set(peerHandle.toString().c_str(), uKeyB64.c_str());
                }

                //Get own unified key
                auto ownKey = keyCmd->getKeyByUserId(mMyHandle);

                //Append [creatorhandle+uk]
                std::string okeyBin((const char*)&mMyHandle, sizeof(mMyHandle.val));
                okeyBin.append(ownKey->buf(), ownKey->size());

                //Encode [creatorhandle+uk] to B64
                std::string oKeyB64;
                mega::Base64::btoa(okeyBin, oKeyB64);

                //Add entry to map
                userKeyMap->set(mMyHandle.toString().c_str(), oKeyB64.c_str());
                return api.call(&mega::MegaApi::createPublicChat, sdkPeers.get(), userKeyMap,
                                !enctitleB64.empty() ? enctitleB64.c_str() : nullptr, meeting, options, sm.get());
            });
        }
        else
        {
            createChatPromise = api.call(&mega::MegaApi::createChat, true, sdkPeers.get(),
                                         !enctitleB64.empty() ? enctitleB64.c_str() : nullptr, options, sm.get());
        }

        return createChatPromise
        .then([this, wptr](ReqResult result) -> Promise<std::pair<karere::Id, std::shared_ptr<KarereScheduledMeeting>>>
        {
            if (wptr.deleted())
                return ::promise::Error("Chat created successfully, but instance was removed");

            auto& list = *result->getMegaTextChatList();
            if (list.size() != 1)
                return ::promise::Error("Empty chat list returned from API");

            auto room = chats->addRoom(*list.get(0));
            if (!room || !room->isGroup())
                return ::promise::Error("API created incorrect group");

            room->connect();

            std::shared_ptr<KarereScheduledMeeting> sm;
            if (result->getMegaScheduledMeetingList() && result->getMegaScheduledMeetingList()->size() == 1)
            {
                sm.reset(new KarereScheduledMeeting(result->getMegaScheduledMeetingList()->at(0)));
            }
            return std::make_pair(karere::Id(room->chatid()), sm);
        });
     });
}

promise::Promise<void> GroupChatRoom::excludeMember(uint64_t userid)
{
    auto wptr = getDelTracker();
    return parent.mKarereClient.api.callIgnoreResult(&mega::MegaApi::removeFromChat, chatid(), userid)
    .then([this, wptr, userid]()
    {
        wptr.throwIfDeleted();
        if (removeMember(userid) && !mHasTitle)
        {
            makeTitleFromMemberNames();
        }
    });
}

ChatRoom::ChatRoom(ChatRoomList& aParent, const uint64_t& chatid, bool aIsGroup,
  unsigned char aShard, chatd::Priv aOwnPriv, int64_t ts, bool aIsArchived, const std::string& aTitle)
   :parent(aParent), mChatid(chatid),
    mShardNo(aShard), mIsGroup(aIsGroup),
    mOwnPriv(aOwnPriv), mCreationTs(ts), mIsArchived(aIsArchived), mTitleString(aTitle), mHasTitle(false)
{}

//chatd::Listener
void ChatRoom::onLastMessageTsUpdated(uint32_t ts)
{
    callAfterInit(this, [this, ts]()
    {
        auto display = roomGui();
        if (display)
            display->onLastTsUpdated(ts);
    }, parent.mKarereClient.appCtx);
}

ApiPromise ChatRoom::requestGrantAccess(mega::MegaNode *node, mega::MegaHandle userHandle)
{
    return parent.mKarereClient.api.call(&::mega::MegaApi::grantAccessInChat, chatid(), node, userHandle);
}

ApiPromise ChatRoom::requestRevokeAccess(mega::MegaNode *node, mega::MegaHandle userHandle)
{
    return parent.mKarereClient.api.call(&::mega::MegaApi::removeAccessInChat, chatid(), node, userHandle);
}

bool ChatRoom::isChatdChatInitialized()
{
    return mChat;
}

const char* ChatRoom::getLoggingName() const
{
    return parent.mKarereClient.getLoggingName();
}

strongvelope::ProtocolHandler* Client::newStrongvelope(const karere::Id& chatid, bool isPublic,
        std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted, const karere::Id& ph)
{
    return new strongvelope::ProtocolHandler(mMyHandle,
         StaticBuffer(mMyPrivCu25519, 32), StaticBuffer(mMyPrivEd25519, 32),
         *mUserAttrCache, db, chatid, isPublic, unifiedKey,
         isUnifiedKeyEncrypted, ph, appCtx);
}

void ChatRoom::createChatdChat(const karere::SetOfIds& initialUsers, bool isPublic,
        std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted, const karere::Id& ph)
{
    mChat = &parent.mKarereClient.mChatdClient->createChat(
        mChatid, mShardNo, this, initialUsers,
        parent.mKarereClient.newStrongvelope(mChatid, isPublic, unifiedKey, isUnifiedKeyEncrypted, ph),
        static_cast<uint32_t>(mCreationTs), mIsGroup);
}

template <class T, typename F>
void callAfterInit(T* self, F&& func, void *ctx)
{
    if (self->isInitializing())
    {
        auto wptr = self->weakHandle();
        marshallCall([wptr, func]()
        {
            if (!wptr.deleted())
                func();
        }, ctx);
    }
    else
    {
        func();
    }
}

void PeerChatRoom::initWithChatd()
{
    SetOfIds peers{parent.mKarereClient.myHandle()};
    if (mPeer)
    {
        peers.insert(karere::Id{mPeer});
    }
    createChatdChat(peers);
}

void PeerChatRoom::connect()
{
    mChat->connect();
}

promise::Promise<void> PeerChatRoom::requesGrantAccessToNodes(mega::MegaNodeList *nodes)
{
    std::vector<ApiPromise> promises;

    for (int i = 0; i < nodes->size(); ++i)
    {
        if (!parent.mKarereClient.api.sdk.hasAccessToAttachment(mChatid, nodes->get(i)->getHandle(), peer()))
        {
            ApiPromise promise = requestGrantAccess(nodes->get(i), peer());
            promises.push_back(promise);
        }
    }

    return promise::when(promises);
}

promise::Promise<void> PeerChatRoom::requestRevokeAccessToNode(mega::MegaNode *node)
{
    std::vector<ApiPromise> promises;

    mega::MegaHandleList *megaHandleList = parent.mKarereClient.api.sdk.getAttachmentAccess(mChatid, node->getHandle());

    for (unsigned int j = 0; j < megaHandleList->size(); ++j)
    {
        ApiPromise promise = requestRevokeAccess(node, peer());
        promises.push_back(promise);
    }

    delete megaHandleList;

    return promise::when(promises);
}

promise::Promise<void> GroupChatRoom::requesGrantAccessToNodes(mega::MegaNodeList *nodes)
{
    std::vector<ApiPromise> promises;

    for (int i = 0; i < nodes->size(); ++i)
    {
        if (publicChat())
        {
           if (!parent.mKarereClient.api.sdk.hasAccessToAttachment(mChatid, nodes->get(i)->getHandle(), Id::COMMANDER()))
           {
               ApiPromise promise = requestGrantAccess(nodes->get(i), Id::COMMANDER());
               promises.push_back(promise);
           }
        }
        else
        {
            for (auto it = mPeers.begin(); it != mPeers.end(); ++it)
            {
                if (!parent.mKarereClient.api.sdk.hasAccessToAttachment(mChatid, nodes->get(i)->getHandle(), it->second->mHandle))
                {
                    ApiPromise promise = requestGrantAccess(nodes->get(i), it->second->mHandle);
                    promises.push_back(promise);
                }
            }
        }
    }

    return promise::when(promises);
}

promise::Promise<void> GroupChatRoom::requestRevokeAccessToNode(mega::MegaNode *node)
{
    std::vector<ApiPromise> promises;

    mega::MegaHandleList *megaHandleList = parent.mKarereClient.api.sdk.getAttachmentAccess(mChatid, node->getHandle());

    for (unsigned int j = 0; j < megaHandleList->size(); ++j)
    {
        ApiPromise promise = requestRevokeAccess(node, megaHandleList->get(j));
        promises.push_back(promise);
    }

    delete megaHandleList;

    return promise::when(promises);
}

IApp::IGroupChatListItem* GroupChatRoom::addAppItem()
{
    auto list = parent.mKarereClient.app.chatListHandler();
    return list ? list->addGroupChatItem(*this) : nullptr;
}

//Create chat or receive an invitation
GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& aChat)
:ChatRoom(parent, aChat.getHandle(), true, static_cast<unsigned char>(aChat.getShard()),
  (chatd::Priv)aChat.getOwnPrivilege(), aChat.getCreationTime(), aChat.isArchived()),
  mRoomGui(nullptr), mMeeting(aChat.isMeeting()), mChatOptions(aChat.getChatOptions())
{
    assert(mChatOptions.isValid());
    bool isPublicChat = aChat.isPublicChat();
    // Save Chatroom into DB
    auto db = parent.mKarereClient.db;
    db.query("insert or replace into chats(chatid, shard, peer, peer_priv, "
             "own_priv, ts_created, archived, mode, meeting, chat_options) values(?,?,-1,0,?,?,?,?,?,?)",
             mChatid, mShardNo, mOwnPriv, aChat.getCreationTime(), aChat.isArchived(), isPublicChat, mMeeting, mChatOptions.value());
    db.query("delete from chat_peers where chatid=?", mChatid); // clean any obsolete data

    // Initialize list of peers and fetch their names
    auto peers = aChat.getPeerList();
    std::vector<promise::Promise<void>> promises;
    if (peers)
    {
        int numPeers = peers->size();
        for (int i = 0; i < numPeers; i++)
        {
            auto userid = peers->getPeerHandle(i);
            promise::Promise<void> nameResolvedPromise = addMember(userid, (chatd::Priv)peers->getPeerPrivilege(i), isPublicChat);
            if (promises.size() < MAX_NAMES_CHAT_WITHOUT_TITLE)
            {
                promises.push_back(nameResolvedPromise);
            }
        }
    }
    // If there is not any promise at vector promise, promise::when is resolved directly
    mMemberNamesResolved = promise::when(promises);

    // Initialize unified-key, if any (note private chats may also have unfied-key if user participated while chat was public)
    const char *unifiedKeyPtr = aChat.getUnifiedKey();
    assert(!(isPublicChat && !unifiedKeyPtr));
    std::shared_ptr<std::string> unifiedKey;
    int isUnifiedKeyEncrypted = strongvelope::kDecrypted;
    if (unifiedKeyPtr)
    {
        std::string unifiedKeyB64(unifiedKeyPtr);
        unifiedKey.reset(new std::string);
        int len = ::mega::Base64::atob(unifiedKeyB64, *unifiedKey);
        if (len != strongvelope::SVCRYPTO_KEY_SIZE + ::mega::MegaClient::USERHANDLE)
        {
            KR_LOG_ERROR("%sInvalid size for unified key", getLoggingName());
            isUnifiedKeyEncrypted = strongvelope::kUndecryptable;
            parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99002, "invalid unified-key detected", false, static_cast<const char*>(nullptr));
        }
        else
        {
            isUnifiedKeyEncrypted = strongvelope::kEncrypted;
        }

        // Save (still) encrypted unified key
        Buffer unifiedKeyBuf;
        unifiedKeyBuf.write(0, (uint8_t)isUnifiedKeyEncrypted);  // prefix to indicate it's encrypted
        unifiedKeyBuf.append(unifiedKey->data(), unifiedKey->size());
        db.query("update chats set unified_key = ? where chatid = ?", unifiedKeyBuf, mChatid);
    }

    // Initialize chatd::Client (and strongvelope)
    initWithChatd(isPublicChat, unifiedKey, isUnifiedKeyEncrypted);

    // Initialize title, if any
    std::string title = aChat.getTitle() ? aChat.getTitle() : "";
    initChatTitle(title, strongvelope::kEncrypted, true);

    mRoomGui = addAppItem();
    mIsInitializing = false;

    // Add scheduled meeting list and notify app
    addSchedMeetings(aChat.getScheduledMeetingList());

    if (aChat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_REPLACE_OCURR)
            || aChat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_APPEND_OCURR))
    {
        addSchedMeetingsOccurrences(aChat);
    }
}

//Resume from cache
GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid,
    unsigned char aShard, chatd::Priv aOwnPriv, int64_t ts, bool aIsArchived,
    const std::string& title, int isTitleEncrypted, bool publicChat, std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted, bool meeting, mega::ChatOptions_t options)
    : ChatRoom(parent, chatid, true, aShard, aOwnPriv, ts, aIsArchived)
    , mRoomGui(nullptr), mMeeting(meeting), mChatOptions(options)
{
    assert(mChatOptions.isValid());
    // Initialize list of peers
    SqliteStmt stmt(parent.mKarereClient.db, "select userid, priv from chat_peers where chatid=?");
    stmt << mChatid;
    std::vector<promise::Promise<void> > promises;
    while(stmt.step())
    {
        auto userid = stmt.integralCol<uint64_t>(0);
        promise::Promise<void> nameResolvedPromise = addMember(userid, stmt.integralCol<chatd::Priv>(1), publicChat, false);
        if (promises.size() < MAX_NAMES_CHAT_WITHOUT_TITLE)
        {
            promises.push_back(nameResolvedPromise);
        }
    }

    mMemberNamesResolved = promise::when(promises);

    // Initialize chatd::Client (and strongvelope)
    initWithChatd(publicChat, unifiedKey, isUnifiedKeyEncrypted);

    // Initialize title, if any
    initChatTitle(title, isTitleEncrypted);

    mRoomGui = addAppItem();
    mIsInitializing = false;

    // load scheduled meetings and scheduled meetings occurrences
    loadSchedMeetingsFromDb();
}

//Load chatLink
GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid,
    unsigned char aShard, chatd::Priv aOwnPriv, int64_t ts, bool aIsArchived, const std::string& title,
    const uint64_t publicHandle, std::shared_ptr<std::string> unifiedKey, bool meeting, const mega::ChatOptions_t options,
    const mega::MegaScheduledMeetingList* smList)
  : ChatRoom(parent, chatid, true, aShard, aOwnPriv, ts, aIsArchived, title)
    , mRoomGui(nullptr), mMeeting(meeting), mChatOptions(options)
{
    Buffer unifiedKeyBuf;
    unifiedKeyBuf.write(0, (uint8_t)strongvelope::kDecrypted);  // prefix to indicate it's decrypted
    unifiedKeyBuf.append(unifiedKey->data(), unifiedKey->size());
    parent.mKarereClient.setCommitMode(false);

    //save to db
    auto db = parent.mKarereClient.db;
    db.query(
        "insert or replace into chats(chatid, shard, peer, peer_priv, "
        "own_priv, ts_created, mode, unified_key, meeting) values(?,?,-1,0,?,?,2,?,?)",
        mChatid, mShardNo, mOwnPriv, mCreationTs, unifiedKeyBuf, mMeeting);

    initWithChatd(true, unifiedKey, 0, publicHandle); // strongvelope only needs the public handle in preview mode (to fetch user attributes via `mcuga`)
    mChat->setPublicHandle(publicHandle);   // chatd always need to know the public handle in preview mode (to send HANDLEJOIN)

    initChatTitle(title, strongvelope::kDecrypted, true);

    mRoomGui = addAppItem();
    mIsInitializing = false;
    addSchedMeetings(smList);
}

void GroupChatRoom::initWithChatd(bool isPublic, std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted, Id ph)
{
    karere::SetOfIds users;
    Id myHandle = parent.mKarereClient.myHandle();

    //Don't add my own handle in preview mode because previewers are not chat members
    if (myHandle != Id::null() && !ph.isValid())
    {
        users.insert(myHandle);
    }

    for (auto& peer: mPeers)
    {
        users.insert(peer.first);
    }

    createChatdChat(users, isPublic, unifiedKey, isUnifiedKeyEncrypted, ph);
}

void GroupChatRoom::connect()
{
    if (chat().onlineState() != chatd::kChatStateOffline)
        return;

    mChat->connect();
}

promise::Promise<void> GroupChatRoom::memberNamesResolved() const
{
    return mMemberNamesResolved;
}

IApp::IPeerChatListItem* PeerChatRoom::addAppItem()
{
    auto list = parent.mKarereClient.app.chatListHandler();
    return list ? list->addPeerChatItem(*this) : nullptr;
}

//Resume from cache
PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid,
    unsigned char aShard, chatd::Priv aOwnPriv, const uint64_t& peer,
    chatd::Priv peerPriv, int64_t ts, bool aIsArchived)
    :ChatRoom(parent, chatid, false, aShard, aOwnPriv, ts, aIsArchived),
    mPeer(peer),
    mPeerPriv(peerPriv),
    mRoomGui(nullptr)
{
    initContact(peer);
    initWithChatd();
    mRoomGui = addAppItem();
    mIsInitializing = false;
}

//Create chat or receive an invitation
PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat)
    :ChatRoom(parent, chat.getHandle(), false, static_cast<unsigned char>(chat.getShard()),
     (chatd::Priv)chat.getOwnPrivilege(), chat.getCreationTime(), chat.isArchived()),
      mPeer(getSdkRoomPeer(chat)), mPeerPriv(getSdkRoomPeerPriv(chat)), mRoomGui(nullptr)
{
    parent.mKarereClient.db.query("insert into chats(chatid, shard, peer, peer_priv, own_priv, ts_created, archived) values (?,?,?,?,?,?,?)",
        mChatid, mShardNo, mPeer, mPeerPriv, mOwnPriv, mCreationTs, mIsArchived);
//just in case
    parent.mKarereClient.db.query("delete from chat_peers where chatid = ?", mChatid);

    KR_LOG_DEBUG("%sAdded 1on1 chatroom '%s' from API", getLoggingName(), ID_CSTR(mChatid));

    initContact(mPeer);
    initWithChatd();
    mRoomGui = addAppItem();
    mIsInitializing = false;
}
PeerChatRoom::~PeerChatRoom()
{
    auto &client = parent.mKarereClient;
    if (!client.isTerminated())
    {
        client.userAttrCache().removeCb(mUsernameAttrCbId);

        if (mRoomGui)
        {
            client.app.chatListHandler()->removePeerChatItem(*mRoomGui);
        }
    }

    if (client.mChatdClient)
    {
        client.mChatdClient->leave(mChatid);
    }
}

void PeerChatRoom::initContact(const uint64_t& peer)
{
    if (!peer) // chat with self
    {
        mContact = nullptr;
        mEmail = parent.mKarereClient.myEmail();
        const auto& myName = parent.mKarereClient.myName();
        if (!myName.empty())
        {
            mTitleString.assign(myName.c_str() + 1, myName.size() - 1);
        }
        else
        {
            mTitleString = mEmail;
        }
        return;
    }
    mContact = parent.mKarereClient.mContactList->contactFromUserId(peer);
    mEmail = mContact ? mContact->email() : "Inactive account";
    if (mContact)
    {
        mContact->attachChatRoom(*this);
    }
    else    // 1on1 with ex-user
    {
        mUsernameAttrCbId = parent.mKarereClient.userAttrCache().
                getAttr(peer, USER_ATTR_FULLNAME, this,
        [](Buffer* data, void* userp)
        {
            //even if both first and last name are null, the data is at least
            //one byte - the firstname-size-prefix, which will be zero but
            //if lastname is not null the first byte will contain the
            //firstname-size-prefix but datasize will be bigger than 1 byte.

            // If the contact has alias don't update the title
            auto self = static_cast<PeerChatRoom*>(userp);
            std::string alias = self->parent.mKarereClient.getUserAlias(self->mPeer);
            if (alias.empty())
            {
                if (!data || data->empty() || (*data->buf() == 0 && data->size() == 1))
                {
                    self->updateTitle(self->mEmail);
                }
                else
                {
                    self->updateTitle(std::string(data->buf()+1, data->dataSize()-1));
                }
            }
        });

        if (mTitleString.empty()) // user attrib fetch was not synchronous
        {
            updateTitle(mEmail);
            assert(!mTitleString.empty());
        }
    }
}

void PeerChatRoom::updateChatRoomTitle()
{
    std::string title = parent.mKarereClient.getUserAlias(mPeer);
    if (title.empty())
    {
        title = mContact ? mContact->getContactName() : "";
        if (title.empty())
        {
            title = mEmail;
        }
    }

    if (mContact)
    {
        mContact->updateTitle(title);
    }
    else
    {
        updateTitle(title);
    }
}

bool PeerChatRoom::isMember(const Id& peerid) const
{
    return peerid == mPeer;
}

unsigned long PeerChatRoom::numMembers() const
{
    return mPeer ? 2 : 1;
}

uint64_t PeerChatRoom::getSdkRoomPeer(const ::mega::MegaTextChat& chat)
{
    auto peers = chat.getPeerList();
    if (!peers || peers->size() == 0)
    {
        return 0; // self chat
    }
    assert(peers->size() == 1);
    return peers->getPeerHandle(0);
}

chatd::Priv PeerChatRoom::getSdkRoomPeerPriv(const mega::MegaTextChat &chat)
{
    auto peers = chat.getPeerList();
    if (!peers)
    {
        return chatd::PRIV_INVALID;
    }
    assert(peers);
    assert(peers->size() == 1);
    return (chatd::Priv) peers->getPeerPrivilege(0);
}

bool ChatRoom::syncOwnPriv(chatd::Priv newPriv)
{
    if (!hasOwnPrivChanged(newPriv))
    {
        return false;
    }

    if (previewMode())
    {
        const bool validPrivInPreviewMode =
            mOwnPriv == chatd::PRIV_RO ||
            mOwnPriv == chatd::PRIV_RM; // still in preview, but ph is invalid

        if (!validPrivInPreviewMode)
        {
            KR_LOG_ERROR("%ssyncOwnPriv: invalid current privilege in preview mode for chat: ",
                         getLoggingName(),
                         karere::Id(chatid()).toString().c_str());
            assert(false);
        }

        if (newPriv >= chatd::PRIV_RO)
        {
            KR_LOG_DEBUG("%ssyncOwnPriv: invalidating ph as we are not in preview mode for chat: ",
                         getLoggingName(),
                         karere::Id(chatid()).toString().c_str());

            // Join
            mChat->setPublicHandle(Id::inval());

            // Remove preview mode flag from DB
            parent.mKarereClient.db.query("update chats set mode = '1' where chatid = ?", mChatid);
        }
    }

    mOwnPriv = newPriv;
    parent.mKarereClient.db.query("update chats set own_priv = ? where chatid = ?", mOwnPriv, mChatid);
    return true;
}

bool ChatRoom::syncArchive(bool aIsArchived)
{
    if (mIsArchived == aIsArchived)
        return false;

    mIsArchived = aIsArchived;
    parent.mKarereClient.db.query("update chats set archived = ? where chatid = ?", mIsArchived, mChatid);

    return true;
}

bool PeerChatRoom::syncPeerPriv(chatd::Priv priv)
{
    if (mPeerPriv == priv)
        return false;

    mPeerPriv = priv;
    parent.mKarereClient.db.query("update chats set peer_priv = ? where chatid = ?", mPeerPriv, mChatid);

    return true;
}

bool PeerChatRoom::syncWithApi(const mega::MegaTextChat &chat)
{
    auto peers = chat.getPeerList();
    if (isNoteToSelf() && (peers && peers->size() > 0))
    {
        KR_LOG_ERROR(
            "%ssyncWithApi: Asked to sync a self-chat with a chat from API with non-zero peers",
            getLoggingName(),
            karere::Id(chatid()).toString().c_str());
    }
    bool changed = syncArchive(chat.isArchived());
    if (changed)
    {
        mIsArchived = chat.isArchived();
        onArchivedChanged(mIsArchived);
    }
    if (mPeer)
    {
        changed |= syncOwnPriv((chatd::Priv)chat.getOwnPrivilege()); // true if own priv changed
        changed |= syncPeerPriv((chatd::Priv)peers->getPeerPrivilege(0));
    }
    return changed;
}

promise::Promise<void> GroupChatRoom::addMember(uint64_t userid, chatd::Priv priv, bool isPublicChat, bool saveToDb)
{
    assert(userid != parent.mKarereClient.myHandle());

    auto it = mPeers.find(userid);
    if (it != mPeers.end())
    {
        if (it->second->mPriv == priv)
        {
            saveToDb = false;
        }
        else
        {
            it->second->mPriv = priv;
        }
    }
    else
    {
        Member *member = new Member(*this, userid, priv);
        mPeers.emplace(userid, member); //usernames will be updated when the Member object gets the username attribute
        bool fetchIsRequired = !isPublicChat || mPeers.size() <= PRELOAD_CHATLINK_PARTICIPANTS;
        member->registerCallBacks(fetchIsRequired);
    }

    if (saveToDb)
    {
        parent.mKarereClient.db.query("insert or replace into chat_peers(chatid, userid, priv) values(?,?,?)",
            mChatid, userid, priv);
    }

    return mPeers[userid]->nameResolved();
}

bool GroupChatRoom::removeMember(uint64_t userid)
{
    auto it = mPeers.find(userid);
    if (it == mPeers.end())
    {
        KR_LOG_WARNING("%sGroupChatRoom::removeMember for a member that we don't have, ignoring",
                       getLoggingName());
        return false;
    }
    else
    {
        KR_LOG_DEBUG("%sGroupChatRoom[%s]: Removed member %s",
                     getLoggingName(),
                     ID_CSTR(mChatid),
                     ID_CSTR(userid));
    }

    delete it->second;
    mPeers.erase(it);
    parent.mKarereClient.db.query("delete from chat_peers where chatid=? and userid=?", mChatid, userid);
    return true;
}

promise::Promise<void> GroupChatRoom::setChatRoomOption(int option, bool enabled)
{
    auto wptr = getDelTracker();
    return parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::setChatOption, chatid(), option, enabled)
    .then([wptr]()
    {
        wptr.throwIfDeleted();
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        wptr.throwIfDeleted();
        KR_LOG_ERROR("%sError setting chatroom option for chat %s: %s",
                     getLoggingName(),
                     ID_CSTR(chatid()),
                     err.what());
        return err;
    });
}

promise::Promise<void> GroupChatRoom::setPrivilege(const karere::Id& userid, chatd::Priv priv)
{
    auto wptr = getDelTracker();
    return parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::updateChatPermissions, chatid(), userid.val, priv)
    .then([this, wptr, userid, priv]()
    {
        wptr.throwIfDeleted();
        if (userid == parent.mKarereClient.myHandle())
        {
            parent.mKarereClient.db.query("update chats set own_priv=? where chatid=?", priv, mChatid);
        }
        else
        {
            parent.mKarereClient.db.query("update chat_peers set priv=? where chatid=? and userid=?", priv, mChatid, userid);
        }
    });
}

promise::Promise<void> ChatRoom::truncateHistory(const karere::Id& msgId)
{
    auto wptr = getDelTracker();
    return parent.mKarereClient.api.callIgnoreResult(
                &::mega::MegaApi::truncateChat,
                chatid(),
                msgId)
    .then([wptr]()
    {
        wptr.throwIfDeleted();
        // TODO: update indexes, last message and so on
    });
}

bool ChatRoom::isCallActive() const
{
    return parent.mKarereClient.isCallActive(mChatid);
}

promise::Promise<void> ChatRoom::archiveChat(bool archive)
{
    auto wptr = getDelTracker();
    return parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::archiveChat, chatid(), archive)
    .then([this, wptr, archive]()
    {
        wptr.throwIfDeleted();

        bool archiveChanged = syncArchive(archive);
        if (archiveChanged)
        {
            onArchivedChanged(archive);
        }
    });
}

promise::Promise<void> ChatRoom::setChatRetentionTime(unsigned period)
{
    return parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::setChatRetentionTime, chatid(), period);
}

void GroupChatRoom::deleteSelf()
{
    //have to post a delete on the event loop, as there may be pending
    //events related to the chatroom/strongvelope instance
    auto wptr = weakHandle();
    marshallCall([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }
        delete this;
    }, parent.mKarereClient.appCtx);
}

ChatRoomList::ChatRoomList(Client& aClient)
:mKarereClient(aClient)
{}

void ChatRoomList::loadFromDb()
{
    auto db = mKarereClient.db;

    //We need to ensure that the DB does not contain any record related with a preview
    SqliteStmt stmtPreviews(db, "select chatid from chats where mode = '2'");
    while(stmtPreviews.step())
    {
        Id chatid = stmtPreviews.integralCol<uint64_t>(0);
        deleteRoomFromDb(chatid);
    }

    SqliteStmt stmt(db, "select chatid, ts_created ,shard, own_priv, peer, peer_priv, title, archived, mode, unified_key, meeting, chat_options from chats");
    while(stmt.step())
    {
        auto chatid = stmt.integralCol<uint64_t>(0);
        if (find(chatid) != end())
        {
            KR_LOG_WARNING("%sChatRoomList: Attempted to load from db cache a chatid that is "
                           "already in memory",
                           mKarereClient.getLoggingName());
            continue;
        }
        auto peer = stmt.integralCol<uint64_t>(4);
        ChatRoom* room;
        if (peer != uint64_t(-1))
        {
            auto peerRoom = new PeerChatRoom(*this,
                                             chatid,
                                             stmt.integralCol<unsigned char>(2),
                                             stmt.integralCol<chatd::Priv>(3),
                                             peer,
                                             stmt.integralCol<chatd::Priv>(5),
                                             stmt.integralCol<int>(1),
                                             stmt.integralCol<int>(7));
            room = peerRoom;
            if (peerRoom->isNoteToSelf())
            {
                mSelfChat = peerRoom;
            }
        }
        else
        {
            std::shared_ptr<std::string> unifiedKey;
            int isUnifiedKeyEncrypted = strongvelope::kDecrypted;

            Buffer unifiedKeyBuf;
            stmt.blobCol(9, unifiedKeyBuf);
            if (!unifiedKeyBuf.empty())
            {
                const char *pos = unifiedKeyBuf.buf();
                isUnifiedKeyEncrypted = (uint8_t)*pos;  pos++;
                size_t len = unifiedKeyBuf.size() - 1;
                assert( (isUnifiedKeyEncrypted == strongvelope::kDecrypted && len == 16)
                        || (isUnifiedKeyEncrypted == strongvelope::kEncrypted && len == 24)  // encrypted version includes invitor's userhandle (8 bytes)
                        || (isUnifiedKeyEncrypted));
                unifiedKey.reset(new std::string(pos, len));
            }

            // Get title and check if it's encrypted or not
            std::string auxTitle;
            int isTitleEncrypted = strongvelope::kDecrypted;

            Buffer titleBuf;
            stmt.blobCol(6, titleBuf);
            if (!titleBuf.empty())
            {
                const char *posTitle = titleBuf.buf();
                isTitleEncrypted = (uint8_t)*posTitle;  posTitle++;
                size_t len = titleBuf.size() - 1;
                auxTitle.assign(posTitle, len);
            }

            room = new GroupChatRoom(*this, chatid, stmt.integralCol<unsigned char>(2), stmt.integralCol<chatd::Priv>(3), stmt.integralCol<int>(1), stmt.integralCol<int>(7), auxTitle, isTitleEncrypted, stmt.integralCol<int>(8), unifiedKey, isUnifiedKeyEncrypted, stmt.integralCol<int>(10), stmt.integralCol<mega::ChatOptions_t>(11));
        }
        emplace(chatid, room);
    }
}

void ChatRoomList::addMissingRoomsFromApi(const mega::MegaTextChatList& rooms, SetOfIds& chatids)
{
    auto size = rooms.size();
    for (int i = 0; i < size; i++)
    {
        auto& apiRoom = *rooms.get(i);
        auto chatid = apiRoom.getHandle();
        auto it = find(chatid);
        if (it != end())
            continue;   // chatroom already known

        ChatRoom* room = addRoom(apiRoom);
        chatids.insert(chatid);

        if (mKarereClient.connected())
        {
            KR_LOG_DEBUG("%s...connecting new room to chatd...", mKarereClient.getLoggingName());
            room->connect();
        }
        else
        {
            KR_LOG_DEBUG("%s...client is not connected, not connecting new room",
                         mKarereClient.getLoggingName());
        }
    }
}

ChatRoom* ChatRoomList::addRoom(const mega::MegaTextChat& apiRoom)
{
    auto chatid = apiRoom.getHandle();
    ChatRoom* room;
    if(apiRoom.isGroup())
    {
        //We need to ensure that unified key exists before decrypt title for public chats. So we decrypt title inside ctor
        room = new GroupChatRoom(*this, apiRoom); //also writes it to cache
    }
    else    // 1on1
    {
        auto peerRoom = new PeerChatRoom(*this, apiRoom);
        room = peerRoom;
        if (peerRoom->isNoteToSelf()) // chat with self
        {
            mSelfChat = peerRoom;
        }
    }

#ifndef NDEBUG
    auto ret =
#endif
    emplace(chatid, room);
    assert(ret.second); //we should not have that room
    return room;
}

void ChatRoom::notifyOwnExcludedFromChat()
{
    if (mAppChatHandler)
        mAppChatHandler->onExcludedFromChat();
    auto listItem = roomGui();
    if (listItem)
        listItem->onExcludedFromChat();
}

void ChatRoom::notifyRejoinedChat()
{
    if (mAppChatHandler)
        mAppChatHandler->onRejoinedChat();
    auto listItem = roomGui();
    if (listItem)
        listItem->onRejoinedChat();
}

void ChatRoomList::removeRoomPreview(Id chatid)
{
    auto it = find(chatid);
    if (it == end())
    {
        CHATD_LOG_WARNING("%sremoveRoomPreview: room not in chat list",
                          mKarereClient.getLoggingName());
        return;
    }
    if (!it->second->previewMode())
    {
        CHATD_LOG_WARNING("%sremoveRoomPreview: room is not a preview",
                          mKarereClient.getLoggingName());
        return;
    }

    GroupChatRoom *groupchat = (GroupChatRoom*)it->second;
    groupchat->notifyPreviewClosed();
    erase(it);
    delete groupchat;
}

void ChatRoomList::removeRoomPreviewMarshall(Id chatid)
{
    auto wptr = mKarereClient.weakHandle();
    marshallCall([wptr, this, chatid]()
    {
        if (wptr.deleted())
        {
            return;
        }
        removeRoomPreview(chatid);
    },mKarereClient.appCtx);
}

void GroupChatRoom::notifyPreviewClosed()
{
    auto listItem = roomGui();
    if (listItem)
        listItem->onPreviewClosed();
}

ScheduledMeetingHandler& GroupChatRoom::schedMeetingHandler()
{
    return parent.mKarereClient.mScheduledMeetingHandler;
}

DbClientInterface& GroupChatRoom::getClientDbInterface()
{
    return parent.mKarereClient.getClientDbInterface();
}

void GroupChatRoom::notifySchedMeetingUpdated(const KarereScheduledMeeting* sm, unsigned long changed)
{
    std::shared_ptr<KarereScheduledMeeting> auxSm(sm->copy());
    callAfterInit(this, [this, auxSm, changed]
    {
        schedMeetingHandler().onSchedMeetingChange(auxSm.get(), changed);
    }, parent.mKarereClient.appCtx);
}

void GroupChatRoom::notifySchedMeetingOccurrencesUpdated(bool append)
{
    callAfterInit(this, [this, append]
    {
       schedMeetingHandler().onSchedMeetingOccurrencesChange(chatid(), append);
    }, parent.mKarereClient.appCtx);
}

void GroupChatRoom::notifyOwnUserPrivChange()
{
    if (mRoomGui)
    {
        mRoomGui->onUserJoin(parent.mKarereClient.myHandle(), mOwnPriv);
    }
}

void GroupChatRoom::updateTitleFromMemberNames(const UserPrivMap& users, bool& peersChanged)
{
    std::vector<promise::Promise<void>> promises;
    for (auto& user: users)
    {
        if (mPeers.find(user.first) == mPeers.end())
        {
            peersChanged = true;
            promise::Promise<void> promise = addMember(user.first, user.second, publicChat());
            if (promises.size() < MAX_NAMES_CHAT_WITHOUT_TITLE)
            {
                promises.push_back(promise);
            }
        }
    }

    if (peersChanged)
    {
        auto wptr = weakHandle();
        promise::when(promises).then(
            [wptr, this]()
            {
                wptr.throwIfDeleted();
                if (!mHasTitle)
                {
                    makeTitleFromMemberNames();
                }
            });
    }
}

void GroupChatRoom::rejoinChatOwnUser()
{
    // in case chat-link was invalidated during preview, the room was disabled
    // now, we upgrade from (invalid) previewer to participant --> enable it back
    if (mChat->isDisabled())
    {
        KR_LOG_WARNING("%sEnable chatroom previously in preview mode", getLoggingName());
        mChat->disable(false);
    }

    // if already connected, need to send a new JOIN to chatd
    if (parent.mKarereClient.connected())
    {
        KR_LOG_DEBUG("%sConnecting existing room to chatd after re-join...", getLoggingName());
        if (mChat->onlineState() < ::chatd::ChatState::kChatStateJoining)
        {
            mChat->connect();
        }
        else
        {
            KR_LOG_DEBUG("%sSkip re-join chatd, since it's already joining right now",
                         getLoggingName());
            parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99003, "Skip re-join chatd", false, static_cast<const char*>(nullptr));
        }
    }
}

void GroupChatRoom::setOwnUserRemoved()
{
    if (syncOwnPriv(chatd::PRIV_RM))
    {
        notifyOwnExcludedFromChat();
    }
}

void Client::onChatsUpdate(::mega::MegaApi*, ::mega::MegaTextChatList* rooms)
{
    if (!rooms)
    {
        const char *scsn = api.sdk.getSequenceNumber();
        KR_LOG_DEBUG("%sChatrooms up to date with API. scsn: %s", getLoggingName(), scsn);
        delete [] scsn;
        return;
    }

    std::shared_ptr<mega::MegaTextChatList> copy(rooms->copy());
#ifndef NDEBUG
    dumpChatrooms(*copy);
#endif
    auto wptr = weakHandle();
    marshallCall([wptr, this, copy]()
    {
        if (wptr.deleted())
        {
            return;
        }
        if (!db.isOpen())
        {
            // This is something that should never happen. Continuing from here
            // will lead to runtime exception & crash.
            // Possible cause is incorrect teardown of the app. Required order:
            // 1. logout megaApi;
            // 2. logout megaChatApi;
            // 3. delete megaChatApi;
            // 4. delete megaApi.
            CHATD_LOG_ERROR("%slambda in marshallCall(): db was closed, cannot notify chats",
                            getLoggingName());
            assert(db.isOpen());
            return;
        }

        chats->onChatsUpdate(*copy);
    }, appCtx);
}

void ChatRoomList::onChatsUpdate(::mega::MegaTextChatList& rooms, bool checkDeleted)
{
    SetOfIds added; // out-param: records the new rooms added to the list
    addMissingRoomsFromApi(rooms, added);
    auto count = rooms.size();
    for (int i = 0; i < count; i++)
    {
        const ::mega::MegaTextChat *apiRoom = rooms.get(i);
        ::mega::MegaHandle chatid = apiRoom->getHandle();
        if (added.has(chatid)) //room was just added, no need to sync
            continue;

        ChatRoom *room = at(chatid);
        room->syncWithApi(*apiRoom);
    }

    if (checkDeleted)   // true only when list of rooms is complete, not for partial updates
    {
        SetOfIds removed;
        for (auto &room : *this)
        {
            bool deleted = true;
            for (int i = 0; i < rooms.size(); i++)
            {
                if (rooms.get(i)->getHandle() == room.first)
                {
                    deleted = false;
                    break;
                }
            }
            if (deleted)
            {
                removed.insert(room.first);
            }
        }
        for (auto &chatid : removed)
        {
            auto it = find(chatid);
            ChatRoom *chatroom = it->second;

            // notfiy deleted chat
            auto listItem = chatroom->roomGui();
            if (listItem)
                listItem->onChatDeleted();

            // delete from the list, from RAM and from DB
            erase(it);
            delete chatroom;
            deleteRoomFromDb(chatid);
        }
    }
}

ChatRoomList::~ChatRoomList()
{
    for (auto& room: *this)
        delete room.second;
}

promise::Promise<void> GroupChatRoom::decryptTitle()
{
    assert(!mEncryptedTitle.empty());

    Buffer buf(mEncryptedTitle.size());
    try
    {
        size_t decLen = base64urldecode(mEncryptedTitle.c_str(), mEncryptedTitle.size(), buf.buf(), buf.bufSize());
        buf.setDataSize(decLen);
    }
    catch(std::exception& e)
    {
        KR_LOG_ERROR(
            "%sFailed to base64-decode chat title for chat %s: %s. Falling back to member names",
            getLoggingName(),
            ID_CSTR(mChatid),
            e.what());

        parent.mKarereClient.api.call(&mega::MegaApi::sendEvent, 99007, "Decryption of chat topic failed", false, static_cast<const char*>(nullptr));
        updateTitleInDb(mEncryptedTitle, strongvelope::kUndecryptable);
        makeTitleFromMemberNames();

        return ::promise::Error(e.what());
    }

    auto wptr = getDelTracker();
    promise::Promise<std::string> pms = chat().crypto()->decryptChatTitleFromApi(buf);
    return pms.then([wptr, this](const std::string title)
    {
        wptr.throwIfDeleted();

        // Update title (also in cache) and notify that has changed
        handleTitleChange(title, true);
    })
    .fail([wptr, this](const ::promise::Error& err)
    {
        wptr.throwIfDeleted();

        KR_LOG_ERROR("%sError decrypting chat title for chat %s: %s. Falling back to member names.",
                     getLoggingName(),
                     ID_CSTR(chatid()),
                     err.what());

        parent.mKarereClient.api.call(&mega::MegaApi::sendEvent, 99007, "Decryption of chat topic failed", false, static_cast<const char*>(nullptr));
        updateTitleInDb(mEncryptedTitle, strongvelope::kUndecryptable);
        makeTitleFromMemberNames();

        return err;
    });
}

void GroupChatRoom::updateTitleInDb(const std::string &title, int isEncrypted)
{
    KR_LOG_DEBUG("%sTitle update in cache", getLoggingName());
    Buffer titleBuf;
    titleBuf.write(0, (uint8_t)isEncrypted);
    titleBuf.append(title.data(), title.size());
    parent.mKarereClient.db.query("update chats set title=? where chatid=?", titleBuf, mChatid);
}

void GroupChatRoom::makeTitleFromMemberNames()
{
    mHasTitle = false;
    std::string newTitle;
    if (mPeers.empty())
    {
        time_t ts = mCreationTs;
        const struct tm *time = localtime(&ts);
        char date[18];
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", time);
        newTitle = "Chat created on ";
        newTitle.append(date);
    }
    else
    {
        unsigned int numMemberNames = 0;
        for (auto& m: mPeers)
        {
            Id userid = m.first;
            const Member *user = m.second;

            std::string alias = parent.mKarereClient.getUserAlias(userid);
            if (!alias.empty())
            {
                // Add user's alias to the title
                newTitle.append(alias).append(", ");
            }
            else
            {
                //name has binary layout
                auto& name = user->mName;
                assert(!name.empty()); //is initialized to '\0', so is never empty

                if (name.size() > 1)
                {
                    int firstnameLen = name.at(0);
                    if (firstnameLen)
                    {
                        // Add user's first name to the title
                        newTitle.append(name.substr(1, firstnameLen)).append(", ");
                    }
                    else
                    {
                        // Add user's last name to the title
                        newTitle.append(name.substr(1)).append(", ");
                    }
                }
                else
                {
                    // Add user's email to the title
                    auto& email = user->mEmail;
                    if (!email.empty())
                        newTitle.append(email).append(", ");
                    else
                        newTitle.append("..., ");
                }
            }

            numMemberNames++;
            if (numMemberNames == MAX_NAMES_CHAT_WITHOUT_TITLE)
            {
                break;
            }
        }

        newTitle.resize(newTitle.size()-2); //truncate last ", "
    }
    assert(!newTitle.empty());
    if (newTitle == mTitleString)
    {
        KR_LOG_DEBUG("%smakeTitleFromMemberNames: same title than existing one, skipping update",
                     getLoggingName());
        return;
    }

    mTitleString = newTitle;
    notifyTitleChanged();
}

promise::Promise<std::shared_ptr<Buffer>> GroupChatRoom::encryptChatTitle(const std::string& title)
{
    return chat().crypto()->encryptChatTitle(title);
}

promise::Promise<void> GroupChatRoom::setTitle(const std::string& title)
{
    auto wptr = getDelTracker();
    return chat().crypto()->encryptChatTitle(title)
    .then([wptr, this](const std::shared_ptr<Buffer>& buf)
    {
        wptr.throwIfDeleted();
        auto b64 = base64urlencode(buf->buf(), buf->dataSize());
        return parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::setChatTitle, chatid(),
            b64.c_str());
    })
    .then([wptr, this, title]()
    {
        wptr.throwIfDeleted();
        if (title.empty())
        {
            clearTitle();
        }
    });
}

GroupChatRoom::~GroupChatRoom()
{
    removeAppChatHandler();

    if (mRoomGui && !parent.mKarereClient.isTerminated())
    {
        parent.mKarereClient.app.chatListHandler()->removeGroupChatItem(*mRoomGui);
    }

    if (previewMode())
    {
        parent.deleteRoomFromDb(mChatid);
    }

    if (parent.mKarereClient.mChatdClient)
    {
        parent.mKarereClient.mChatdClient->leave(mChatid);
    }

    for (auto& m: mPeers)
    {
        delete m.second;
    }
}

promise::Promise<void> GroupChatRoom::leave()
{
    auto wptr = getDelTracker();

    return parent.mKarereClient.api
        .callIgnoreResult(&mega::MegaApi::removeFromChat, mChatid, mega::INVALID_HANDLE)
        .fail(
            [wptr](const ::promise::Error& err) -> Promise<void>
            {
                wptr.throwIfDeleted();
                if (err.code() ==
                    ::mega::MegaError::API_EARGS) // room does not actually exist on API, ignore
                                                  // room and remove it locally
                    return promise::_Void();
                else
                    return err;
            })
        .then(
            [this, wptr]()
            {
                wptr.throwIfDeleted();
                setOwnUserRemoved();
            });
}

promise::Promise<void> GroupChatRoom::invite(uint64_t userid, chatd::Priv priv)
{
    auto wptr = getDelTracker();

    //Add new user to strongvelope set of users
    promise::Promise<std::string> pms;
    if (mHasTitle && !publicChat())
    {
        pms = chat().crypto()->encryptChatTitle(mTitleString, userid)
        .then([](const std::shared_ptr<Buffer>& buf)
        {
            return base64urlencode(buf->buf(), buf->dataSize());
        });
    }
    else
    {
        pms = promise::Promise<std::string>(std::string());
    }

    return pms
    .then([this, wptr, userid, priv](const std::string& title)
    {
        wptr.throwIfDeleted();
        ApiPromise invitePms;
        if (publicChat())
        {
            invitePms = chat().crypto()->encryptUnifiedKeyForAllParticipants(userid)
            .then([wptr, this, userid, priv](chatd::KeyCommand* encKey)
             {
                //Get peer unified key
                auto useruk = encKey->getKeyByUserId(userid);

                //Get creator handle in binary
                uint64_t invitorHandle = chat().client().mKarereClient->myHandle().val;

                //Append [invitorhandle+uk]
                std::string uKeyBin((const char*)&invitorHandle, sizeof(invitorHandle));
                uKeyBin.append(useruk->buf(), useruk->size());

                //Encode [invitorhandle+uk] to B64
                std::string uKeyB64;
                mega::Base64::btoa(uKeyBin, uKeyB64);

                return parent.mKarereClient.api.call(&mega::MegaApi::inviteToPublicChat, mChatid, userid, priv,
                    uKeyB64.c_str());
             });
        }
        else
        {
            invitePms = parent.mKarereClient.api.call(&mega::MegaApi::inviteToChat, mChatid, userid, priv,
                title.empty() ? nullptr : title.c_str());
        }
        return invitePms
        .then([this, wptr, userid, priv](ReqResult)
        {
            wptr.throwIfDeleted();
            addMember(userid, priv, publicChat())
            .then([wptr, this]()
            {
                wptr.throwIfDeleted();
                if (!mHasTitle)
                {
                    makeTitleFromMemberNames();
                }
            });
        });
    });
}

promise::Promise<void> GroupChatRoom::autojoinPublicChat(uint64_t ph)
{
    Id myHandle(parent.mKarereClient.myHandle());
    mAutoJoining = true;

    auto wptr = weakHandle();
    return chat()
        .crypto()
        ->encryptUnifiedKeyToUser(myHandle)
        .then(
            [this, myHandle, wptr, ph](std::string key) -> ApiPromise
            {
                if (wptr.deleted())
                {
                    return ::promise::Error("autojoinPublicChat: wptr deleted");
                }
                // Append [invitorhandle+uk]
                std::string uKeyBin((const char*)&myHandle, sizeof(myHandle.val));
                uKeyBin.append(key.data(), key.size());

                // Encode [invitorhandle+uk] to B64
                std::string uKeyB64;
                mega::Base64::btoa(uKeyBin, uKeyB64);

                parent.mKarereClient.setCommitMode(false);
                return parent.mKarereClient.api.call(&mega::MegaApi::chatLinkJoin,
                                                     ph,
                                                     uKeyB64.c_str());
            })
        .then(
            [this, wptr, myHandle](ReqResult)
            {
                if (wptr.deleted())
                {
                    return;
                }
                onUserJoin(parent.mKarereClient.myHandle(), chatd::PRIV_STANDARD);
            })
        .fail(
            [this, wptr](const ::promise::Error&)
            {
                if (wptr.deleted())
                {
                    return;
                }
                mAutoJoining = false;
            });
 }

//chatd::Listener::init
void ChatRoom::init(chatd::Chat& chat, chatd::DbInterface*& dbIntf)
{
    mChat = &chat;
    dbIntf = new ChatdSqliteDb(*mChat, parent.mKarereClient.db);
    if (mAppChatHandler)
    {
        KR_LOG_WARNING("%sApp chat handler is already set, remove it first", getLoggingName());
        assert(!mAppChatHandler); // keep original behavior in case this happens (it shouldn't)
        setAppChatHandler(mAppChatHandler);
    }
}

bool ChatRoom::setAppChatHandler(IApp::IChatHandler* handler)
{
    if (mAppChatHandler)
    {
        KR_LOG_WARNING("%sApp chat handler is already set, remove it first", getLoggingName());
        assert(!mAppChatHandler);
        return false;
    }

    mAppChatHandler = handler;
    chatd::DbInterface* dummyIntf = nullptr;
// mAppChatHandler->init() may rely on some events, so we need to set mChatWindow as listener before
// calling init(). This is safe, as and we will not get any async events before we
//return to the event loop
    mChat->setListener(mAppChatHandler);
    mAppChatHandler->init(*mChat, dummyIntf);
    return true;
}

void ChatRoom::removeAppChatHandler()
{
    if (!mAppChatHandler)
        return;
    mAppChatHandler = nullptr;
    mChat->setListener(this);
}

bool ChatRoom::hasChatHandler() const
{
    return mAppChatHandler != NULL;
}

void GroupChatRoom::onUserJoin(Id userid, chatd::Priv privilege)
{
    if (bool isOwnUser = userid == parent.mKarereClient.myHandle(); isOwnUser)
    {
        if (!syncOwnPriv(privilege)) // There isn't change for own privilege, avoid to call 'onUserJoin'
        {
            return;
        }
    }
    else
    {
        auto it = mPeers.find(userid);
        if (bool peerPrivNotChanged = it != mPeers.end() && it->second->mPriv == privilege; peerPrivNotChanged)
        {
            return;
        }

        auto wptr = weakHandle();
        addMember(userid, privilege, publicChat())
        .then([wptr, this]()
        {
            wptr.throwIfDeleted();
            if (!mHasTitle)
            {
                makeTitleFromMemberNames();
            }
        });
    }

    // Notify apps about user priv change
    if (mRoomGui)
    {
        mRoomGui->onUserJoin(userid, privilege);
    }
}

void GroupChatRoom::onUserLeave(Id userid)
{
    if (userid == Id::null())
    {
        if (!previewMode())
        {
            assert(false);
            return;
        }

        // preview is not allowed anymore, notify the user and clean cache
        setOwnUserRemoved();
    }
    else if (userid == parent.mKarereClient.myHandle())
    {
        setOwnUserRemoved();
    }
    else
    {
        if (removeMember(userid) && !mHasTitle)
        {
            makeTitleFromMemberNames();
        }

        if (mRoomGui)
            mRoomGui->onUserLeave(userid);
    }
}

void PeerChatRoom::onUserJoin(Id userid, chatd::Priv privilege)
{
    if (userid == parent.mKarereClient.myHandle())
        syncOwnPriv(privilege);
    else if (userid.val == mPeer)
        syncPeerPriv(privilege);
    else
        KR_LOG_ERROR(
            "%sPeerChatRoom: Bug: Received JOIN event from chatd for a third user, ignoring",
            getLoggingName());
}
void PeerChatRoom::onUserLeave(Id userid)
{
    KR_LOG_ERROR("%sPeerChatRoom: Bug: Received leave event for user %s from chatd on a permanent "
                 "chat, ignoring",
                 getLoggingName(),
                 ID_CSTR(userid));
}

void ChatRoom::onLastTextMessageUpdated(const chatd::LastTextMsg& msg)
{
    if (mIsInitializing)
    {
        auto wptr = weakHandle();
        marshallCall([=]()
        {
            if (wptr.deleted())
                return;
            auto display = roomGui();
            if (display)
                display->onLastMessageUpdated(msg);
        }, parent.mKarereClient.appCtx);
    }
    else
    {
        auto display = roomGui();
        if (display)
           display->onLastMessageUpdated(msg);
    }
}

//chatd notification
void ChatRoom::onOnlineStateChange(chatd::ChatState state)
{
    auto display = roomGui();
    if (display)
    {
        display->onChatOnlineState(state);
    }
}

void ChatRoom::onMsgOrderVerificationFail(const chatd::Message &msg, chatd::Idx idx, const std::string &errmsg)
{
    KR_LOG_ERROR("%smsgOrderFail[chatid: %s, msgid %s, idx %d, userid %s]: %s",
                 getLoggingName(),
                 ID_CSTR(mChatid),
                 ID_CSTR(msg.id()),
                 idx,
                 ID_CSTR(msg.userid),
                 errmsg.c_str());
}

void ChatRoom::onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status)
{
    // truncate can be received as NEWMSG when the `msgid` is new for the client (later on the MSGUPD is also received)
    if ( (msg.type == chatd::Message::kMsgTruncate)   // truncate received from a peer or from myself in another client
         || (msg.userid != parent.mKarereClient.myHandle() && status == chatd::Message::kNotSeen) )  // new (unseen) message received from a peer
    {
        parent.mKarereClient.app.onChatNotification(mChatid, msg, status, idx);
    }

    if (msg.type == chatd::Message::kMsgChatTitle)
    {
        std::string title(msg.buf(), msg.size());

        // Update title and notify that has changed
        ((GroupChatRoom *) this)->handleTitleChange(title);
    }
}

void GroupChatRoom::handleTitleChange(const std::string &title, bool saveToDB)
{
    if (saveToDB)
    {
        updateTitleInDb(title, strongvelope::kDecrypted);
    }

    if (mTitleString == title)
    {
        KR_LOG_DEBUG("%sSame title has already been notified, skipping update", getLoggingName());
        return;
    }

    mTitleString = title;
    mHasTitle = true;

    notifyTitleChanged();
}

bool GroupChatRoom::isMember(const Id& peerid) const
{
    return mPeers.find(peerid.val) != mPeers.end();
}

unsigned long GroupChatRoom::numMembers() const
{
    return static_cast<unsigned long>(mPeers.size() + 1);
}

bool GroupChatRoom::isMeeting() const
{
    return mMeeting;
}

bool GroupChatRoom::isOpenInvite() const
{
    return mChatOptions.openInvite();
}

bool GroupChatRoom::isSpeakRequest() const
{
    return mChatOptions.speakRequest();
}

bool GroupChatRoom::isWaitingRoom() const
{
    return mChatOptions.waitingRoom();
}

void ChatRoom::onMessageEdited(const chatd::Message& msg, chatd::Idx idx)
{
    chatd::Message::Status status = mChat->getMsgStatus(msg, idx);

    if ( (msg.type == chatd::Message::kMsgTruncate) // truncate received from a peer or from myself in another client
         || (msg.userid != parent.mKarereClient.myHandle() && status == chatd::Message::kNotSeen) )    // received message from a peer, still unseen, was edited / deleted
    {
        parent.mKarereClient.app.onChatNotification(mChatid, msg, status, idx);
    }
}

void ChatRoom::onMessageStatusChange(chatd::Idx idx, chatd::Message::Status status, const chatd::Message& msg)
{
    if (msg.userid != parent.mKarereClient.myHandle()
            && status == chatd::Message::kSeen)  // received message from a peer changed to seen
    {
        parent.mKarereClient.app.onChatNotification(mChatid, msg, status, idx);
    }
}

void ChatRoom::onUnreadChanged()
{
    IApp::IChatListItem *room = roomGui();
    if (room)
    {
        room->onUnreadCountChanged();
    }
}

void ChatRoom::onPreviewersUpdate()
{
    IApp::IChatListItem *room = roomGui();
    if (room)
    {
        room->onPreviewersCountUpdate(mChat->getNumPreviewers());
    }
}

void ChatRoom::onArchivedChanged(bool archived)
{
    IApp::IChatListItem *room = roomGui();
    if (room)
    {
        room->onChatArchived(archived);
    }
    if (mAppChatHandler)
    {
        mAppChatHandler->onChatArchived(archived);
    }

    // since the archived rooms don't count for the chats with unread messages,
    // we need to notifiy the apps about the changes on unread messages.
    onUnreadChanged();
}

void PeerChatRoom::updateTitle(const std::string& title)
{
    mTitleString = title;
    notifyTitleChanged();
}

void ChatRoom::notifyTitleChanged()
{
    callAfterInit(this, [this]
    {
        auto display = roomGui();
        if (display)
            display->onTitleChanged(mTitleString);

        if (mAppChatHandler)
            mAppChatHandler->onTitleChanged(mTitleString);
    }, parent.mKarereClient.appCtx);
}

void ChatRoom::notifyChatModeChanged()
{
    callAfterInit(this, [this]
    {
        auto display = roomGui();
        if (display)
            display->onChatModeChanged(this->publicChat());

        if (mAppChatHandler)
            mAppChatHandler->onChatModeChanged(this->publicChat());
    }, parent.mKarereClient.appCtx);
}

void ChatRoom::notifyChatOptionsChanged(int option)
{
    callAfterInit(this, [this, option]
    {
        // currently not needed to notify apps via onChatListItemUpdate
        if (mAppChatHandler)
        {
            mAppChatHandler->onChatOptionsChanged(option);
        }
    }, parent.mKarereClient.appCtx);
}

void GroupChatRoom::enablePreview(uint64_t ph)
{
    // Current priv is PRIV_RM and need to be updated
    mOwnPriv = chatd::PRIV_RO;
    parent.mKarereClient.db.query("update chats set own_priv = ? where chatid = ?", mOwnPriv, mChatid);
    if (mRoomGui)
    {
        mRoomGui->onUserJoin(parent.mKarereClient.myHandle(), mOwnPriv);
    }

    mChat->setPublicHandle(ph);
    chat().disable(false);
    connect();
}

bool GroupChatRoom::publicChat() const
{
    assert(mChat);
    if (mChat)
    {
        return (mChat->crypto()->isPublicChat());
    }

    parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99011, "GroupChatRoom::publicChat(), chatd::Chat isn't yet created", false, static_cast<const char*>(nullptr));

    return false;
}

uint64_t GroupChatRoom::getPublicHandle() const
{
    assert(mChat);
    if (mChat)
    {
        return (mChat->getPublicHandle());
    }

    parent.mKarereClient.api.callIgnoreResult(&::mega::MegaApi::sendEvent, 99011, "GroupChatRoom::getPublicHandle(), chatd::Chat isn't yet created", false, static_cast<const char*>(nullptr));
    return karere::Id::inval();
}

unsigned int GroupChatRoom::getNumPreviewers() const
{
    return mChat->getNumPreviewers();
}

// return true if new peer, peer removed or peer's privilege updated
bool GroupChatRoom::previewMode() const
{
    return mChat->previewMode();
}

void ChatRoomList::deleteRoomFromDb(const Id &chatid)
{
    auto db = mKarereClient.db;
    if (db.isOpen())   // upon karere::Client destruction, DB is already closed
    {
        db.query("delete from chat_peers where chatid = ?", chatid);
        db.query("delete from chat_vars where chatid = ?", chatid);
        db.query("delete from chats where chatid = ?", chatid);
        db.query("delete from history where chatid = ?", chatid);
        db.query("delete from manual_sending where chatid = ?", chatid);
        db.query("delete from sending where chatid = ?", chatid);
        db.query("delete from sendkeys where chatid = ?", chatid);
        db.query("delete from node_history where chatid = ?", chatid);
        // no need to clear scheduled meetings nor occurrences by chatid, as they will be deleted on cascade
    }
}

promise::Promise<std::shared_ptr<std::string>> GroupChatRoom::unifiedKey()
{
    return mChat->crypto()->getUnifiedKey();
}
// return true if new peer or peer removed. Updates peer privileges as well
bool GroupChatRoom::syncMembers(const mega::MegaTextChat& chat)
{
    auto getUserPrivMap = [](const mega::MegaTextChat& chat) -> UserPrivMap
    {
        UserPrivMap u;
        auto members = chat.getPeerList();
        if (members)
        {
            auto size = members->size();
            for (int i = 0; i < size; ++i)
            {
                u.emplace(members->getPeerHandle(i), static_cast<chatd::Priv>(members->getPeerPrivilege(i)));
            }
        }
        return u;
    };

    bool peersChanged = false;
    UserPrivMap users = getUserPrivMap(chat);
    auto db = parent.mKarereClient.db;
    auto commitEach = parent.mKarereClient.commitEach() || mAutoJoining;
    parent.mKarereClient.setCommitMode(false);

    for (auto ourIt = mPeers.begin(); ourIt != mPeers.end();)
    {
        auto [userid, member] = *ourIt;
        auto itApiUser = users.find(userid);
        auto userRemoved = itApiUser == users.end();

        if (userRemoved)
        {
            peersChanged = true;
            ourIt++;    // prevent iterator becoming invalid due to removal
            removeMember(userid);
            // we must not call onUserLeave(userid), as OP_JOIN code path will do anyway, and it
            // would duplicate notification in case we also call from here
        }
        else
        {
            // update existing peer privilege
            auto& peerPriv = member->mPriv;
            auto newPriv = itApiUser->second;
            if (peerPriv != newPriv)
            {
                KR_LOG_DEBUG(
                    "%sGroupChatRoom[%s]:syncMembers: Changed privilege of member %s: %d -> %d",
                    getLoggingName(),
                    ID_CSTR(chatid()),
                    ID_CSTR(userid),
                    peerPriv,
                    newPriv);

                onUserJoin(member->mHandle, newPriv);
                peerPriv = newPriv;
                db.query("update chat_peers set priv=? where chatid=? and userid=?", peerPriv, mChatid, userid);
            }
            ourIt++;
        }
    }
    parent.mKarereClient.setCommitMode(commitEach);
    updateTitleFromMemberNames(users, peersChanged);
    return peersChanged;
}

void GroupChatRoom::initChatTitle(const std::string& title, int isTitleEncrypted, bool saveToDb)
{
    auto wptr = weakHandle();
    mHasTitle = (!title.empty() && title.at(0));
    if (mHasTitle)
    {
        if (saveToDb)
        {
            updateTitleInDb(title, isTitleEncrypted);
        }

        switch (isTitleEncrypted)
        {
            case strongvelope::kDecrypted:
                mTitleString = title;
                notifyTitleChanged();
                return;

            case strongvelope::kEncrypted:
                mEncryptedTitle = title;
                decryptTitle().fail(
                    [this, wptr, lname = std::string{getLoggingName()}](const ::promise::Error& e)
                    {
                        if (wptr.deleted())
                        {
                            return;
                        }
                        KR_LOG_ERROR("%sGroupChatRoom: failed to decrypt title for chat %s: %s",
                                     lname.c_str(),
                                     ID_CSTR(mChatid),
                                     e.what());
                    });
                return;

            case strongvelope::kUndecryptable:
                KR_LOG_ERROR("%sUndecryptable chat title for chat %s",
                             getLoggingName(),
                             ID_CSTR(mChatid));
                // fallback to makeTitleFromMemberNames()
                break;
        }
    }

    // if has no title or it's undecryptable...
    mMemberNamesResolved.then(
        [wptr, this]()
        {
            if (wptr.deleted())
            {
                return;
            }
            makeTitleFromMemberNames();
        });
}

bool GroupChatRoom::hasChatLinkChanged(const uint64_t ph, const std::string &decryptedTitle,
                                       const bool meeting, const ::mega::ChatOptions_t opts) const
{
    if ((ph != getPublicHandle())
        || (meeting != mMeeting)
        || (!mChatOptions.areEqual(opts))
        || (titleString().compare(decryptedTitle)))
    {
        return true;
    }

    return false;
}

void GroupChatRoom::clearTitle()
{
    makeTitleFromMemberNames();
    parent.mKarereClient.db.query("update chats set title=NULL where chatid=?", mChatid);
}

void GroupChatRoom::syncChatTitle(const mega::MegaTextChat& chat, const bool membersChanged)
{
    // Title changes
    const char* title = chat.getTitle();
    mHasTitle = (title && title[0]);
    if (mHasTitle && mEncryptedTitle != title)
    {
        // if the title was already decrypted in cache at startup, the `mEncryptedTitle` won't
        // be initialized yet (the encrypted flavour of the title is saved in cache but
        // overwriten when decrypted) In consequence, the first actionpacket will initialize it
        // and decrypt it once per execution
        mEncryptedTitle = title;
        updateTitleInDb(mEncryptedTitle, strongvelope::kEncrypted);
        decryptTitle().fail(
            [wptr = weakHandle()](const ::promise::Error& err)
            {
                if (wptr.deleted())
                {
                    return;
                }
                KR_LOG_DEBUG("Can't decrypt chatroom title. In function: "
                             "GroupChatRoom::syncWithApi. Error: %s",
                             err.what());
            });
    }
    else if (!mHasTitle && membersChanged)
    {
        KR_LOG_DEBUG("%sEmpty title received for groupchat %s. Peers changed, updating title...",
                     getLoggingName(),
                     ID_CSTR(mChatid));
        clearTitle();
    }
}

void GroupChatRoom::syncSchedMeetings(const mega::MegaTextChat& chat)
{
    if (chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_MEETING))
    {
        updateSchedMeetings(chat);
    }
    else if (chat.getScheduledMeetingList() && chat.getScheduledMeetingList()->size() &&
             mScheduledMeetings.empty())
    {
        // if chat from API have scheduled meetings, but we don't have those meetings stored in
        // karere
        addSchedMeetings(chat.getScheduledMeetingList());
    }

    if (chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_REPLACE_OCURR) ||
        chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_APPEND_OCURR))
    {
        addSchedMeetingsOccurrences(chat);
    }
    else if (chat.getUpdatedOccurrencesList() && chat.getUpdatedOccurrencesList()->size())
    {
        assert(false);
        KR_LOG_WARNING("%ssyncWithApi: Chat received from SDK contains updated occurrences, but no "
                       "related change is set",
                       getLoggingName());
    }
}

bool GroupChatRoom::syncOwnPrivilege(const mega::MegaTextChat& chat)
{
    auto oldPriv = mOwnPriv;
    auto newPriv = static_cast<chatd::Priv>(chat.getOwnPrivilege());
    auto ownPrivChanged = oldPriv != newPriv;
    bool removedFromChat = false;
    if (ownPrivChanged) // Manage own user privilege change
    {
        if (!syncOwnPriv(newPriv))
        {
            KR_LOG_ERROR("%sChatroom[%s]: API event: couldn't update own priv for chat: ",
                         getLoggingName(),
                         ID_CSTR(mChatid));
            assert(false);
            return removedFromChat;
        }

        if (newPriv == chatd::PRIV_RM)
        {
            KR_LOG_DEBUG("%sChatroom[%s]: API event: We were removed from chat: ",
                         getLoggingName(),
                         ID_CSTR(mChatid));
            notifyOwnExcludedFromChat();
            removedFromChat = true;
        }
        else if (oldPriv == chatd::PRIV_RM && newPriv != chatd::PRIV_RM)
        {
            KR_LOG_DEBUG("%sChatroom[%s]: API event: We were re/invited or re/joined to chat: ",
                         getLoggingName(),
                         ID_CSTR(mChatid));
            rejoinChatOwnUser();
            notifyRejoinedChat();
        }
        else
        {
            KR_LOG_DEBUG("%sChatroom[%s]: API event: Our own privilege changed for chat: ",
                         getLoggingName(),
                         ID_CSTR(mChatid));
            notifyOwnUserPrivChange();
        }
    }

    return removedFromChat;
}

bool GroupChatRoom::syncWithApi(const mega::MegaTextChat& chat)
{
    // Updates GroupChatRoom mode, if it has changed respect to MegaTextChat received from SDK
    auto syncChatMode = [this](const mega::MegaTextChat& chat)
    {
        if (!chat.isPublicChat() && publicChat())
        {
            KR_LOG_DEBUG("%sChatroom[%s]: API event: mode changed to private",
                         getLoggingName(),
                         ID_CSTR(mChatid));
            setChatPrivateMode();
            // in case of previewMode, it's also updated in cache
        }
    };

    // Updates GroupChatRoom options, if it has changed respect to MegaTextChat received from SDK
    auto syncChatOptions = [this](const mega::MegaTextChat& chat)
    {
        if (!mChatOptions.areEqual(chat.getChatOptions()))
        {
            KR_LOG_DEBUG("%sChatroom[%s]: API event: chat options have changed",
                         getLoggingName(),
                         ID_CSTR(mChatid));
            updateChatOptions(chat.getChatOptions());
        }
    };

    // Sync chat mode (public|private)
    syncChatMode(chat);

    // Sync chat options
    syncChatOptions(chat);

    // Sync scheduled meetings
    syncSchedMeetings(chat);

    // Sync own privilege, if this method returns true, it means that we have been removed from chat
    if (syncOwnPrivilege(chat))
    {
        return true;
    }

    // Sync chat participants priv changes
    bool membersChanged = syncMembers(chat);
    mAutoJoining = false; // set mAutoJoining false to enable Db transactional mode

    // Sync chat title changes
    syncChatTitle(chat, membersChanged);

    // Sync changes in archive mode
    if (syncArchive(chat.isArchived()))
    {
        onArchivedChanged(mIsArchived);
    }

    KR_LOG_DEBUG("%sSynced group chatroom %s with API.", getLoggingName(), ID_CSTR(mChatid));
    return true;
}

void GroupChatRoom::setChatPrivateMode()
{
    if (mMeeting)
    {
        mMeeting = false; // in case public chat was a meeting room set mMeeting to false
    }

    //Update strongvelope
    KR_LOG_DEBUG("%sGroupChatRoom::setChatPrivateMode: EKR enabled (private chat) for chat: %s",
                 getLoggingName(),
                 karere::Id(mChatid).toString().c_str());

    chat().crypto()->setPrivateChatMode();

    //Update cache updating mode and ensure that meeting field is disabled (0)
    parent.mKarereClient.db.query("update chats set mode = '0', meeting = '0' where chatid = ?", mChatid);

    notifyChatModeChanged();

    for (auto member : mPeers)
    {
        chat().requestUserAttributes(member.first);
        chat().crypto()->fetchUserKeys(member.first);
    }
}

void GroupChatRoom::updateChatOptions(mega::ChatOptions_t opt)
{
    mega::ChatOptions newOptions(opt);
    mega::ChatOptions oldOptions(mChatOptions);

    if (!newOptions.isValid())
    {
        KR_LOG_WARNING("%saddOrUpdateChatOptions: options value (%u) is out of range",
                       getLoggingName(),
                       newOptions.value());
        assert(false);
        return;
    }

    // update chat options in ram and db
    parent.mKarereClient.db.query("update chats set chat_options = ? where chatid = ?", opt, mChatid);
    mChatOptions.set(opt); // replace old options set by new one

    // compare old and new options set to notify apps those ones that have changed
    if (oldOptions.speakRequest() != newOptions.speakRequest()) { notifyChatOptionsChanged(mega::ChatOptions::kSpeakRequest); }
    if (oldOptions.waitingRoom() != newOptions.waitingRoom())   { notifyChatOptionsChanged(mega::ChatOptions::kWaitingRoom); }
    if (oldOptions.openInvite() != newOptions.openInvite())     { notifyChatOptionsChanged(mega::ChatOptions::kOpenInvite); }
}

void GroupChatRoom::addSchedMeetings(const mega::MegaScheduledMeetingList* schedMeetings)
{
    if (!schedMeetings || !schedMeetings->size())
    {
        return;
    }

    for (unsigned int i = 0; i < schedMeetings->size(); i++)
    {
        auto res = mScheduledMeetings.emplace(schedMeetings->at(i)->schedId(), new KarereScheduledMeeting(schedMeetings->at(i)));
        if (res.second)
        {
            assert(res.first->second);
            getClientDbInterface().insertOrUpdateSchedMeeting(*res.first->second);
            notifySchedMeetingUpdated(res.first->second.get(), KarereScheduledMeeting::newSchedMeetingFlagsValue());
        }
        else
        {
            KR_LOG_WARNING("%saddSchedMeetings: can't add a scheduled meeting", getLoggingName());
        }
    }
}

void GroupChatRoom::updateSchedMeetingsWithList(const mega::MegaScheduledMeetingList* smList)
{
    if (!smList) { return; }

    // update sched meetings in ram and db with received ones in smList
    for (unsigned int i = 0; i < smList->size(); ++i)
    {
        const mega::MegaScheduledMeeting* sm = smList->at(i);
        auto it = mScheduledMeetings.find(sm->schedId());
        if (it != mScheduledMeetings.end() && it->second)
        {
            const KarereScheduledMeeting* ksm = it->second.get();
            KarereScheduledMeeting::sched_bs_t diff = ksm->compare(sm);

            if (diff.any()) // sm has changed respect received data in smList
            {
                it->second.reset(new KarereScheduledMeeting(sm));
                notifySchedMeetingUpdated(it->second.get(), diff.to_ulong());
                getClientDbInterface().insertOrUpdateSchedMeeting(*it->second);
            }
        }
        else // not found (new scheduled meeting), add it
        {
            auto res = mScheduledMeetings.emplace(sm->schedId(), new KarereScheduledMeeting(sm));
            if (res.second)
            {
                notifySchedMeetingUpdated(res.first->second.get(), KarereScheduledMeeting::newSchedMeetingFlagsValue());
                assert(res.first->second);
                getClientDbInterface().insertOrUpdateSchedMeeting(*res.first->second);
            }
        }
    }

    // remove (from ram and db) those sched meetings in db not found in smList
    const auto schedMeetingsInDb = getClientDbInterface().getSchedMeetingsByChatId(chatid());
    for (const auto& i: schedMeetingsInDb)
    {
        const auto sm = i.get();
        assert(sm);
        if (sm && !smList->getBySchedId(sm->schedId())) // if schedid not found in list received from API
        {
            notifySchedMeetingUpdated(sm, KarereScheduledMeeting::deletedSchedMeetingFlagsValue());
            mScheduledMeetings.erase(sm->schedId());
            getClientDbInterface().removeSchedMeetingBySchedId(sm->schedId());
        }
    }

    // clear list of current scheduled meetings occurrences from db by chatid
    getClientDbInterface().clearSchedMeetingOcurrByChatid(chatid());

    // clear list of current scheduled meetings occurrences in ram
    mScheduledMeetingsOcurrences.clear();

    // set occurrences loaded flag to false
    mAllDbOccurrencesLoadedInRam = false;

    // notify scheduled meetings occurrences for this chat have changed (in order to app discard them)
    notifySchedMeetingOccurrencesUpdated(false /*append*/);
}

void GroupChatRoom::updateSchedMeetings(const mega::MegaTextChat& chat)
{
    if (!chat.getSchedMeetingsChanged())
    {
        return;
    }

    const mega::MegaHandleList* changed = chat.getSchedMeetingsChanged();
    const mega::MegaScheduledMeetingList* apiSchedMeetings = chat.getScheduledMeetingList();
    for (unsigned int i = 0; i < changed->size(); i++)
    {
        auto h = changed->get(i);
        auto it = mScheduledMeetings.find(h);
        ::mega::MegaScheduledMeeting* newSched = apiSchedMeetings ? apiSchedMeetings->getBySchedId(h) : nullptr;

        if (!newSched) // remove scheduled meeting
        {
            getClientDbInterface().removeSchedMeetingBySchedId(h); // remove from db
            if (it != mScheduledMeetings.end())
            {
                // schedId was in changed list, but not in sched meeting list from API (it has been removed)
                // important: SDK will notify deletion of child scheduled meetings when it's parent has been removed
                notifySchedMeetingUpdated(it->second.get(), KarereScheduledMeeting::deletedSchedMeetingFlagsValue());
                mScheduledMeetings.erase(it);

                // clear list of current scheduled meetings occurrences from db by chatid
                // this is required as we are removing a scheduled meeting by sched Id (FK),
                // however we want to remove all scheduled meeting for that chat due to API specs
                getClientDbInterface().clearSchedMeetingOcurrByChatid(chat.getHandle());

                // clear list of current scheduled meetings occurrences
                mScheduledMeetingsOcurrences.clear();

                // set occurrences loaded flag to false
                mAllDbOccurrencesLoadedInRam = false;
                // we don't need to notify with notifySchedMeetingOccurrencesUpdated as SDK will automatically fetch occurrences again
            }
            else // if scheduled meeting we want to remove, no longer exists in ram
            {
                KR_LOG_WARNING("%supdateSchedMeetings: scheduled meeting %s no longer exists",
                               getLoggingName(),
                               karere::Id(h).toString().c_str());
            }
        }
        else
        {
            KarereScheduledMeeting::sched_bs_t diff = (it == mScheduledMeetings.end())
                    ? KarereScheduledMeeting::sched_bs_t(KarereScheduledMeeting::newSchedMeetingFlagsValue())
                    : it->second->compare(newSched);

            if (diff.any())
            {
                if (it != mScheduledMeetings.end())
                {
                    it->second.reset(new KarereScheduledMeeting(newSched));
                    notifySchedMeetingUpdated(it->second.get(), diff.to_ulong());

                    // insert in db
                    assert(it->second);
                    getClientDbInterface().insertOrUpdateSchedMeeting(*it->second);
                }
                else // not found (new scheduled meeting), add it
                {
                    auto res = mScheduledMeetings.emplace(newSched->schedId(), new KarereScheduledMeeting(newSched));
                    if (res.second)
                    {
                        notifySchedMeetingUpdated(res.first->second.get(), diff.to_ulong());

                        assert(res.first->second);
                        getClientDbInterface().insertOrUpdateSchedMeeting(*res.first->second);
                    }
                }
            }
        }
    }
}

const KarereScheduledMeeting* GroupChatRoom::getScheduledMeetingsBySchedId(const karere::Id& schedId) const
{
    auto it = mScheduledMeetings.find(schedId);
    return it != mScheduledMeetings.end()
            ? it->second.get()
            : nullptr;
}

const std::map<karere::Id, std::unique_ptr<KarereScheduledMeeting>>& GroupChatRoom::getScheduledMeetings() const
{
    return mScheduledMeetings;
}

const std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>>&
GroupChatRoom::getScheduledMeetingsOccurrences() const
{
    return mScheduledMeetingsOcurrences;
}

std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>>
GroupChatRoom::getFutureScheduledMeetingsOccurrences(unsigned int count, ::mega::m_time_t since, ::mega::m_time_t until) const
{
    if (mScheduledMeetingsOcurrences.empty() || !count)
    {
        std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>>();
    }

    std::vector<std::shared_ptr<KarereScheduledMeetingOccurr>> ocurrList;
    for (const auto& it: mScheduledMeetingsOcurrences)
    {
        assert(it->endDateTime() != ::mega::mega_invalid_timestamp);
        const ::mega::m_time_t startOcurrTs = it->startDateTime();
        const ::mega::m_time_t endOcurrTs = it->endDateTime();
        const ::mega::m_time_t sinceTs = since
                ? since         /* provided by user (unix timestamp [UTC]) */
                : time(nullptr) /* now (unix timestamp [UTC]) */;

        const auto isInRange = [&sinceTs, &until](::mega::m_time_t ts)
        {
            const bool hasuntil = until != ::mega::mega_invalid_timestamp;
            return sinceTs < ts && (!hasuntil || until > ts);
        };

        if (isInRange(startOcurrTs) || isInRange(endOcurrTs))
        {
            ocurrList.emplace_back(it->copy());
        }
    }
    parent.mKarereClient.sortOccurrences(ocurrList); // sort occurrences list
    if (ocurrList.size() > count) { ocurrList.resize(count); }
    return ocurrList;
}

void GroupChatRoom::addSchedMeetingsOccurrences(const mega::MegaTextChat& chat)
{
    assert(chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_REPLACE_OCURR)
           || chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_APPEND_OCURR));

    // set occurrences loaded flag to false, it doesn't matter if we are clearing current list,
    // or appending newer ones, we can assume that we don't have all db occurrences loaded in ram
    // this flag is set false upon every changes in ocurrences for this chatroom
    mAllDbOccurrencesLoadedInRam = false;

    if (chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_REPLACE_OCURR))
    {
        // important: just wipe current occurrences list (from db and RAM) if fetch was not triggered by user
        getClientDbInterface().clearSchedMeetingOcurrByChatid(chat.getHandle());
        mScheduledMeetingsOcurrences.clear();
    }

    // add received occurrences from SDK if any
    if (chat.getUpdatedOccurrencesList() && chat.getUpdatedOccurrencesList()->size())
    {
        const mega::MegaScheduledMeetingList* schedMeetings = chat.getUpdatedOccurrencesList() ;
        for (unsigned int i = 0; i < chat.getUpdatedOccurrencesList()->size(); i++)
        {
            std::unique_ptr<KarereScheduledMeetingOccurr> aux = std::make_unique<KarereScheduledMeetingOccurr>(schedMeetings->at(i));
            getClientDbInterface().insertOrUpdateSchedMeetingOcurr(*aux);
        }
    }

    notifySchedMeetingOccurrencesUpdated(chat.hasChanged(mega::MegaTextChat::CHANGE_TYPE_SCHED_APPEND_OCURR)); // notify scheduled meetings occurrences for this chat have changed
}

void GroupChatRoom::loadSchedMeetingsFromDb()
{
    std::vector<std::unique_ptr<KarereScheduledMeeting>> schedMeetings = getClientDbInterface().getSchedMeetingsByChatId(chatid());
    for (unsigned int i = 0; i < schedMeetings.size(); i++)
    {
        mScheduledMeetings.emplace(schedMeetings.at(i)->schedId(), std::move((schedMeetings.at(i))));
    }
}

size_t GroupChatRoom::loadOccurresInMemoryFromDb()
{
    if (!mAllDbOccurrencesLoadedInRam)
    {
        // only load occurrences from DB if occurrences in memory are not up to date
        mScheduledMeetingsOcurrences.clear();
        std::vector<std::unique_ptr<KarereScheduledMeetingOccurr>> schedMeetingsOccurr = getClientDbInterface().getSchedMeetingsOccurByChatId(chatid());
        for (unsigned int i = 0; i < schedMeetingsOccurr.size(); i++)
        {
            std::unique_ptr<KarereScheduledMeetingOccurr> aux(new KarereScheduledMeetingOccurr((schedMeetingsOccurr.at(i)).get()));
            mScheduledMeetingsOcurrences.emplace_back(std::move(aux));
        }
        mAllDbOccurrencesLoadedInRam = true; // set occurrences loaded flag true, to indicate that occurrences have been loaded from Db
    }
    return mScheduledMeetingsOcurrences.size();
}

GroupChatRoom::Member::Member(GroupChatRoom& aRoom, const uint64_t& user, chatd::Priv aPriv)
: mRoom(aRoom), mHandle(user), mPriv(aPriv), mName("\0", 1)
{
}

GroupChatRoom::Member::~Member()
{
    mRoom.parent.mKarereClient.userAttrCache().removeCb(mNameAttrCbHandle);
    mRoom.parent.mKarereClient.userAttrCache().removeCb(mEmailAttrCbHandle);
}

promise::Promise<void> GroupChatRoom::Member::nameResolved() const
{
    return mNameResolved;
}

void GroupChatRoom::Member::registerCallBacks(bool fetchIsRequired)
{
    mNameAttrCbHandle = mRoom.parent.mKarereClient.userAttrCache().getAttr(
        mHandle, USER_ATTR_FULLNAME, this, [](Buffer* buf, void* userp)
    {
        auto self = static_cast<Member*>(userp);
        if (buf && !buf->empty())
        {
            self->mName.assign(buf->buf(), buf->dataSize());
        }
        else
        {
            self->mName.assign("\0", 1);
        }
        if (self->mRoom.mAppChatHandler)
        {
            if (!self->mRoom.publicChat() || self->mRoom.mPeers.size() <= PRELOAD_CHATLINK_PARTICIPANTS)
            {
                self->mRoom.mAppChatHandler->onMemberNameChanged(self->mHandle, self->mName);
            }
        }

        if (!self->mNameResolved.done())
        {
            self->mNameResolved.resolve();
        }
        else if (self->mRoom.memberNamesResolved().done() && !self->mRoom.mHasTitle)
        {
            self->mRoom.makeTitleFromMemberNames();
        }
    }, false, fetchIsRequired, mRoom.isChatdChatInitialized() ? mRoom.chat().getPublicHandle() : karere::Id::inval().val);

    if (!mRoom.parent.mKarereClient.anonymousMode())
    {
        mEmailAttrCbHandle = mRoom.parent.mKarereClient.userAttrCache().getAttr(
            mHandle, USER_ATTR_EMAIL, this, [](Buffer* buf, void* userp)
        {
            auto self = static_cast<Member*>(userp);
            if (buf && !buf->empty())
            {
                self->mEmail.assign(buf->buf(), buf->dataSize());
                if (self->mName.size() <= 1 && self->mRoom.memberNamesResolved().done() && !self->mRoom.mHasTitle)
                {
                    self->mRoom.makeTitleFromMemberNames();
                }
            }
        }, false, fetchIsRequired, mRoom.isChatdChatInitialized() ? mRoom.chat().getPublicHandle() : karere::Id::inval().val);
    }
}

void Client::connectToChatd()
{
    for (auto& item: *chats)
    {
        auto& chat = *item.second;
        if (!chat.chat().isDisabled())
        {
            chat.connect();
        }
    }
}

ContactList::ContactList(Client& aClient)
:client(aClient)
{}

void ContactList::loadFromDb()
{
    SqliteStmt stmt(client.db, "select userid, email, visibility, since from contacts");
    while(stmt.step())
    {
        auto userid = stmt.integralCol<uint64_t>(0);
        Contact *contact = new Contact(*this, userid, stmt.stringCol(1), stmt.integralCol<int>(2), stmt.integralCol<int64_t>(3), nullptr);
        this->emplace(userid, contact);
    }
}

void Contact::onVisibilityChanged(int newVisibility)
{
    assert(newVisibility != mVisibility);
    auto oldVisibility = mVisibility;
    mVisibility = newVisibility;

    if (mChatRoom
            && oldVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN
            && newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
    {
        mChatRoom->notifyRejoinedChat();
    }
}

void Contact::setContactName(std::string name)
{
    mName = name;
}

std::string Contact::getContactName(bool binaryLayout)
{
    return (binaryLayout || mName.empty()) ? mName : mName.substr(1);
}

void ContactList::syncWithApi(mega::MegaUserList &users)
{
    int count = users.size();
    for (int i = 0; i < count; i++)
    {
        ::mega::MegaUser &user = *users.get(i);
        if (user.getHandle() == client.myHandle())
        {
            continue;
        }

        auto newVisibility = user.getVisibility();

        uint64_t changed = user.getChanges();
        bool updateCache = !user.isOwnChange();

        ContactList::iterator it = find(user.getHandle());
        if (it != end())    // existing contact or ex-contact
        {
            auto handle = it->first;
            Contact *contact = it->second;
            auto oldVisibility = contact->visibility();

            if (oldVisibility != newVisibility)
            {
                if (newVisibility == ::mega::MegaUser::VISIBILITY_INACTIVE)
                {
                    delete contact;
                    erase(it);
                    client.db.query("delete from contacts where userid=?", handle);
                    return;
                }
                else
                {
                    client.db.query("update contacts set visibility = ? where userid = ?", newVisibility, handle);
                    contact->onVisibilityChanged(newVisibility);

                    if (oldVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN
                            && newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
                    {
                        // API doesn't notify about changes for ex-contacts, so need to update user attributes
                        assert(user.getChanges());  // currently, firstname and lastname only (driven by SDK)
                        updateCache = true;
                    }
                }
            }

            if (contact->email() != user.getEmail())
            {
                std::string newEmail;
                const char *userEmail = user.getEmail();
                if (userEmail && userEmail[0])
                {
                    newEmail.assign(userEmail);
                }

                // Update contact email in memory and cache
                contact->mEmail = newEmail;
                client.db.query("update contacts set email = ? where userid = ?", newEmail, handle);

                // We need to update user email in attr cache
                updateCache = true;
            }

            if (contact->since() != user.getTimestamp())
            {
                contact->mSince = user.getTimestamp();
                client.db.query("update contacts set since = ? where userid = ?", contact->since(), handle);
            }
        }
        else    // contact was not created yet
        {
            std::string email(user.getEmail());
            auto userid = user.getHandle();
            auto ts = user.getTimestamp();
            client.db.query("insert or replace into contacts(userid, email, visibility, since) values(?,?,?,?)",
                            userid, email, newVisibility, ts);
            Contact *contact = new Contact(*this, userid, email, newVisibility, ts, nullptr);
            emplace(userid, contact);

            // find if there is a 1on1 room with this contact
            // (in case on 1on1 with users who canceled and restored their account,
            // MEGAchat knows about the chatroom but not about the contact)
            for (auto &it : *client.chats)
            {
                if (it.second->isGroup())
                    continue;

                auto chat = static_cast<PeerChatRoom*>(it.second);
                if (chat->peer() == userid)
                {
                    KR_LOG_WARNING("%sContact restored (%s) for a 1on1 room (%s)",
                                   client.getLoggingName(),
                                   Id(userid).toString().c_str(),
                                   Id(chat->chatid()).toString().c_str());

                    chat->initContact(userid);
                    break;
                }
            }

            KR_LOG_DEBUG("%sAdded new user from API: %s", client.getLoggingName(), email.c_str());

            // If the user was part of a group before being added as a contact, we need to update user attributes,
            // currently firstname, lastname and email, in order to ensure that are re-fetched for users
            // with group chats previous to establish contact relationship
            changed = ::mega::MegaUser::CHANGE_TYPE_FIRSTNAME | ::mega::MegaUser::CHANGE_TYPE_LASTNAME | ::mega::MegaUser::CHANGE_TYPE_EMAIL;
            updateCache = true;
        }

        if (changed && updateCache)
        {
            client.userAttrCache().onUserAttrChange(user.getHandle(), changed);
        }
    }
}

ContactList::~ContactList()
{
    for (auto& it: *this)
        delete it.second;
}

const std::string* ContactList::getUserEmail(uint64_t userid) const
{
    auto it = find(userid);
    if (it == end())
        return nullptr;
    return &(it->second->email());
}

Contact* ContactList::contactFromEmail(const std::string &email) const
{
    for (auto it = begin(); it != end(); it++)
    {
        if (it->second->email() == email)
        {
            return it->second;
        }
    }
    return nullptr;
}

Contact* ContactList::contactFromUserId(uint64_t userid) const
{
    auto it = find(userid);
    return (it == end())? nullptr : it->second;
}

Contact::Contact(ContactList& clist, const uint64_t& userid,
                 const std::string& email, int visibility,
                 int64_t since, PeerChatRoom* room)
    :mClist(clist), mUserid(userid), mChatRoom(room), mEmail(email),
     mSince(since), mVisibility(visibility)
{
    mUsernameAttrCbId = mClist.client.userAttrCache()
            .getAttr(userid, USER_ATTR_FULLNAME, this,
    [](Buffer* data, void* userp)
    {
        //even if both first and last name are null, the data is at least
        //one byte - the firstname-size-prefix, which will be zero but
        //if lastname is not null the first byte will contain the
        //firstname-size-prefix but datasize will be bigger than 1 byte.

        // If fullname received is valid
        auto self = static_cast<Contact*>(userp);
        std::string alias = self->mClist.client.getUserAlias(self->userId());
        if (data && !data->empty() && *data->buf() != 0 && data->size() != 1)
        {
            // Update contact name
            std::string name(data->buf(), data->dataSize());

            // Preserve binary layout for contact name
            self->setContactName(name);
            if (alias.empty())
            {
                // Update title if there's no alias
                self->updateTitle(self->getContactName());
            }
        }
        else if (alias.empty())
        {
            // If there's no alias nor fullname
            self->updateTitle(self->mEmail);
        }
    });

    mEmailAttrCbId = mClist.client.userAttrCache().getAttr(userid, USER_ATTR_EMAIL, this,
    [](Buffer* data, void* userp)
    {
        auto self = static_cast<Contact*>(userp);
        if (data && !data->empty() && *data->buf() != 0 && data->size() != 1)
        {
            self->mEmail.assign(data->buf(), data->dataSize());
            if (self->mChatRoom)
            {
                // if peerChatRoom exists, update email
                self->mChatRoom->mEmail.assign(self->mEmail);
            }

            // If contact has alias or contactName don't update title
            std::string alias = self->mClist.client.getUserAlias(self->userId());
            std::string contactName = self->getContactName();
            if (alias.empty() && contactName.empty())
            {
                // Set email as title because contact doesn't have alias nor fullname
                self->updateTitle(self->mEmail);
            }
        }
    });

    if (mTitleString.empty()) // user attrib fetch was not synchronous
    {
        updateTitle(email);
        assert(!mTitleString.empty()
               || mClist.client.api.sdk.isLoggedIn() == ::mega::EPHEMERALACCOUNTPLUSPLUS);
    }

    mIsInitializing = false;
}

// the title string starts with a byte equal to the first name length, followed by first name,
// then second name
void Contact::updateTitle(const std::string& str)
{
    mTitleString = str;
    notifyTitleChanged();
}

void Contact::notifyTitleChanged()
{
    callAfterInit(this, [this]
    {
        //1on1 chatrooms don't have a binary layout for the title
        if (mChatRoom)
            mChatRoom->updateTitle(mTitleString);
    }, mClist.client.appCtx);
}

Contact::~Contact()
{
    auto& client = mClist.client;
    if (!client.isTerminated())
    {
        client.userAttrCache().removeCb(mUsernameAttrCbId);
        client.userAttrCache().removeCb(mEmailAttrCbId);
    }
}

promise::Promise<ChatRoom*> Contact::createChatRoom()
{
    if (mChatRoom)
    {
        KR_LOG_WARNING(
            "%sContact::createChatRoom: chat room already exists, check before calling this method",
            mChatRoom->getLoggingName());
        return Promise<ChatRoom*>(mChatRoom);
    }
    mega::MegaTextChatPeerListPrivate peers;
    peers.addPeer(mUserid, chatd::PRIV_MODERATOR);
    return mClist.client.api.call(&mega::MegaApi::createChat, false, &peers, nullptr, mega::ChatOptions::kEmpty, nullptr)
    .then([this](ReqResult result) -> Promise<ChatRoom*>
    {
        auto& list = *result->getMegaTextChatList();
        if (list.size() < 1)
            return ::promise::Error("Empty chat list returned from API");
        if (mChatRoom)
        {
            return mChatRoom;
        }
        auto room = mClist.client.chats->addRoom(*list.get(0));
        if (!room)
            return ::promise::Error("API created an incorrect 1on1 room");
        room->connect();
        return room;
    });
}

void Contact::setChatRoom(PeerChatRoom& room)
{
    assert(!mChatRoom);
    assert(!mTitleString.empty());
    mChatRoom = &room;
    mChatRoom->updateTitle(mTitleString);
}

void Contact::attachChatRoom(PeerChatRoom& room)
{
    if (mChatRoom)
        throw std::runtime_error("attachChatRoom[room "+Id(room.chatid()).toString()+ "]: contact "+
            Id(mUserid).toString()+" already has a chat room attached");
    KR_LOG_DEBUG("%sAttaching 1on1 chatroom %s to contact %s",
                 room.getLoggingName(),
                 ID_CSTR(room.chatid()),
                 ID_CSTR(mUserid));
    setChatRoom(room);
}

#define RETURN_ENUM_NAME(name) case name: return #name

const char* Client::initStateToStr(unsigned char state)
{
    switch (state)
    {
        RETURN_ENUM_NAME(kInitCreated);
        RETURN_ENUM_NAME(kInitWaitingNewSession);
        RETURN_ENUM_NAME(kInitHasOfflineSession);
        RETURN_ENUM_NAME(kInitHasOnlineSession);
        RETURN_ENUM_NAME(kInitTerminated);
        RETURN_ENUM_NAME(kInitErrGeneric);
        RETURN_ENUM_NAME(kInitErrNoCache);
        RETURN_ENUM_NAME(kInitErrCorruptCache);
        RETURN_ENUM_NAME(kInitErrSidInvalid);
    default:
        return "(unknown)";
    }
}
const char* Client::connStateToStr(ConnState state)
{
    switch(state)
    {
        RETURN_ENUM_NAME(kDisconnected);
        RETURN_ENUM_NAME(kConnecting);
        RETURN_ENUM_NAME(kConnected);
        default: return "(invalid)";
    }
}

bool Client::isCallActive(const Id& chatid) const
{
    bool callActive = false;

#ifndef KARERE_DISABLE_WEBRTC
    if (rtc)
    {
        rtcModule::ICall* call = rtc->findCallByChatid(chatid);
        if (call)
        {
            callActive = call->participate();
        }
    }
#endif

    return callActive;
}

bool Client::isCallInProgress(const karere::Id& chatid) const
{
    bool participantingInCall = false;

#ifndef KARERE_DISABLE_WEBRTC
    if (rtc)
    {
        rtcModule::ICall* call = rtc->findCallByChatid(chatid);
        if (call)
        {
            participantingInCall = (call->getState() == rtcModule::CallState::kStateInProgress);
        }
    }
#endif

    return participantingInCall;
}

void Client::updateAliases(Buffer *data)
{
    // Clean aliases map in case alias attr has been removed
    std::vector<Id>aliasesUpdated;
    if (!data || data->empty())
    {
        AliasesMap::iterator itAliases = mAliasesMap.begin();
        while (itAliases != mAliasesMap.end())
        {
            Id userid = itAliases->first;
            auto it = itAliases++;
            mAliasesMap.erase(it);
            aliasesUpdated.emplace_back(userid);
        }
    }
    // still some records/aliases in the attribute
    else if (std::unique_ptr<::mega::string_map> records{
                 ::mega::tlv::containerToRecords({data->buf(), data->size()})})
    {
        // Create a new map <uhBin, aliasB64> for the aliases that have been updated
        for (const auto& r: *records)
        {
            if (r.first.empty())
            {
                KR_LOG_ERROR("%s: Invalid string handle in aliases", getLoggingName());
                continue;
            }

            if (const Id userId(r.first.data()); !userId.isValid())
            {
                KR_LOG_ERROR("%s: Invalid handle in aliases", getLoggingName());
                continue;
            }
            else if (auto& alias = mAliasesMap[userId]; alias != r.second)
            {
                aliasesUpdated.emplace_back(userId);
                alias = r.second;
            }
        }

        for (auto itA = mAliasesMap.begin(); itA != mAliasesMap.end();)
        {
            if (const Id userId = itA->first; records->find(userId.toString()) == records->end())
            {
                aliasesUpdated.emplace_back(userId);
                mAliasesMap.erase(itA++);
            }
            else
            {
                ++itA;
            }
        }
    }
    // Update those contact's titles without a peer chatroom associated
    for (auto &userid : aliasesUpdated)
    {
        Contact *contact =  mContactList->contactFromUserId(userid);
        if (contact && !contact->chatRoom())
        {
            std::string title = getUserAlias(userid);
            if (title.empty())
            {
                title = contact->getContactName();
                if (title.empty())
                {
                    title = contact->email();
                }
            }
            contact->updateTitle(title);
        }
    }

    // Iterate through all chatrooms and update the aliases contained in aliasesUpdated
    for (auto &itChats : *chats)
    {
        ChatRoom *chatroom = itChats.second;
        for (auto &userid : aliasesUpdated)
        {
            if (chatroom->isGroup())
            {
                // If chatroom is a group chatroom and there's at least a chat member included
                // in aliasesUpdated map we need to re-generate the default title
                // if there's no custom title
                GroupChatRoom *room = static_cast<GroupChatRoom *>(chatroom);
                if (room->hasTitle() || room->peers().find(userid) == room->peers().end())
                {
                    continue;
                }
                room->makeTitleFromMemberNames();
                break;
            }
            else
            {
                PeerChatRoom *room = static_cast<PeerChatRoom *>(chatroom);
                if (userid != room->peer())
                {
                    continue;
                }
                room->updateChatRoomTitle();
                break;
            }
        }
    }
}

std::string Client::getUserAlias(uint64_t userId)
{
    std::string aliasBin;
    AliasesMap::iterator it = mAliasesMap.find(userId);
    if (it != mAliasesMap.end())
    {
        const std::string &aliasB64 = it->second;
        ::mega::Base64::atob(aliasB64, aliasBin);
    }
    return aliasBin;
}

void Client::setMyEmail(const std::string &email)
{
    mMyEmail = email;
}

const std::string& Client::getMyEmail() const
{
    return mMyEmail;
}

DbClientInterface& Client::getClientDbInterface()
{
    assert(mClientDbInterface);
    return *mClientDbInterface;
}

std::string encodeFirstName(const std::string& first)
{
    std::string result;
    result.reserve(first.size()+1);
    result+=(char)(first.size());
    if (!first.empty())
    {
        result.append(first);
    }
    return result;
}

// Init Stats methods

bool InitStats::isCompleted() const
{
    return mCompleted;
}

void InitStats::onCanceled()
{
    mCompleted = true;

    // clear maps to free some memory
    mStageShardStats.clear();
    mStageStats.clear();
    KR_LOG_WARNING("Init stats have been cancelled");
}

std::string InitStats::onCompleted(unsigned long long numNodes, size_t numChats, size_t numContacts)
{
    assert(!mCompleted);
    mCompleted = true;

    if (mInitState == kInitAnonymous)
    {
        // these stages don't occur in anonymous mode
        mStageStats[kStatsLogin] = 0;
        mStageStats[kStatsFetchNodes] = 0;
        mStageStats[kStatsPostFetchNodes] = 0;
    }

    mNumNodes = numNodes;
    mNumChats = static_cast<long>(numChats);
    mNumContacts = static_cast<long>(numContacts);

    std::string json = toJson();

    // clear maps to free some memory
    mStageShardStats.clear();
    mStageStats.clear();

    return json;
}

mega::dstime InitStats::currentTime()
{
    return mega::m_clock_getmonotonictimeDS();
}

void InitStats::shardStart(uint8_t stage, uint8_t shard)
{
    if (mCompleted)
    {
        return;
    }

    mStageShardStats[stage][shard].tsStart = currentTime();
}

void InitStats::shardEnd(uint8_t stage, uint8_t shard)
{
    if (mCompleted)
    {
        return;
    }

    InitStats::ShardStats *shardStats = &mStageShardStats[stage][shard];
    if (shardStats->tsStart)    // if starting ts not recorded --> discard
    {
        shardStats->elapsed = currentTime() - shardStats->tsStart;

        if (shardStats->elapsed > shardStats->maxElapsed)
        {
            shardStats->maxElapsed = shardStats->elapsed;
        }

        shardStats->tsStart = 0;
    }
}

void InitStats::incrementRetries(uint8_t stage, uint8_t shard)
{
    if (mCompleted)
    {
        return;
    }

    mStageShardStats[stage][shard].mRetries++;
}

void InitStats::handleShardStats(chatd::Connection::State oldState, chatd::Connection::State newState, uint8_t shard)
{
    if (mCompleted)
    {
        return;
    }

    switch (newState)
    {
        case chatd::Connection::State::kStateFetchingUrl:
            shardStart(InitStats::kStatsFetchChatUrl, shard);
            break;

        case chatd::Connection::State::kStateResolving:
            shardEnd(InitStats::kStatsFetchChatUrl, shard);
            break;

        case chatd::Connection::State::kStateConnecting:
            shardStart(InitStats::kStatsConnect, shard);
            break;

        case chatd::Connection::State::kStateConnected:
             shardEnd(InitStats::kStatsConnect, shard);
             shardStart(InitStats::kStatsLoginChatd, shard);
             break;

        case chatd::Connection::State::kStateDisconnected:  //Increments connection retries
            switch (oldState)
            {
                case chatd::Connection::State::kStateFetchingUrl:
                    incrementRetries(InitStats::kStatsFetchChatUrl, shard);
                    break;
                case chatd::Connection::State::kStateConnecting:
                    incrementRetries(InitStats::kStatsConnect, shard);
                    break;
                case chatd::Connection::State::kStateConnected:
                    incrementRetries(InitStats::kStatsLoginChatd, shard);
                    break;
                default:
                    break;
            }
            break;

        default:
            break;

    }
}

void InitStats::stageStart(uint8_t stage)
{
    if (mCompleted)
    {
        return;
    }

    mStageStats[stage] = currentTime();
}

void InitStats::stageEnd(uint8_t stage)
{
    if (mCompleted)
    {
        return;
    }

    assert(mStageStats[stage]);
    mStageStats[stage] = currentTime() - mStageStats[stage];
}

void InitStats::setInitState(uint8_t state)
{
    if (mCompleted)
    {
        return;
    }

    switch (state)
    {
        case  Client::kInitErrNoCache:
        case  Client::kInitErrCorruptCache:
            mInitState = kInitInvalidCache;
            break;

        case  Client::kInitHasOfflineSession:
            mInitState = kInitResumeSession;
            break;

        case  Client::kInitWaitingNewSession:
            mInitState = kInitNewSession;
            break;

        case  Client::kInitAnonymousMode:
            mInitState = kInitAnonymous;
            break;

        default:
            break;
    }
}

std::string InitStats::stageToString(uint8_t stage)
{
    switch(stage)
    {
        case kStatsInit: return "Init";
        case kStatsLogin: return "Login";
        case kStatsFetchNodes: return "Fetch nodes";
        case kStatsPostFetchNodes: return "Post fetch nodes";
        case kStatsConnection: return "Connection";
        case kStatsCreateAccount: return "Create account";
        default: return "(unknown)";
    }
}

std::string InitStats::shardStageToString(uint8_t stage)
{
    switch(stage)
    {
        case kStatsFetchChatUrl: return "Fetch chat url";
        case kStatsQueryDns: return "Query DNS";
        case kStatsConnect: return "Connect";
        case kStatsLoginChatd: return "Login all chats";
        default: return "(unknown)";
    }
}

std::string InitStats::toJson()
{
    std::string result;
    mega::dstime totalElapsed = 0; //Total elapsed time to finish all stages
    rapidjson::Document jSonDocument(rapidjson::kArrayType);
    rapidjson::Value jSonObject(rapidjson::kObjectType);
    rapidjson::Value jsonValue(rapidjson::kNumberType);

    // Generate stages array
    rapidjson::Document stageArray(rapidjson::kArrayType);
    for (StageMap::const_iterator itStages = mStageStats.begin(); itStages != mStageStats.end(); itStages++)
    {
        rapidjson::Value jSonStage(rapidjson::kObjectType);
        uint8_t stage = itStages->first;
        mega::dstime elapsed = itStages->second;

        // Add stage
        jsonValue.SetInt64(stage);
        jSonStage.AddMember(rapidjson::Value("stg"), jsonValue, jSonDocument.GetAllocator());

        std::string tag = stageToString(stage);
        rapidjson::Value stageTag(rapidjson::kStringType);
        stageTag.SetString(tag.c_str(), static_cast<unsigned int>(tag.length()), jSonDocument.GetAllocator());
        jSonStage.AddMember(rapidjson::Value("tag"), stageTag, jSonDocument.GetAllocator());

        // Add stage elapsed time
        totalElapsed += elapsed;
        jsonValue.SetInt64(elapsed);
        jSonStage.AddMember(rapidjson::Value("elap"), jsonValue, jSonDocument.GetAllocator());
        stageArray.PushBack(jSonStage, jSonDocument.GetAllocator());
    }

    // Generate sharded stages array
    rapidjson::Value shardStagesArray(rapidjson::kArrayType);
    StageShardMap::iterator itshstgs;
    for (itshstgs = this->mStageShardStats.begin(); itshstgs != mStageShardStats.end(); itshstgs++)
    {
        rapidjson::Value jSonStage(rapidjson::kObjectType);
        rapidjson::Document shardArray(rapidjson::kArrayType);
        uint8_t stage = itshstgs->first;

        ShardMap *shardMap = &(itshstgs->second);
        if (shardMap)
        {
            ShardMap::iterator itShard;
            for (itShard = shardMap->begin(); itShard != shardMap->end(); itShard++)
            {
                rapidjson::Value jSonShard(rapidjson::kObjectType);
                uint8_t shard = itShard->first;
                ShardStats &shardStats = itShard->second;

                // Add stage
                jsonValue.SetInt(shard);
                jSonShard.AddMember(rapidjson::Value("sh"), jsonValue, jSonDocument.GetAllocator());

                // Add stage elapsed time
                jsonValue.SetInt64(shardStats.elapsed);
                jSonShard.AddMember(rapidjson::Value("elap"), jsonValue, jSonDocument.GetAllocator());

                // Add stage elapsed time
                jsonValue.SetInt64(shardStats.maxElapsed);
                jSonShard.AddMember(rapidjson::Value("max"), jsonValue, jSonDocument.GetAllocator());

                // Add stage retries
                jsonValue.SetInt(shardStats.mRetries);
                jSonShard.AddMember(rapidjson::Value("ret"), jsonValue, jSonDocument.GetAllocator());
                shardArray.PushBack(jSonShard, jSonDocument.GetAllocator());
            }
        }

        jsonValue.SetInt(stage);
        jSonStage.AddMember(rapidjson::Value("stg"), jsonValue, jSonDocument.GetAllocator());

        std::string tag = shardStageToString(stage);
        rapidjson::Value stageTag(rapidjson::kStringType);
        stageTag.SetString(tag.c_str(), static_cast<unsigned int>(tag.length()), jSonDocument.GetAllocator());
        jSonStage.AddMember(rapidjson::Value("tag"), stageTag, jSonDocument.GetAllocator());

        jSonStage.AddMember(rapidjson::Value("sa"), shardArray, jSonDocument.GetAllocator());
        shardStagesArray.PushBack(jSonStage, jSonDocument.GetAllocator());
    }

    // Add number of nodes
    jsonValue.SetUint64(mNumNodes);
    jSonObject.AddMember(rapidjson::Value("nn"), jsonValue, jSonDocument.GetAllocator());

    // Add number of contacts
    jsonValue.SetInt64(mNumContacts);
    jSonObject.AddMember(rapidjson::Value("ncn"), jsonValue, jSonDocument.GetAllocator());

    // Add number of chats
    jsonValue.SetInt64(mNumChats);
    jSonObject.AddMember(rapidjson::Value("nch"), jsonValue, jSonDocument.GetAllocator());

    // Add number of contacts
    jsonValue.SetInt64(mInitState);
    jSonObject.AddMember(rapidjson::Value("sid"), jsonValue, jSonDocument.GetAllocator());

    // Add init stats version
    uint32_t version = INITSTATSVERSION;
    jsonValue.SetUint(version);
    jSonObject.AddMember(rapidjson::Value("v"), jsonValue, jSonDocument.GetAllocator());

    // Add total elapsed
    jsonValue.SetInt64(totalElapsed);
    jSonObject.AddMember(rapidjson::Value("telap"), jsonValue, jSonDocument.GetAllocator());

    // Add stages array
    jSonObject.AddMember(rapidjson::Value("stgs"), stageArray, jSonDocument.GetAllocator());

    // Add sharded stages array
    jSonObject.AddMember(rapidjson::Value("shstgs"), shardStagesArray, jSonDocument.GetAllocator());

    jSonDocument.PushBack(jSonObject, jSonDocument.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    jSonDocument.Accept(writer);
    result.assign(buffer.GetString(), buffer.GetSize());
    return result;
}

KarereScheduledFlags::KarereScheduledFlags(const unsigned long numericValue)
    : mega::ScheduledFlags(numericValue)
{}

KarereScheduledFlags::KarereScheduledFlags(const KarereScheduledFlags *ksf)
    : mega::ScheduledFlags(ksf)
{}

KarereScheduledFlags::KarereScheduledFlags(const mega::MegaScheduledFlags *msf)
    : mega::ScheduledFlags(msf ? msf->getNumericValue() : mega::ScheduledFlags::schedEmptyFlags)
{}

KarereScheduledRules::KarereScheduledRules(const int freq, const int interval, const mega::m_time_t until,
                                           const karere_rules_vector* byWeekDay,
                                           const karere_rules_vector* byMonthDay,
                                           const karere_rules_map* byMonthWeekDay)
    : mega::ScheduledRules(freq, interval, until, byWeekDay, byMonthDay, byMonthWeekDay)
{}

KarereScheduledRules::KarereScheduledRules(const KarereScheduledRules *ksr)
    : mega::ScheduledRules(ksr)
{}

KarereScheduledRules::KarereScheduledRules(const mega::MegaScheduledRules *rules)
    : KarereScheduledRules(rules->freq(), rules->interval(), rules->until(),
                           rules->byWeekDay() ? std::unique_ptr<mega::MegaSmallIntVector>(
                                   dynamic_cast<const mega::MegaIntegerListPrivate*>(rules->byWeekDay())->toByteList()
                                   ).get()
                               : nullptr,
                           rules->byMonthDay() ? std::unique_ptr<mega::MegaSmallIntVector>(
                                   dynamic_cast<const mega::MegaIntegerListPrivate*>(rules->byMonthDay())->toByteList()
                                   ).get()
                               : nullptr,
                           rules->byMonthWeekDay() ? std::unique_ptr<mega::MegaSmallIntMap>(
                                   dynamic_cast<const mega::MegaIntegerMapPrivate*>(rules->byMonthWeekDay())->toByteMap()
                                   ).get()
                               : nullptr)
{}

void KarereScheduledRules::setByWeekDay(const karere_rules_vector* byWD)
{
    mByWeekDay.reset(byWD ? new karere_rules_vector(*byWD) : nullptr);
}
void KarereScheduledRules::setByMonthDay(const karere_rules_vector* byMD)
{
    mByMonthDay.reset(byMD ? new karere_rules_vector(*byMD) : nullptr);
}
void KarereScheduledRules::setByMonthWeekDay(const karere_rules_map* byMWD)
{
    mByMonthWeekDay.reset(byMWD ? new karere_rules_map(*byMWD) : nullptr);
}

bool KarereScheduledRules::equalTo(const mega::MegaScheduledRules* r) const
{
    const auto psr = dynamic_cast<const mega::MegaScheduledRulesPrivate*>(r);

    return psr && mega::ScheduledRules::equalTo(psr->getSdkScheduledRules().get());
}

mega::MegaScheduledRules* KarereScheduledRules::getMegaScheduledRules() const
{
    const std::unique_ptr<mega::MegaIntegerListPrivate> auxByWD (
        byWeekDay() ? new mega::MegaIntegerListPrivate(*byWeekDay()) : nullptr);
    const std::unique_ptr<mega::MegaIntegerListPrivate> auxByMD (
        byMonthDay() ? new mega::MegaIntegerListPrivate(*byMonthDay()) : nullptr);
    const std::unique_ptr<mega::MegaIntegerMapPrivate> auxByMWD (
        byMonthWeekDay() ? new mega::MegaIntegerMapPrivate(*byMonthWeekDay()) : nullptr);

    return mega::MegaScheduledRules::createInstance(freq(), interval(), until(),
                                                    auxByWD.get(), auxByMD.get(), auxByMWD.get());
}

bool KarereScheduledRules::serialize(Buffer& out) const
{
    std::string aux;
    bool ret = mega::ScheduledRules::serialize(aux);
    out.append(aux.data(), aux.size());

    return ret;
}

KarereScheduledRules* KarereScheduledRules::unserialize(const Buffer& in)
{
    if (in.empty())  { return nullptr; }

    std::string aux(in.buf(), in.dataSize());
    std::unique_ptr<mega::ScheduledRules> sr(mega::ScheduledRules::unserialize(aux));
    if (!sr)         { return nullptr; }

    return new KarereScheduledRules(sr->freq(), sr->interval(), sr->until(),
                                    sr->byWeekDay(), sr->byMonthDay(), sr->byMonthWeekDay());
}

KarereScheduledMeeting::KarereScheduledMeeting(const karere::Id& chatid, const karere::Id& organizerid,
                                               const std::string& timezone, const mega::m_time_t startDateTime,
                                               const mega::m_time_t endDateTime, const std::string& title,
                                               const std::string& description, const karere::Id& schedId,
                                               const karere::Id& parentSchedId, const int cancelled,
                                               const std::string& attributes, const mega::m_time_t overrides,
                                               const KarereScheduledFlags* flags, const KarereScheduledRules* rules)
    : mega::ScheduledMeeting(chatid, timezone, startDateTime, endDateTime, title, description, organizerid, schedId,
                             parentSchedId, cancelled, attributes, overrides, flags, rules)
{}

KarereScheduledMeeting::KarereScheduledMeeting(const KarereScheduledMeeting *ksm)
    : mega::ScheduledMeeting(ksm)
{}

KarereScheduledMeeting::KarereScheduledMeeting(const mega::MegaScheduledMeeting *msm)
    : mega::ScheduledMeeting(msm->chatid(),
                             msm->timezone() ? msm->timezone() : std::string(),
                             msm->startDateTime(), msm->endDateTime(),
                             msm->title() ? msm->title() : std::string(),
                             msm->description() ? msm->description() : std::string(),
                             msm->organizerUserid(), msm->schedId(), msm->parentSchedId(), msm->cancelled(),
                             msm->attributes() ? msm->attributes() : std::string(),
                             msm->overrides(),
                             msm->flags() ? std::make_unique<KarereScheduledFlags>(msm->flags()).get() : nullptr,
                             msm->rules() ? std::make_unique<KarereScheduledRules>(msm->rules()).get() : nullptr)
{}

const KarereScheduledFlags* KarereScheduledMeeting::flags() const { return dynamic_cast<const KarereScheduledFlags*>(mega::ScheduledMeeting::flags()); }
const KarereScheduledRules* KarereScheduledMeeting::rules() const { return dynamic_cast<const KarereScheduledRules*>(mega::ScheduledMeeting::rules()); }

KarereScheduledMeeting::sched_bs_t KarereScheduledMeeting::compare(const mega::MegaScheduledMeeting* sm) const
{
    // scheduled meeting Handle and chatid can't change
    sched_bs_t bs = 0;
    if (parentSchedId() != sm->parentSchedId())                                      { bs[SC_PARENT] = true; }
    if (timezone().compare(sm->timezone() ? sm->timezone() : std::string()))         { bs[SC_TZONE] = true; }
    if (cancelled() != sm->cancelled())                                              { bs[SC_CANC] = true; }
    if (startDateTime() != sm->startDateTime())                                      { bs[SC_START] = true; }
    if (endDateTime() != sm->endDateTime())                                          { bs[SC_END] = true; }
    if (title().compare(sm->title() ? sm->title(): std::string()))                   { bs[SC_TITLE] = true; }
    if (description().compare(sm->description() ? sm->description(): std::string())) { bs[SC_DESC] = true; }
    if (attributes().compare(sm->attributes() ? sm->attributes(): std::string()))    { bs[SC_ATTR] = true; }
    if (overrides() != sm->overrides())                                              { bs[SC_OVERR] = true; }

    const std::unique_ptr<mega::MegaScheduledFlags> smFlags(sm->flags());
    if (flags() || smFlags)
    {
        if (!flags() || !smFlags)                                                    { bs[SC_FLAGS] = true; }
        else if (!flags()->equalTo(smFlags.get()))                                   { bs[SC_FLAGS] = true; }
    }

    const std::unique_ptr<mega::MegaScheduledRules> smRules(sm->rules());
    if (rules() || smRules)
    {
        if (!rules() || !smRules)                                                    { bs[SC_RULES] = true; }
        else if (!rules()->equalTo(smRules.get()))                                   { bs[SC_RULES] = true; }
    }
    return bs;
}

/* class KarereScheduledMeetingOccurr */
KarereScheduledMeetingOccurr::KarereScheduledMeetingOccurr(const karere::Id& schedId, const karere::Id& parentSchedId, const std::string& timezone, mega::m_time_t startDateTime, mega::m_time_t endDateTime, mega::m_time_t overrides, int cancelled)
    : mSchedId(schedId),
      mParentSchedId(parentSchedId),
      mOverrides(overrides),
      mTimezone(timezone),
      mStartDateTime(startDateTime),
      mEndDateTime(endDateTime),
      mCancelled(cancelled)
{
}

KarereScheduledMeetingOccurr::KarereScheduledMeetingOccurr(const KarereScheduledMeetingOccurr* scheduledMeeting)
    : mSchedId(scheduledMeeting->schedId()),
      mParentSchedId(scheduledMeeting->parentSchedId()),
      mOverrides(scheduledMeeting->overrides()),
      mTimezone(scheduledMeeting->timezone()),
      mStartDateTime(scheduledMeeting->startDateTime()),
      mEndDateTime(scheduledMeeting->endDateTime()),
      mCancelled(scheduledMeeting->cancelled())
{
}

KarereScheduledMeetingOccurr::KarereScheduledMeetingOccurr(const mega::MegaScheduledMeeting* scheduledMeeting)
    : mSchedId(scheduledMeeting->schedId()),
      mParentSchedId(scheduledMeeting->parentSchedId()),
      mOverrides(scheduledMeeting->overrides()),
      mTimezone(scheduledMeeting->timezone() ? scheduledMeeting->timezone() : std::string()),
      mStartDateTime(scheduledMeeting->startDateTime()),
      mEndDateTime(scheduledMeeting->endDateTime()),
      mCancelled(scheduledMeeting->cancelled())
{
}

KarereScheduledMeetingOccurr* KarereScheduledMeetingOccurr::copy() const
{
   return new KarereScheduledMeetingOccurr(this);
}

KarereScheduledMeetingOccurr::~KarereScheduledMeetingOccurr()
{
}

const karere::Id& KarereScheduledMeetingOccurr::schedId() const                 { return mSchedId; }
const karere::Id& KarereScheduledMeetingOccurr::parentSchedId() const           { return mParentSchedId; }
const std::string& KarereScheduledMeetingOccurr::timezone() const               { return mTimezone; }
::mega::m_time_t KarereScheduledMeetingOccurr::startDateTime() const            { return mStartDateTime; }
::mega::m_time_t KarereScheduledMeetingOccurr::endDateTime() const              { return mEndDateTime; }
::mega::m_time_t KarereScheduledMeetingOccurr::overrides() const                { return mOverrides; }
int KarereScheduledMeetingOccurr::cancelled() const                             { return mCancelled; }
}
