#include <mclc_enums_to_string.h>

#include <megachatapi.h>
namespace c = ::megachat;

#include <cassert>

namespace mclc::clc_etos
{

std::string msgTypeToString(const int msgType)
{
    switch (msgType)
    {
        case c::MegaChatMessage::TYPE_UNKNOWN: return "TYPE_UNKNOWN";
        case c::MegaChatMessage::TYPE_INVALID: return "TYPE_INVALID";
        case c::MegaChatMessage::TYPE_NORMAL: return "TYPE_NORMAL";
        case c::MegaChatMessage::TYPE_ALTER_PARTICIPANTS: return "TYPE_ALTER_PARTICIPANTS";
        case c::MegaChatMessage::TYPE_TRUNCATE: return "TYPE_TRUNCATE";
        case c::MegaChatMessage::TYPE_PRIV_CHANGE: return "TYPE_PRIV_CHANGE";
        case c::MegaChatMessage::TYPE_CHAT_TITLE: return "TYPE_CHAT_TITLE";
        case c::MegaChatMessage::TYPE_CALL_ENDED: return "TYPE_CALL_ENDED";
        case c::MegaChatMessage::TYPE_CALL_STARTED: return "TYPE_CALL_STARTED";
        case c::MegaChatMessage::TYPE_PUBLIC_HANDLE_CREATE: return "TYPE_PUBLIC_HANDLE_CREATE";
        case c::MegaChatMessage::TYPE_PUBLIC_HANDLE_DELETE: return "TYPE_PUBLIC_HANDLE_DELETE";
        case c::MegaChatMessage::TYPE_SET_PRIVATE_MODE: return "TYPE_SET_PRIVATE_MODE";
        case c::MegaChatMessage::TYPE_SET_RETENTION_TIME: return "TYPE_SET_RETENTION_TIME";
        case c::MegaChatMessage::TYPE_NODE_ATTACHMENT: return "TYPE_NODE_ATTACHMENT";
        case c::MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT: return "TYPE_REVOKE_NODE_ATTACHMENT";
        case c::MegaChatMessage::TYPE_CONTACT_ATTACHMENT: return "TYPE_CONTACT_ATTACHMENT";
        case c::MegaChatMessage::TYPE_CONTAINS_META: return "TYPE_CONTAINS_META";
        case c::MegaChatMessage::TYPE_VOICE_CLIP: return "TYPE_VOICE_CLIP";
        default: assert(false); return "Invalid Msg Type (" + std::to_string(msgType) + ")";
    }
#ifndef WIN32
    return {}; // warning C4702: unreachable code
#endif
}

std::string msgStatusToString(const int msgStatus)
{
    switch (msgStatus)
    {
        case c::MegaChatMessage::STATUS_UNKNOWN: return "STATUS_UNKNOWN";
        case c::MegaChatMessage::STATUS_SENDING: return "STATUS_SENDING";
        case c::MegaChatMessage::STATUS_SENDING_MANUAL: return "STATUS_SENDING_MANUAL";
        case c::MegaChatMessage::STATUS_SERVER_RECEIVED: return "STATUS_SERVER_RECEIVED";
        case c::MegaChatMessage::STATUS_SERVER_REJECTED: return "STATUS_SERVER_REJECTED";
        case c::MegaChatMessage::STATUS_DELIVERED: return "STATUS_DELIVERED";
        case c::MegaChatMessage::STATUS_NOT_SEEN: return "STATUS_NOT_SEEN";
        case c::MegaChatMessage::STATUS_SEEN: return "STATUS_SEEN";
        default: assert(false); return "Invalid Msg Status (" + std::to_string(msgStatus) + ")";
    }
#ifndef WIN32
    return {}; // warning C4702: unreachable code
#endif
}
}
