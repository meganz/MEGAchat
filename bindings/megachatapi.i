#ifdef SWIGJAVA
#define __ANDROID__
#endif

%module(directors="1") megachat
%{

/* Includes the header */
#include "megachatapi.h"

extern JavaVM *MEGAjvm;

extern jstring strEncodeUTF8;
extern jclass applicationClass;
extern jclass clsString;
extern jmethodID ctorString;
extern jmethodID deviceListMID;
extern jmethodID getBytes;
extern jmethodID startVideoCaptureMID;
extern jmethodID stopVideoCaptureMID;
extern jobject surfaceTextureHelper;
extern int sdkVersion;

%}
%import "megaapi.h"

#ifdef SWIGJAVA

//Use compilation-time constants in Java
%javaconst(1);

%typemap(out) char*
%{
    if ($1)
    {
        int len = strlen($1);
        jbyteArray $1_array = jenv->NewByteArray(len);
        jenv->SetByteArrayRegion($1_array, 0, len, (const jbyte*)$1);
        $result = (jstring) jenv->NewObject(clsString, ctorString, $1_array, strEncodeUTF8);
        jenv->DeleteLocalRef($1_array);
    }
%}

%typemap(in) char*
%{
    jbyteArray $1_array;
    $1 = 0;
    if ($input)
    {
        $1_array = (jbyteArray) jenv->CallObjectMethod($input, getBytes, strEncodeUTF8);
        jsize $1_size = jenv->GetArrayLength($1_array);
        $1 = new char[$1_size + 1];
        if ($1_size)
        {
            jenv->GetByteArrayRegion($1_array, 0, $1_size, (jbyte*)$1);
        }
        $1[$1_size] = '\0';
    }
%}

%typemap(freearg) char*
%{
    if ($1)
    {
        delete [] $1;
        jenv->DeleteLocalRef($1_array);
    }
%}

%typemap(directorin,descriptor="Ljava/lang/String;") char *
%{
    $input = 0;
    if ($1)
    {
        int len = strlen($1);
        jbyteArray $1_array = jenv->NewByteArray(len);
        jenv->SetByteArrayRegion($1_array, 0, len, (const jbyte*)$1);
        $input = (jstring) jenv->NewObject(clsString, ctorString, $1_array, strEncodeUTF8);
        jenv->DeleteLocalRef($1_array);
    }
    Swig::LocalRefGuard $1_refguard(jenv, $input);
%}

%apply (char *STRING, size_t LENGTH) {(char *buffer, size_t size)};

#if SWIG_VERSION < 0x030012
%typemap(directorargout) (char *buffer, size_t size)
%{
    jenv->DeleteLocalRef($input);
%}
#else
%typemap(directorargout) (char *buffer, size_t size)
%{
   // not copying the buffer back to improve performance
%}
#endif

//Make the "delete" method protected
%typemap(javadestruct, methodname="delete", methodmodifiers="protected synchronized") SWIGTYPE 
{   
    if (swigCPtr != 0) {
      if (swigCMemOwn) {
        swigCMemOwn = false;
        $jnicall;
      }
      swigCPtr = 0;
    }
}

%javamethodmodifiers copy ""

#endif

//Generate inheritable wrappers for listener objects
%feature("director") megachat::MegaChatRequestListener;
%feature("director") megachat::MegaChatCallListener;
%feature("director") megachat::MegaChatVideoListener;
%feature("director") megachat::MegaChatListener;
%feature("director") megachat::MegaChatLogger;
%feature("director") megachat::MegaChatRoomListener;
%feature("director") megachat::MegaChatNotificationListener;
%feature("director") megachat::MegaChatNodeHistoryListener;
%feature("director") megachat::MegaChatScheduledMeetingListener;

%newobject megachat::MegaChatRequest::copy;
%newobject megachat::MegaChatError::copy;
%newobject megachat::MegaChatMessage::copy;
%newobject megachat::MegaChatRoom::copy;
%newobject megachat::MegaChatCall::copy;
%newobject megachat::MegaChatListItem::copy;
%newobject megachat::MegaChatScheduledRules::copy;
%newobject megachat::MegaChatScheduledFlags::copy;
%newobject megachat::MegaChatScheduledMeeting::copy;
%newobject megachat::MegaChatScheduledMeetingList::copy;
%newobject megachat::MegaChatScheduledMeetingOccurrList::copy;
%newobject megachat::MegaChatWaitingRoom::copy;
%newobject megachat::MegaChatRoomList::copy;
%newobject megachat::MegaChatListItemList::copy;
%newobject megachat::MegaChatPeerList::copy;
%newobject megachat::MegaChatRichPreview::copy;
%newobject megachat::MegaChatGeolocation::copy;
%newobject megachat::MegaChatGiphy::copy;
%newobject megachat::MegaChatContainsMeta::copy;
%newobject megachat::MegaChatSession::copy;
%newobject megachat::MegaChatPresenceConfig::copy;

%newobject megachat::MegaChatApi::getMessageReactions;
%newobject megachat::MegaChatApi::getReactionUsers;
%newobject megachat::MegaChatApi::getScheduledMeetingsByChat;
%newobject megachat::MegaChatApi::getScheduledMeeting;
%newobject megachat::MegaChatApi::getAllScheduledMeetings;
%newobject megachat::MegaChatApi::getUserFirstnameFromCache;
%newobject megachat::MegaChatApi::getUserLastnameFromCache;
%newobject megachat::MegaChatApi::getUserEmailFromCache;
%newobject megachat::MegaChatApi::getUserFullnameFromCache;
%newobject megachat::MegaChatApi::getUserAliasFromCache;
%newobject megachat::MegaChatApi::getUserAliasesFromCache;
%newobject megachat::MegaChatApi::getContactEmail;
%newobject megachat::MegaChatApi::getMyFirstname;
%newobject megachat::MegaChatApi::getMyLastname;
%newobject megachat::MegaChatApi::getMyFullname;
%newobject megachat::MegaChatApi::getMyEmail;
%newobject megachat::MegaChatApi::getChatRooms;
%newobject megachat::MegaChatApi::getChatRoomsByType;
%newobject megachat::MegaChatApi::getChatRoomByUser;
%newobject megachat::MegaChatApi::getChatListItems;
%newobject megachat::MegaChatApi::getChatListItemsByPeers;
%newobject megachat::MegaChatApi::getChatListItemsByType;
%newobject megachat::MegaChatApi::getChatListItem;
%newobject megachat::MegaChatApi::getActiveChatListItems;
%newobject megachat::MegaChatApi::getInactiveChatListItems;
%newobject megachat::MegaChatApi::getArchivedChatListItems;
%newobject megachat::MegaChatApi::getUnreadChatListItems;
%newobject megachat::MegaChatApi::getChatRoom;
%newobject megachat::MegaChatApi::getMessage;
%newobject megachat::MegaChatApi::getMessageFromNodeHistory;
%newobject megachat::MegaChatApi::getManualSendingMessage;
%newobject megachat::MegaChatApi::sendMessage;
%newobject megachat::MegaChatApi::sendGiphy;
%newobject megachat::MegaChatApi::attachContacts;
%newobject megachat::MegaChatApi::forwardContact;
%newobject megachat::MegaChatApi::sendGeolocation;
%newobject megachat::MegaChatApi::editGeolocation;
%newobject megachat::MegaChatApi::revokeAttachmentMessage;
%newobject megachat::MegaChatApi::editMessage;
%newobject megachat::MegaChatApi::deleteMessage;
%newobject megachat::MegaChatApi::getLastMessageSeen;
%newobject megachat::MegaChatApi::getChatCall;
%newobject megachat::MegaChatApi::getChatCallByCallId;

typedef long long time_t;
typedef long long uint64_t;
typedef long long int64_t;

/* generate the wrappers */
%include "megachatapi.h"

