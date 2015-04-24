/*
 * busConstants.h
 *
 *  Created on: 26/02/2015
 *      Author: michael
 */

#ifndef SRC_BUSCONSTANTS_H_
#define SRC_BUSCONSTANTS_H_

#include "messageBus.h"

#define CONTACT_ADDED_EVENT "contact_added:"
#define CONTACT_DELETED_EVENT "contact_deleted:"
#define CONTACT_CHANGED_EVENT "contact_changed:"
#define CONTACT_JID "contact_jid"
#define CONTACT_STATE "contact_state"
#define CONTACT_OLD_STATE "contact_old_state"

#define ROOM_ADDED_EVENT "room_added:"
#define ROOM_ADDED_JID "room_jid"

#define ROOM_MESSAGE_EVENT "room_message:"
#define ROOM_MESSAGE_CONTENTS "room_message_contents"

#define ROOM_USER_JOIN_EVENT "room_user_join"
#define ROOM_USER_JID "room_user_jid"

#define ERROR_MESSAGE_EVENT "error_message"
#define ERROR_MESSAGE_CONTENTS "error_message_contents"

#define WARNING_MESSAGE_EVENT "warning_message"
#define WARNING_MESSAGE_CONTENTS "warning_message_contents"

#define GENERAL_EVENTS "general_events"
#define GENERAL_EVENTS_CONTENTS "general_events_contents"

#define USER_INFO_EVENT "user_info:"
#define USER_INFO_PUBLIC_KEY "public_key"
#define USER_INFO_EVENT_ERROR "user_info_error:"

#define THIS_USER_INFO_EVENT "this_user_info:"

#define M_BUS_PARAMS  message_bus::ErrorReporter,message_bus::MessageBus,message_bus::DefaultHandler
#define M_MESS_PARAMS message_bus::ErrorReporter

typedef message_bus::SharedMessageBus<M_BUS_PARAMS> MCBus;
typedef message_bus::SharedMessage<M_MESS_PARAMS> MCMessage;
typedef message_bus::MessageListener<M_MESS_PARAMS> MCListener;

static inline void sendBusMessage(const char *type, const char *valName,
        const std::string &message) {
   message_bus::SharedMessage<M_MESS_PARAMS> busMessage(type);
   busMessage->addValue(valName, message);
   CHAT_LOG_DEBUG("sending message type: '%s' with content type: '%s' with contents: '%s'" type, message;
   message_bus::SharedMessageBus<M_BUS_PARAMS>::
       getMessageBus()->alertListeners(type, busMessage);
}

static inline void sendBusMessage(const char *type, const char *valName, const char *message) {
   std::string messStr(message);
   sendBusMessage(type, valName, messStr);
}

static inline void sendErrorMessage(const char *message) {
    sendBusMessage(ERROR_MESSAGE_EVENT, ERROR_MESSAGE_CONTENTS, message);
}

static inline void sendWarningMessage(const char *message) {
    sendBusMessage(WARNING_MESSAGE_EVENT, WARNING_MESSAGE_CONTENTS, message);
}

static inline void sendGeneralMessage(const char *message) {
    sendBusMessage(GENERAL_EVENTS, GENERAL_EVENTS_CONTENTS, message);
}

static inline void sendErrorMessage(const std::string &message) {
    sendBusMessage(ERROR_MESSAGE_EVENT, ERROR_MESSAGE_CONTENTS, message);
}

static inline void sendGeneralMessage(const std::string &message) {
    sendBusMessage(GENERAL_EVENTS, GENERAL_EVENTS_CONTENTS, message);
}

static inline void sendWarningMessage(const std::string &message) {
    sendBusMessage(WARNING_MESSAGE_EVENT, WARNING_MESSAGE_CONTENTS, message);
}

#define VC_BUS__PARAMS message_bus::ErrorReporter,message_bus::MessageBus,message_bus::DefaultHandler
#define VC_MESS_PARAMS message_bus::ErrorReporter

typedef message_bus::SharedMessageBus<VC_BUS__PARAMS> VCBus;
typedef message_bus::SharedMessage<VC_MESS_PARAMS> VCMessage;
typedef message_bus::MessageListener<VC_MESS_PARAMS> VCListener;


#endif /* SRC_BUSCONSTANTS_H_ */
