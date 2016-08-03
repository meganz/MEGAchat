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

namespace megachat
{

using namespace mega;


class MegaApi;
class MegaChatApi;
class MegaChatApiImpl;
class MegaChatRequest;
class MegaChatRequestListener;
class MegaChatCall;
class MegaChatListener;
class MegaChatVideoListener;

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
    virtual void onChatVideoData(MegaChatApi *api, MegaChatCall *chatCall, int width, int height, char*buffer, int size);
};

class MegaChatListener
{
public:
    virtual void onChatCallStart(MegaChatApi* api, MegaChatCall *call);
    virtual void onChatCallStateChange(MegaChatApi *api, MegaChatCall *call);
    virtual void onChatCallTemporaryError(MegaChatApi* api, MegaChatCall *call, MegaError* error);
    virtual void onChatCallFinish(MegaChatApi* api, MegaChatCall *call, MegaError* error);
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
        TYPE_DELETE,
        TYPE_SET_CHAT_STATUS,
        TYPE_START_CHAT_CALL, TYPE_ANSWER_CHAT_CALL
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
     * @brief Returns a readable string that shows the type of request
     *
     * This function provides exactly the same result as MegaChatRequest::getRequestString.
     * It's provided for a better Python compatibility
     *
     * @return Readable string showing the type of request
     */
    virtual const char* __str__() const;

    /**
     * @brief Returns a readable string that shows the type of request
     *
     * This function provides exactly the same result as MegaChatRequest::getRequestString.
     * It's provided for a better PHP compatibility
     *
     * @return Readable string showing the type of request
     */
    virtual const char* __toString() const;

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
    virtual void onRequestFinish(MegaChatApi* api, MegaChatRequest *request, MegaError* e);

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
    virtual void onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaError* error);
    virtual ~MegaChatRequestListener();
};

class MegaChatApi
{

public:
    enum
    {
        PRESENCE_OFFLINE = 0,
        PRESENCE_BUSY,
        PRESENCE_AWAY,
        PRESENCE_ONLINE
    };


    // chat will reuse an existent megaApi instance (ie. the one for cloud storage)
    MegaChatApi(MegaApi *megaApi);

    // chat will use its own megaApi, a new instance
    MegaChatApi(const char *appKey, const char* appDir);

    virtual ~MegaChatApi();

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
     * - PRESENCE_OFFLINE = 1
     * The user appears as being offline
     *
     * - PRESENCE_BUSY = 2
     * The user is busy and don't want to be disturbed.
     *
     * - PRESENCE_AWAY = 3
     * The user is away and might not answer.
     *
     * - PRESENCE_ONLINE = 4
     * The user is connected and online.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    void setChatStatus(int status, MegaChatRequestListener *listener = NULL);

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
    void addChatListener(MegaChatListener *listener);
    void removeChatListener(MegaChatListener *listener);
    void addChatLocalVideoListener(MegaChatVideoListener *listener);
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);

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

}

#endif // MEGACHATAPI_H
