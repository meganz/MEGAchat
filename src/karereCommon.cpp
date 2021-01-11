#include "karereCommon.h"
#include "stringUtils.h"
#include "base/timers.hpp"
#include "megachatapi_impl.h"
#include "waiter/libuvWaiter.h"

#ifndef KARERE_DISABLE_WEBRTC
namespace rtcModule {void globalCleanup(); }
#endif

#ifndef LOGGER_SPRINTF_BUF_SIZE
    #define LOGGER_SPRINTF_BUF_SIZE 10240
#endif

namespace karere
{
const char* gDbSchemaVersionSuffix = "10";
/*
    2 --> +3: invalidate cached chats to reload history (so call-history msgs are fetched)
    3 --> +4: invalidate both caches, SDK + MEGAchat, if there's at least one chat (so deleted chats are re-fetched from API)
    4 --> +5: modify attachment, revoke, contact and containsMeta and create a new table node_history
    5 --> +6: invalidate both caches, SDK + MEGAchat, (so deleted chats are re-fetched from API) if there's at least one chat,
              otherwise modify cache structure to support public chats
    6 --> +7: update keyid for truncate messages in db
    7 --> +8: modify chats and create a new table chat_reactions
    8 --> +9: create table DNS cache
    9 --> +10: create table chat_pending_reactions and modify sendkeys table
*/

bool gCatchException = true;

void globalCleanup()
{
#ifndef KARERE_DISABLE_WEBRTC
    rtcModule::globalCleanup();
#endif
    services_shutdown();
}

void init_uv_timer(void *ctx, uv_timer_t *timer)
{
    uv_timer_init(((::mega::LibuvWaiter *)(((megachat::MegaChatApiImpl *)ctx)->waiter))->eventloop, timer);
}
}
