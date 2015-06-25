#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include "contactList.h"
#include "ITypes.h" //for IPtr
#include "karereEventObjects.h"
#include "rtcModule/IRtcModule.h"
#include <memory>
#include <map>
#include <type_traits>
#include <retryHandler.h>
#include <busConstants.h>
#include <serverListProviderForwards.h>

namespace strophe { class Connection; }
namespace rtcModule
{
    class IRtcModule;
    class IEventHandler;
}
namespace mega { namespace rh { class IRetryController; } }
class MyMegaApi;

namespace karere
{
class TextModule;
class ChatRoom;
class Client
{
public:
    /** client's strophe connection */
    std::shared_ptr<strophe::Connection> conn;
    std::string getUsername() const
    {
        return strophe::getNodeFromJid(conn->fullOrBareJid());
    }

    /**
    * Get resource of current connection.
    */
    std::string getResource() const
    {
        return strophe::getResourceFromJid(conn->fullJid());
    }
    /**
    * @Get a unique nickname based on current connection.
    * @returns {string} nickname based on current connection.
    */
    std::string getNickname() const { return getUsername() + "__" + getResource(); }

    /**
     * @brief Initialize the contact list.
     *
     * This performs a request to xmpp roster server and fetch the contact list.
     * Contact list also registers a contact presence handler to update the list itself based on received presence messages.
     */
    std::shared_ptr<MyMegaApi> api;
    //we use IPtr smart pointers instead of std::unique_ptr because we want to delete not via the
    //destructor, but via a destroy() method. This is to support cross-DLL loading of plugins,
    //where operator delete would try to deallocate memory via the memory manager/runtime of the caller,
    //which is often not the one that allocated that memory (usually the DLL allocates the object).
    //Calling a function defined in the DLL that in turn calls the destructor ensures that operator
    //delete is called from code inside the DLL, i.e. in the runtime where the class is implemented,
    //operates and was allocated
    rtcModule::IPtr<rtcModule::IRtcModule> rtc;
    rtcModule::IPtr<TextModule> mTextModule;
    Client(const std::string& email, const std::string& password);
    virtual ~Client();
    void registerRtcHandler(rtcModule::IEventHandler* rtcHandler);
    promise::Promise<int> init();
    /** @brief Notifies the client that internet connection is again available */
    void notifyNetworkOffline();
    /** @brief Notifies the client that network connection is down */
    void notifyNetworkOnline();
    void startKeepalivePings();
    /**
     * @brief Ping a target peer to check whether he/she is alive
     * @param [peerJid] {const char*} peer's Jid. If NULL, then no 'to'
     * attribute will be included in the stanza, effectively sending the ping to the server
     * @param [intervalSec] {int} optional with default value as 100, interval in seconds to do ping.
     *
     * This performs a xmpp ping request to xmpp server and check whether the target user is alive or not.
     */
    strophe::StanzaPromise pingPeer(const char* peerJid);
    /**
    * @brief set user's chat presence.
    * set user's presence state, which can be one of online, busy, away, online
    */
    void setPresence(const Presence pres, const int delay = 0);

    /**
    * @brief get
    * @param roomJid {string} room's JID.
    * @param chatState {ChatState} user's chat state.
    * @returns {void}
    */
    const Contact& getContact(const std::string& userJid)
    {
        return contactList.getContact(userJid);
    }
    ContactList& getContactList()
    {
        return contactList;
    }
protected:
    /** our own email address */
    std::string mEmail;
    /** our password */
    std::string mPassword;
    /** client's contact list */
    ContactList contactList;
    typedef FallbackSingleServerProvider<>  XmppServerProvider;
    std::unique_ptr<XmppServerProvider> mXmppServerProvider;
    std::unique_ptr<mega::rh::IRetryController> mReconnectController;
    xmpp_ts mLastPingTs = 0;
    /* handler for webrtc events */
    rtcModule::IPtr<rtcModule::IEventHandler> mRtcHandler;
    void setupReconnectHandler();
    promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
        getOtherUserInfo(std::string &emailAddress);
    promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
        getThisUserInfo();
    void setupHandlers();
    promise::Promise<int> initializeContactList();
    /**
     * @brief send response to ping request.
     *
     * This performs an xmpp response to the received xmpp ping request.
     */
    void sendPong(const std::string& peerJid, const std::string& messageId);

};
}
#endif // CHATCLIENT_H
