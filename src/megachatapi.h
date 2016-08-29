/**
 * @file megachatapi.h
 * @brief Public header file of the intermediate layer for the MEGA Chat C++ SDK.
 *
 * (c) 2013-2016 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef MEGACHATAPI_H
#define MEGACHATAPI_H


#include <megaapi.h>

namespace mega { class MegaApi; }
using namespace mega;

namespace megachat
{

class MegaChatApi;
class MegaChatApiImpl;
class MegaChatRequest;
class MegaChatRequestListener;
class MegaChatError;
class MegaChatRoom;
class MegaChatCall;
class MegaChatCallListener;
class MegaChatVideoListener;
class MegaChatGlobalListener;

class MegaChatCall
{
public:
    enum
    {
        CALL_STATUS_CONNECTING = 0,
        CALL_STATUS_RINGING,
        CALL_STATUS_CONNECTED,
        CALL_STATUS_DISCONNECTED
    };

    virtual ~MegaChatCall();
    virtual MegaChatCall *copy();

    virtual int getStatus() const;
    virtual int getTag() const;
    virtual MegaHandle getContactHandle() const;
};

class MegaChatVideoListener
{
public:
    virtual ~MegaChatVideoListener() {}

    virtual void onChatVideoData(MegaChatApi *api, MegaChatCall *chatCall, int width, int height, char*buffer);
};

class MegaChatCallListener
{
public:
    virtual ~MegaChatCallListener() {}

    virtual void onChatCallStart(MegaChatApi* api, MegaChatCall *call);
    virtual void onChatCallStateChange(MegaChatApi *api, MegaChatCall *call);
    virtual void onChatCallTemporaryError(MegaChatApi* api, MegaChatCall *call, MegaChatError* error);
    virtual void onChatCallFinish(MegaChatApi* api, MegaChatCall *call, MegaChatError* error);
};

class MegaChatRoom
{
public:
    virtual ~MegaChatRoom() {}
};

class MegaChatRoomListener
{
public:
    virtual ~MegaChatRoomListener() {}


};

/**
 * @brief List of MegaChatRoom objects
 *
 * A MegaChatRoomList has the ownership of the MegaChatRoom objects that it contains, so they will be
 * only valid until the MegaChatRoomList is deleted. If you want to retain a MegaChatRoom returned by
 * a MegaChatRoomList, use MegaChatRoom::copy.
 *
 * Objects of this class are immutable.
 */
class MegaChatRoomList
{
public:
    virtual ~MegaChatRoomList() {}

    virtual MegaChatRoomList *copy() const;

    /**
     * @brief Returns the MegaChatRoom at the position i in the MegaChatRoomList
     *
     * The MegaChatRoomList retains the ownership of the returned MegaChatRoom. It will be only valid until
     * the MegaChatRoomList is deleted.
     *
     * If the index is >= the size of the list, this function returns NULL.
     *
     * @param i Position of the MegaChatRoom that we want to get for the list
     * @return MegaChatRoom at the position i in the list
     */
    virtual const MegaChatRoom *get(unsigned int i)  const;

    /**
     * @brief Returns the number of MegaChatRooms in the list
     * @return Number of MegaChatRooms in the list
     */
    virtual int size() const;

};


/**
 * @brief Provides information about an asynchronous request
 *
 * Most functions in this API are asynchonous, except the ones that never require to
 * contact MEGA servers. Developers can use listeners (MegaListener, MegaChatRequestListener)
 * to track the progress of each request. MegaChatRequest objects are provided in callbacks sent
 * to these listeners and allow developers to know the state of the request, their parameters
 * and their results.
 *
 * Objects of this class aren't live, they are snapshots of the state of the request
 * when the object is created, they are immutable.
 *
 * These objects have a high number of 'getters', but only some of them return valid values
 * for each type of request. Documentation of each request specify which fields are valid.
 *
 */
class MegaChatRequest
{
public:
    enum {
        TYPE_CONNECT,   // connect to chatd (call it after login+fetchnodes with MegaApi)
        TYPE_DELETE,    // delete MegaChatApi instance
        TYPE_SET_ONLINE_STATUS,
        TYPE_START_CHAT_CALL, TYPE_ANSWER_CHAT_CALL,
        TYPE_MUTE_CHAT_CALL, TYPE_HANG_CHAT_CALL,
        TYPE_SEND_MESSAGE, TYPE_EDIT_MESSAGE, TYPE_DELETE_MESSAGE,
        TYPE_CREATE_CHATROOM, TYPE_REMOVE_CHATROOM,
        TYPE_INVITE_TO_CHATROOM, TYPE_UPDATE_PEER_PERMISSIONS,
        TYPE_GRANT_ACCESS, TYPE_REMOVE_ACCESS,
        TYPE_GET_HISTORY, TYPE_CHAT_TRUNCATE,
        TYPE_SHARE_CONTACT,
        TYPE_EDIT_CHATROOM_NAME, TYPE_EDIT_CHATROOM_PIC
    };

    virtual ~MegaChatRequest();

    /**
     * @brief Creates a copy of this MegaChatRequest object
     *
     * The resulting object is fully independent of the source MegaChatRequest,
     * it contains a copy of all internal attributes, so it will be valid after
     * the original object is deleted.
     *
     * You are the owner of the returned object
     *
     * @return Copy of the MegaChatRequest object
     */
    virtual MegaChatRequest *copy();

    /**
     * @brief Returns the type of request associated with the object
     * @return Type of request associated with the object
     */
    virtual int getType() const;

    /**
     * @brief Returns a readable string that shows the type of request
     *
     * This function returns a pointer to a statically allocated buffer.
     * You don't have to free the returned pointer
     *
     * @return Readable string showing the type of request
     */
    virtual const char *getRequestString() const;

    /**
     * @brief Returns a readable string that shows the type of request
     *
     * This function provides exactly the same result as MegaChatRequest::getRequestString.
     * It's provided for a better Java compatibility
     *
     * @return Readable string showing the type of request
     */
    virtual const char* toString() const;

    /**
     * @brief Returns the tag that identifies this request
     *
     * The tag is unique for the MegaChatApi object that has generated it only
     *
     * @return Unique tag that identifies this request
     */
    virtual int getTag() const;

    /**
     * @brief Returns a number related to this request
     * @return Number related to this request
     */
    virtual long long getNumber() const;

    /**
     * @brief Return the number of times that a request has temporarily failed
     * @return Number of times that a request has temporarily failed
     */
    virtual int getNumRetry() const;
};

/**
 * @brief Interface to receive information about requests
 *
 * All requests allows to pass a pointer to an implementation of this interface in the last parameter.
 * You can also get information about all requests using MegaApi::addRequestListener
 *
 * MegaListener objects can also receive information about requests
 *
 * This interface uses MegaChatRequest objects to provide information of requests. Take into account that not all
 * fields of MegaChatRequest objects are valid for all requests. See the documentation about each request to know
 * which fields contain useful information for each one.
 *
 */
class MegaChatRequestListener
{
public:
    /**
     * @brief This function is called when a request is about to start being processed
     *
     * The SDK retains the ownership of the request parameter.
     * Don't use it after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     */
    virtual void onRequestStart(MegaChatApi* api, MegaChatRequest *request);

    /**
     * @brief This function is called when a request has finished
     *
     * There won't be more callbacks about this request.
     * The last parameter provides the result of the request. If the request finished without problems,
     * the error code will be API_OK
     *
     * The SDK retains the ownership of the request and error parameters.
     * Don't use them after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     * @param e Error information
     */
    virtual void onRequestFinish(MegaChatApi* api, MegaChatRequest *request, MegaChatError* e);

    /**
     * @brief This function is called to inform about the progres of a request
         *
         * The SDK retains the ownership of the request parameter.
         * Don't use it after this functions returns.
         *
         * The api object is the one created by the application, it will be valid until
         * the application deletes it.
         *
         * @param api MegaChatApi object that started the request
         * @param request Information about the request
         * @see MegaChatRequest::getTotalBytes MegaChatRequest::getTransferredBytes
         */
    virtual void onRequestUpdate(MegaChatApi*api, MegaChatRequest *request);

    /**
     * @brief This function is called when there is a temporary error processing a request
     *
     * The request continues after this callback, so expect more MegaChatRequestListener::onRequestTemporaryError or
     * a MegaChatRequestListener::onRequestFinish callback
     *
     * The SDK retains the ownership of the request and error parameters.
     * Don't use them after this functions returns.
     *
     * The api object is the one created by the application, it will be valid until
     * the application deletes it.
     *
     * @param api MegaChatApi object that started the request
     * @param request Information about the request
     * @param error Error information
     */
    virtual void onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError* error);
    virtual ~MegaChatRequestListener();
};


/**
 * @brief Provides information about an error
 */
class MegaChatError
{
public:
    enum {
        ERROR_OK = 0,
        ERROR_UNKNOWN = -1,
        ERROR_ARGS = -2
    };

    MegaChatError() {}
    virtual ~MegaChatError() {}

    /**
     * @brief Returns the error code associated with this MegaChatError
     * @return Error code associated with this MegaChatError
     */
    virtual int getErrorCode() const = 0;

    /**
     * @brief Returns the type of the error associated with this MegaChatError
     * @return Type of the error associated with this MegaChatError
     */
    virtual int getErrorType() const = 0;

    /**
     * @brief Returns a readable description of the error
     *
     * @return Readable description of the error
     */
    virtual const char* getErrorString() const = 0;

    /**
     * @brief Returns a readable description of the error
     *
     * This function provides exactly the same result as MegaChatError::getErrorString.
     * It's provided for a better Java compatibility
     *
     * @return Readable description of the error
     */
    virtual const char* toString() const = 0;
};

class MegaChatApi
{

public:
    enum Status
    {
        STATUS_OFFLINE    = 0,
        STATUS_BUSY       = 1,
        STATUS_AWAY       = 2,
        STATUS_ONLINE     = 3,
        STATUS_CHATTY     = 4
    };


    // chat will reuse an existent megaApi instance (ie. the one for cloud storage)
    MegaChatApi(MegaApi *megaApi);

//    // chat will use its own megaApi, a new instance
//    MegaChatApi(const char *appKey, const char* appDir);

    virtual ~MegaChatApi();


    // ============= Requests ================

    /**
     * @brief Performs karere-only login, assuming the Mega SDK is already logged in
     * with an existing session.
     *
     * After calling this function, the chat-engine is fully initialized (despite it's
     * not connected yet, call MegaChatApi::connect) and the application can request the
     * required objects, such as MegaChatApi::getChatRooms, to show the GUI to the user.
     *
     * @note: until the MegaChatApi::connect function is called, MegaChatApi will operate
     * in offline mode (cannot send/receive any message or call)
     */
    void init();

    /**
     * @brief Establish the connection with chat-related servers (chatd, XMPP and Gelb).
     *
     * This function must be called only after calling:
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *  - MegaChatApi::init to initialize the chat engine
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void connect(MegaChatRequestListener *listener = NULL);

    /**
     * @brief Set your online status.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHAT_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaRequest::getNumber - Returns the new status of the user in chat.
     *
     * @param status Online status in the chat.
     *
     * It can be one of the following values:
     * - STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - STATUS_BUSY = 2
     * The user is busy and don't want to be disturbed.
     *
     * - STATUS_AWAY = 3
     * The user is away and might not answer.
     *
     * - STATUS_ONLINE = 4
     * The user is connected and online.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void setOnlineStatus(int status, MegaChatRequestListener *listener = NULL);

    MegaChatRoomList* getChatRooms();

    // Audio/Video device management
    MegaStringList *getChatAudioInDevices();
    MegaStringList *getChatVideoInDevices();
    bool setChatAudioInDevice(const char *device);
    bool setChatVideoInDevice(const char *device);

    // Call management
    void startChatCall(MegaUser *peer, bool enableVideo = true, MegaChatRequestListener *listener = NULL);
    void answerChatCall(MegaChatCall *call, bool accept, MegaChatRequestListener *listener = NULL);
    void hangAllChatCalls();

    // Listeners
    void addChatCallListener(MegaChatCallListener *listener);
    void removeChatCallListener(MegaChatCallListener *listener);
    void addChatLocalVideoListener(MegaChatVideoListener *listener);
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);
    void addChatRoomListener(MegaChatRoomListener *listener);
    void removeChatRoomListener(MegaChatRoomListener *listener);

    /**
     * @brief Register a listener to receive all events about requests
     *
     * You can use MegaChatApi::removeChatRequestListener to stop receiving events.
     *
     * @param listener Listener that will receive all events about requests
     */
    void addChatRequestListener(MegaChatRequestListener* listener);

    /**
     * @brief Unregister a MegaChatRequestListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    void removeChatRequestListener(MegaChatRequestListener* listener);


private:
    MegaChatApiImpl *pImpl;
};

class MegaChatGlobalListener
{
public:

//    /**
//     * @brief This function is called when the chat engine is fully initialized
//     *
//     * At this moment, the list of chats is up to date and it can be retrieved by the app.
//     *
//     * @note The chat engine is able to work offline, so the connection to the
//     * the chat servers is not necessarily established when this function is called.
//     *
//     * @param api MegaChatApi connected to the account
//     */
//    virtual void onChatCurrent(MegaChatApi* api);

    /**
     * @brief onOnlineStatusUpdate
     *
     * @param api MegaChatApi connected to the account
     * @param status New status of the account
     *
     * It can be one of the following values:
     * - STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - STATUS_BUSY = 2
     * The user is busy and don't want to be disturbed.
     *
     * - STATUS_AWAY = 3
     * The user is away and might not answer.
     *
     * - STATUS_ONLINE = 4
     * The user is connected and online.
     *
     */
    virtual void onOnlineStatusUpdate(MegaChatApi* api, MegaChatApi::Status status);

    /**
     * @brief This funtion is called when there are new or updated chats in the account
     *
     * When the chat engine is fully initialized, this function is also called, but
     * the second parameter will be NULL. @note that it can work offline, so the
     * connection to the chat servers may not have been established yet. Wait for
     * MegaChatRequestListener::onRequestFinish response to ensure the connection is established.
     *
     * The SDK retains the ownership of the MegaChatRoomList in the second parameter. The list
     * and all the MegaChatRoom objects that it contains will be valid until this function returns.
     * If you want to save the list, use MegaChatRoomList::copy. If you want to save only some of
     * the MegaChatRoom objects, use MegaChatRoom::copy for those chats.
     *
     * @param api MegaChatApi connected to the account
     * @param chats List that contains the new or updated chats
     */
    virtual void onChatRoomUpdate(MegaChatApi* api, MegaChatRoomList *chats);
};


}

#endif // MEGACHATAPI_H
