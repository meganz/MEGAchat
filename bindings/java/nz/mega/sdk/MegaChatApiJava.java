package nz.mega.sdk;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Set;

public class MegaChatApiJava {
    MegaChatApi megaChatApi;
    static DelegateMegaChatLogger logger;

    // Error information but application will continue run.
    public final static int LOG_LEVEL_ERROR = MegaChatApi.LOG_LEVEL_ERROR;
    // Information representing errors in application but application will keep running
    public final static int LOG_LEVEL_WARNING = MegaChatApi.LOG_LEVEL_WARNING;
    // Mainly useful to represent current progress of application.
    public final static int LOG_LEVEL_INFO = MegaChatApi.LOG_LEVEL_INFO;
    public final static int LOG_LEVEL_VERBOSE = MegaChatApi.LOG_LEVEL_VERBOSE;
    // Informational logs, that are useful for developers. Only applicable if DEBUG is defined.
    public final static int LOG_LEVEL_DEBUG = MegaChatApi.LOG_LEVEL_DEBUG;
    public final static int LOG_LEVEL_MAX = MegaChatApi.LOG_LEVEL_MAX;

    static Set<DelegateMegaChatRequestListener> activeRequestListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatRequestListener>());
    static Set<DelegateMegaChatListener> activeChatListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatListener>());
    static Set<DelegateMegaChatRoomListener> activeChatRoomListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatRoomListener>());
    static Set<DelegateMegaChatCallListener> activeChatCallListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatCallListener>());
    static Set<DelegateMegaChatVideoListener> activeChatVideoListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatVideoListener>());
    static Set<DelegateMegaChatNotificationListener> activeChatNotificationListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatNotificationListener>());

    void runCallback(Runnable runnable) {
        runnable.run();
    }

    /**
     * Creates an instance of MegaChatApi to access to the chat-engine.
     *
     * @param megaApi Instance of MegaApi to be used by the chat-engine.
     * session will be discarded and MegaChatApi expects to have a login+fetchnodes before MegaChatApi::init
     */
    public MegaChatApiJava(MegaApiJava megaApi){
        megaChatApi = new MegaChatApi(megaApi.getMegaApi());
    }

    public void addChatRequestListener(MegaChatRequestListenerInterface listener)
    {
        megaChatApi.addChatRequestListener(createDelegateRequestListener(listener, false));
    }

    public void addChatListener(MegaChatListenerInterface listener)
    {
        megaChatApi.addChatListener(createDelegateChatListener(listener));
    }

    public void addChatCallListener(MegaChatCallListenerInterface listener)
    {
        megaChatApi.addChatCallListener(createDelegateChatCallListener(listener));
    }

    public void addChatLocalVideoListener(MegaChatVideoListenerInterface listener)
    {
        megaChatApi.addChatLocalVideoListener(createDelegateChatVideoListener(listener, false));
    }

    public void addChatRemoteVideoListener(MegaChatVideoListenerInterface listener)
    {
        megaChatApi.addChatRemoteVideoListener(createDelegateChatVideoListener(listener, true));
    }

    /**
     * Register a listener to receive notifications
     *
     * You can use MegaChatApi::removeChatRequestListener to stop receiving events.
     *
     * @param listener Listener that will receive all events about requests
     */
    public void addChatNotificationListener(MegaChatNotificationListenerInterface listener){
        megaChatApi.addChatNotificationListener(createDelegateChatNotificationListener(listener));
    }

    /**
     * Unregister a MegaChatNotificationListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    public void removeChatNotificationListener(MegaChatNotificationListenerInterface listener){
        ArrayList<DelegateMegaChatNotificationListener> listenersToRemove = new ArrayList<DelegateMegaChatNotificationListener>();
        synchronized (activeChatNotificationListeners) {
            Iterator<DelegateMegaChatNotificationListener> it = activeChatNotificationListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatNotificationListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i=0;i<listenersToRemove.size();i++){
            megaChatApi.removeChatNotificationListener(listenersToRemove.get(i));
        }
    }

    public void removeChatRequestListener(MegaChatRequestListenerInterface listener) {

        ArrayList<DelegateMegaChatRequestListener> listenersToRemove = new ArrayList<DelegateMegaChatRequestListener>();
        synchronized (activeRequestListeners) {
            Iterator<DelegateMegaChatRequestListener> it = activeRequestListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatRequestListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i=0;i<listenersToRemove.size();i++){
            megaChatApi.removeChatRequestListener(listenersToRemove.get(i));
        }
    }

    public void removeChatListener(MegaChatListenerInterface listener) {
        ArrayList<DelegateMegaChatListener> listenersToRemove = new ArrayList<DelegateMegaChatListener>();
        synchronized (activeChatListeners) {
            Iterator<DelegateMegaChatListener> it = activeChatListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i=0;i<listenersToRemove.size();i++){
            megaChatApi.removeChatListener(listenersToRemove.get(i));
        }
    }

    public void removeChatCallListener(MegaChatCallListenerInterface listener) {
        ArrayList<DelegateMegaChatCallListener> listenersToRemove = new ArrayList<DelegateMegaChatCallListener>();
        synchronized (activeChatCallListeners) {
            Iterator<DelegateMegaChatCallListener> it = activeChatCallListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatCallListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i=0;i<listenersToRemove.size();i++){
            megaChatApi.removeChatCallListener(listenersToRemove.get(i));
        }
    }

    public void removeChatVideoListener(MegaChatVideoListenerInterface listener) {
        ArrayList<DelegateMegaChatVideoListener> listenersToRemove = new ArrayList<DelegateMegaChatVideoListener>();
        synchronized (activeChatVideoListeners) {
            Iterator<DelegateMegaChatVideoListener> it = activeChatVideoListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatVideoListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i = 0; i < listenersToRemove.size(); i++) {
            DelegateMegaChatVideoListener delegateListener = listenersToRemove.get(i);
            delegateListener.setRemoved();
            if (delegateListener.isRemote()) {
                megaChatApi.removeChatRemoteVideoListener(delegateListener);
            }
            else {
                megaChatApi.removeChatLocalVideoListener(delegateListener);
            }
        }
    }


    public int init(String sid)
    {
        return megaChatApi.init(sid);
    }

    /**
     * Returns the current initialization state
     *
     * The possible values are:
     *  - MegaChatApi::INIT_ERROR = -1
     *  - MegaChatApi::INIT_WAITING_NEW_SESSION = 1
     *  - MegaChatApi::INIT_OFFLINE_SESSION = 2
     *  - MegaChatApi::INIT_ONLINE_SESSION = 3
     *  - MegaChatApi::INIT_NO_CACHE = 7
     *
     * The returned value will be undefined if \c init(sid) has not been called yet.
     *
     * @return The current initialization state
     */
    public int getInitState(){
        return megaChatApi.getInitState();
    }

    /**
     * Establish the connection with chat-related servers (chatd, presenced and Gelb).
     *
     * This function must be called only after calling:
     *  - MegaChatApi::init to initialize the chat engine
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *
     * At that point, the initialization state should be MegaChatApi::INIT_ONLINE_SESSION.
     *
     * The online status after connecting will be whatever was last used.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void connect(MegaChatRequestListenerInterface listener)
    {
        megaChatApi.connect(createDelegateRequestListener(listener));
    }

    /**
     * Establish the connection with chat-related servers (chatd, presenced and Gelb).
     *
     * This function must be called only after calling:
     *  - MegaChatApi::init to initialize the chat engine
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *
     * At that point, the initialization state should be MegaChatApi::INIT_ONLINE_SESSION.
     *
     * The online status after connecting will be whatever was last used.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     *
     */
    public void connect(){
        megaChatApi.connect();
    }

    /**
     * Establish the connection with chat-related servers (chatd, presenced and Gelb).
     *
     * This function is intended to be used instead of MegaChatApi::connect when the connection
     * is done by a service in background, which is launched without user-interaction. It avoids
     * to notify to the server that this client is active, but actually the user is away.
     *
     * This function must be called only after calling:
     *  - MegaChatApi::init to initialize the chat engine
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *
     * At that point, the initialization state should be MegaChatApi::INIT_ONLINE_SESSION.
     * The online status after connecting will be whatever was last used.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns true.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void connectInBackground(MegaChatRequestListenerInterface listener)
    {
        megaChatApi.connectInBackground(createDelegateRequestListener(listener));
    }

    /**
     * Establish the connection with chat-related servers (chatd, presenced and Gelb).
     *
     * This function is intended to be used instead of MegaChatApi::connect when the connection
     * is done by a service in background, which is launched without user-interaction. It avoids
     * to notify to the server that this client is active, but actually the user is away.
     *
     * This function must be called only after calling:
     *  - MegaChatApi::init to initialize the chat engine
     *  - MegaApi::login to login in MEGA
     *  - MegaApi::fetchNodes to retrieve current state of the account
     *
     * At that point, the initialization state should be MegaChatApi::INIT_ONLINE_SESSION.
     * The online status after connecting will be whatever was last used.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CONNECT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns true.
     */
    public void connectInBackground()
    {
        megaChatApi.connectInBackground();
    }

    /**
     * Disconnect from chat-related servers (chatd, presenced and Gelb).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISCONNECT
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void disconnect(MegaChatRequestListenerInterface listener){
        megaChatApi.disconnect(createDelegateRequestListener(listener));
    }

    /**
     * Returns the current state of the connection
     *
     * It can be one of the following values:
     *  - MegaChatApi::DISCONNECTED = 0
     *  - MegaChatApi::CONNECTING   = 1
     *  - MegaChatApi::CONNECTED    = 2
     *
     * @return The state of connection
     */
    public int getConnectionState(){
        return megaChatApi.getConnectionState();
    }

    /**
     * Returns the current state of the connection to chatd
     *
     * The possible values are:
     *  - MegaChatApi::CHAT_CONNECTION_OFFLINE      = 0
     *  - MegaChatApi::CHAT_CONNECTION_IN_PROGRESS  = 1
     *  - MegaChatApi::CHAT_CONNECTION_LOGGING      = 2
     *  - MegaChatApi::CHAT_CONNECTION_ONLINE       = 3
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return The state of connection
     */
    public int getChatConnectionState(long chatid){
        return  megaChatApi.getChatConnectionState(chatid);
    }

    /**
     * Refresh DNS servers and retry pending connections
     *
     * The associated request type with this request is MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void retryPendingConnections(MegaChatRequestListenerInterface listener){
        megaChatApi.retryPendingConnections(createDelegateRequestListener(listener));
    }

    /**
     * Logout of chat servers invalidating the session
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOGOUT
     *
     * After calling \c logout, the subsequent call to MegaChatApi::init expects to
     * have a new session created by MegaApi::login.
     *
     */
    public void logout(){
        megaChatApi.logout();
    }

    /**
     * Logout of chat servers invalidating the session
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOGOUT
     *
     * After calling \c logout, the subsequent call to MegaChatApi::init expects to
     * have a new session created by MegaApi::login.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void logout(MegaChatRequestListenerInterface listener){
        megaChatApi.logout(createDelegateRequestListener(listener));
    }

    /**
     * @brief Logout of chat servers without invalidating the session
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOGOUT
     *
     * After calling \c localLogout, the subsequent call to MegaChatApi::init expects to
     * have an already existing session created by MegaApi::fastLogin(session)
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void localLogout(MegaChatRequestListenerInterface listener){
        megaChatApi.localLogout(createDelegateRequestListener(listener));
    }

    /**
     * Creates a chat for one or more participants, allowing you to specify their
     * permissions and if the chat should be a group chat or not (when it is just for 2 participants).
     *
     * There are two types of chat: permanent an group. A permanent chat is between two people, and
     * participants can not leave it.
     *
     * The creator of the chat will have moderator level privilege and should not be included in the
     * list of peers.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns if the new chat is a group chat or permanent chat
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * @note If you are trying to create a chat with more than 1 other person, then it will be forced
     * to be a group chat.
     *
     * @note If peers list contains only one person, group chat is not set and a permament chat already
     * exists with that person, then this call will return the information for the existing chat, rather
     * than a new chat.
     *
     * @param group Flag to indicate if the chat is a group chat or not
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param listener MegaChatRequestListener to track this request
     */
    public void createChat(boolean group, MegaChatPeerList peers, MegaChatRequestListenerInterface listener){
        megaChatApi.createChat(group, peers, createDelegateRequestListener(listener));
    }

    public void inviteToChat(long chatid, long userhandle, int privs)
    {
        megaChatApi.inviteToChat(chatid, userhandle, privs);
    }

    /**
     * Adds a user to an existing chat. To do this you must have the
     * moderator privilege in the chat, and the chat must be a group chat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_INVITE_TO_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the MegaChatHandle of the user to be invited
     * - MegaChatRequest::getPrivilege - Returns the privilege level wanted for the user
     *
     * On the onTransferFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot invite peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param userhandle MegaChatHandle that identifies the user
     * @param privs Privilege level for the new peers. Valid values are:
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     * @param listener MegaChatRequestListener to track this request
     */
    public void inviteToChat(long chatid, long userhandle, int privs, MegaChatRequestListenerInterface listener)
    {
        megaChatApi.inviteToChat(chatid, userhandle, privs, createDelegateRequestListener(listener));
    }

    /**
     * Remove another user from a chat. To remove a user you need to have the
     * operator/moderator privilege. Only groupchats can be left.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the MegaChatHandle of the user to be removed
     *
     * On the onTransferFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to remove peers.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot remove peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param uh MegaChatHandle that identifies the user.
     * @param listener MegaChatRequestListener to track this request
     */
    public void removeFromChat(long chatid, long uh, MegaChatRequestListenerInterface listener){
        megaChatApi.removeFromChat(chatid, uh,createDelegateRequestListener(listener));
    }

    /**
     * Leave a chatroom. Only groupchats can be left.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * On the onTransferFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to remove peers.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot remove peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void leaveChat(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.leaveChat(chatid,createDelegateRequestListener(listener));
    }

    /**
     * Allows a logged in operator/moderator to adjust the permissions on any other user
     * in their group chat. This does not work for a 1:1 chat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the MegaChatHandle of the user whose permission
     * is to be upgraded
     * - MegaChatRequest::getPrivilege - Returns the privilege level wanted for the user
     *
     * On the onTransferFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to update the privilege level.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chatid or user handle are invalid
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param uh MegaChatHandle that identifies the user
     * @param privilege Privilege level for the existing peer. Valid values are:
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     * @param listener MegaChatRequestListener to track this request
     */
    public void updateChatPermissions(long chatid, long uh, int privilege, MegaChatRequestListenerInterface listener){
        megaChatApi.updateChatPermissions(chatid, uh, privilege, createDelegateRequestListener(listener));
    }

    /**
     * Set your online status.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHAT_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaRequest::getNumber - Returns the new status of the user in chat.
     *
     * @param status Online status in the chat.
     *
     * It can be one of the following values:
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_BUSY = 2
     * The user is busy and don't want to be disturbed.
     *
     * - MegaChatApi::STATUS_AWAY = 3
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 4
     * The user is connected and online.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void setOnlineStatus(int status, MegaChatRequestListenerInterface listener){
        megaChatApi.setOnlineStatus(status, createDelegateRequestListener(listener));
    }

    public void setOnlineStatus(int status)
    {
        megaChatApi.setOnlineStatus(status);
    }

    /**
     * Enable/disable the autoaway option, with one specific timeout
     *
     * When autoaway is enabled and persist is false, the app should call to
     * \c signalPresenceActivity regularly in order to keep the current online status.
     * Otherwise, after \c timeout seconds, the online status will be changed to away.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true if autoaway is enabled.
     * - MegaChatRequest::getNumber - Returns the specified timeout.
     *
     * @param enable True to enable the autoaway feature
     * @param timeout Seconds to wait before turning away (if no activity has been signalled)
     * @param listener MegaChatRequestListenerInterface to track this request
     */
    public void setPresenceAutoaway(boolean enable, int timeout, MegaChatRequestListenerInterface listener){
        megaChatApi.setPresenceAutoaway(enable, timeout, createDelegateRequestListener(listener));
    }

    /**
     * Enable/disable the autoaway option, with one specific timeout
     *
     * When autoaway is enabled and persist is false, the app should call to
     * \c signalPresenceActivity regularly in order to keep the current online status.
     * Otherwise, after \c timeout seconds, the online status will be changed to away.
     *
     * @param enable True to enable the autoaway feature
     * @param timeout Seconds to wait before turning away (if no activity has been signalled)
     */
    public void setPresenceAutoaway(boolean enable, int timeout){
        megaChatApi.setPresenceAutoaway(enable, timeout);
    }

    /**
     * Enable/disable the persist option
     *
     * When this option is enable, the online status shown to other users will be the
     * one specified by the user, even when you are disconnected.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRESENCE_PERSIST
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true if presence status is persistent.
     *
     * @param enable True to enable the persist feature
     * @param listener MegaChatRequestListenerInterface to track this request
     */
    public void setPresencePersist(boolean enable, MegaChatRequestListenerInterface listener){
        megaChatApi.setPresencePersist(enable, createDelegateRequestListener(listener));
    }

    /**
     * Enable/disable the persist option
     *
     * When this option is enable, the online status shown to other users will be the
     * one specified by the user, even when you are disconnected.
     *
     * @param enable True to enable the persist feature
     */
    public void setPresencePersist(boolean enable){
        megaChatApi.setPresencePersist(enable);
    }

    /**
     * Signal there is some user activity
     *
     * When the presence configuration is set to autoaway (and persist is false), this
     * function should be called regularly to not turn into away status automatically.
     *
     * A good approach is to call this function with every mouse move or keypress on desktop
     * platforms; or at any finger tap or gesture and any keypress on mobile platforms.
     *
     * Failing to call this function, you risk a user going "Away" while typing a lengthy message,
     * which would be awkward.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SIGNAL_ACTIVITY.
     *
     * @param listener MegaChatRequestListenerInterface to track this request
     */
    public void signalPresenceActivity(MegaChatRequestListenerInterface listener){
        megaChatApi.signalPresenceActivity(createDelegateRequestListener(listener));
    }

    /**
     * Signal there is some user activity
     *
     * When the presence configuration is set to autoaway (and persist is false), this
     * function should be called regularly to not turn into away status automatically.
     *
     * A good approach is to call this function with every mouse move or keypress on desktop
     * platforms; or at any finger tap or gesture and any keypress on mobile platforms.
     *
     * Failing to call this function, you risk a user going "Away" while typing a lengthy message,
     * which would be awkward.
     */
    public void signalPresenceActivity(){
        megaChatApi.signalPresenceActivity();
    }

    /**
     * Get your online status.
     *
     * It can be one of the following values:
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_BUSY = 2
     * The user is busy and don't want to be disturbed.
     *
     * - MegaChatApi::STATUS_AWAY = 3
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 4
     * The user is connected and online.
     */
    public int getOnlineStatus(){
        return megaChatApi.getOnlineStatus();
    }

    /**
     * Get the current presence configuration
     *
     * @see \c MegaChatPresenceConfig for further details.
     *
     * @return The current presence configuration
     */
    public MegaChatPresenceConfig getPresenceConfig(){
        return megaChatApi.getPresenceConfig();
    }

    /**
     * Returns whether the autoaway option is enabled.
     *
     * @note This function returns true even when the Presence Config
     * is pending to be confirmed by the server.
     *
     * @return True if autoaway is enabled.
     */
    public boolean isSignalActivityRequired(){
        return megaChatApi.isSignalActivityRequired();
    }

    /**
     * Get the online status of a user.
     *
     * It can be one of the following values:
     *
     * - MegaChatApi::STATUS_OFFLINE = 1
     * The user appears as being offline
     *
     * - MegaChatApi::STATUS_AWAY = 2
     * The user is away and might not answer.
     *
     * - MegaChatApi::STATUS_ONLINE = 3
     * The user is connected and online.
     *
     * - MegaChatApi::STATUS_BUSY = 4
     * The user is busy and don't want to be disturbed.
     *
     * @param userhandle Handle of the peer whose name is requested.
     * @return Online status of the user
     */
    public int getUserOnlineStatus(long userhandle){
        return megaChatApi.getUserOnlineStatus(userhandle);
    }

    /**
     * Set the status of the app
     *
     * Apps in mobile devices can be in different status. Typically, foreground and
     * background. The app should define its status in order to receive notifications
     * from server when the app is in background.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_BACKGROUND_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getfLAG - Returns the background status
     *
     * @param background True if the the app is in background, false if in foreground.
     */
    public void setBackgroundStatus(boolean background, MegaChatRequestListenerInterface listener){
        if (background){
            megaChatApi.saveCurrentState();
        }
        megaChatApi.setBackgroundStatus(background, createDelegateRequestListener(listener));
    }

    /**
     * Set the status of the app
     *
     * Apps in mobile devices can be in different status. Typically, foreground and
     * background. The app should define its status in order to receive notifications
     * from server when the app is in background.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_BACKGROUND_STATUS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getfLAG - Returns the background status
     *
     * @param background True if the the app is in background, false if in foreground.
     */
    public void setBackgroundStatus(boolean background){
        if (background){
            megaChatApi.saveCurrentState();
        }
        megaChatApi.setBackgroundStatus(background);
    }

    /**
     * Returns the current firstname of the user
     *
     * This function is useful to get the firstname of users who participated in a groupchat with
     * you but already left. If the user sent a message, you may want to show the name of the sender.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_FIRSTNAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the firstname of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param listener MegaChatRequestListener to track this request
     */
    public void getUserFirstname(long userhandle, MegaChatRequestListenerInterface listener){
        megaChatApi.getUserFirstname(userhandle, createDelegateRequestListener(listener));
    }

    /**
     * Returns the current lastname of the user
     *
     * This function is useful to get the lastname of users who participated in a groupchat with
     * you but already left. If the user sent a message, you may want to show the name of the sender.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_LASTNAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the lastname of the user
     *
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param listener MegaChatRequestListener to track this request
     */
    public void getUserLastname(long userhandle, MegaChatRequestListenerInterface listener){
        megaChatApi.getUserLastname(userhandle, createDelegateRequestListener(listener));
    }

    /**
     * Returns the current email address of the user
     *
     * This function is useful to get the email address of users you are contact with.
     * Note that for any other user without contact relationship, this function will return NULL.
     *
     * You take the ownership of the returned value
     *
     * @param userhandle Handle of the user whose name is requested.
     * @return The email address of the contact, or NULL if not found.
     */
    public String getContactEmail(long userhandle){
        return megaChatApi.getContactEmail(userhandle);
    }

    /**
     * @brief Returns the handle of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * @return Own user handle
     */
    public long getMyUserHandle(){
        return megaChatApi.getMyUserHandle();
    }

    /**
     * @brief Returns the firstname of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user firstname
     */
    public String getMyFirstname(){
        return megaChatApi.getMyFirstname();
    }

    /**
     * @brief Returns the lastname of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user lastname
     */
    public String getMyLastname(){
        return megaChatApi.getMyLastname();
    }

    /**
     * @brief Returns the fullname of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user fullname
     */
    public String getMyFullname(){
        return megaChatApi.getMyFullname();
    }

    /**
     * @brief Returns the email of the logged in user.
     *
     * This function works even in offline mode (MegaChatApi::INIT_OFFLINE_SESSION),
     * since the value is retrieved from cache.
     *
     * You take the ownership of the returned value
     *
     * @return Own user email
     */
    public String getMyEmail(){
        return megaChatApi.getMyEmail();
    }

    public ArrayList<MegaChatRoom> getChatRooms()
    {
        return chatRoomListToArray(megaChatApi.getChatRooms());
    }

    /**
     * Get the MegaChatRoom for the 1on1 chat with the specified user
     *
     * If the 1on1 chat with the user specified doesn't exist, this function will
     * return NULL.
     *
     * It is needed to have successfully completed the \c MegaChatApi::init request
     * before calling this function.
     *
     * You take the ownership of the returned value
     *
     * @param userhandle MegaChatHandle that identifies the user
     * @return MegaChatRoom object for the specified \c userhandle
     */
    public MegaChatRoom getChatRoomByUser(long userhandle){
        return megaChatApi.getChatRoomByUser(userhandle);
    }

    /**
     * Get all chatrooms (1on1 and groupal) with limited information
     *
     * It is needed to have successfully completed the \c MegaChatApi::init request
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatListItemList objects with all chatrooms of this account.
     */
    public ArrayList<MegaChatListItem> getChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getChatListItems());

    }

    /**
     * Get the MegaChatListItem that has a specific handle
     *
     * You can get the handle of the chatroom using MegaChatRoom::getChatId or
     * MegaChatListItem::getChatId.
     *
     * It is needed to have successfully completed the \c MegaChatApi::init request
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * You take the ownership of the returned value
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return MegaChatListItem object for the specified \c chatid
     */
    public MegaChatListItem getChatListItem(long chatid){
        return megaChatApi.getChatListItem(chatid);
    }

    /**
     * Return the number of chatrooms with unread messages
     * @return The number of chatrooms with unread messages
     */
    public int getUnreadChats(){
        return megaChatApi.getUnreadChats();
    }

    /**
     * Return the chatrooms that are currently active
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the active chatrooms
     */
    public ArrayList<MegaChatListItem> getActiveChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getActiveChatListItems());
    }

    /**
     * Return the chatrooms that are currently inactive
     *
     * Chatrooms became inactive when you left a groupchat or, for 1on1 chats,
     * when the contact-relationship is broken (you remove the contact or you are
     * removed by the other contact).
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the active chatrooms
     */
    public ArrayList<MegaChatListItem> getInactiveChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getInactiveChatListItems());
    }

    /**
     * Return the chatrooms that have unread messages
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the chatrooms with unread messages
     */
    public ArrayList<MegaChatListItem> getUnreadChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getUnreadChatListItems());
    }

    /**
     * Get the chat id for the 1on1 chat with the specified user
     *
     * If the 1on1 chat with the user specified doesn't exist, this function will
     * return MEGACHAT_INVALID_HANDLE.
     *
     * @param userhandle MegaChatHandle that identifies the user
     * @return MegaChatHandle that identifies the 1on1 chatroom
     */
    long getChatHandleByUser(long userhandle){
        return megaChatApi.getChatHandleByUser(userhandle);
    }

    /**
     * Get the MegaChatRoom that has a specific handle
     *
     * You can get the handle of a MegaChatRoom using MegaChatRoom::getChatId or
     * MegaChatListItem::getChatId.
     *
     * It is needed to have successfully completed the \c MegaChatApi::init request
     * before calling this function.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatRoom objects with all chatrooms of this account.
     */

    public MegaChatRoom getChatRoom(long chatid){
        return megaChatApi.getChatRoom(chatid);
    }

/*
    /**
     * @brief Returns the handle of the user.
     *
     * @return For outgoing messages, it returns the handle of the target user.
     * For incoming messages, it returns the handle of the sender.
     *
    public long getUserHandle()
    {

    }
*/

    public void removeFromChat(long chatid, long userhandle)
    {
        megaChatApi.removeFromChat(chatid, userhandle);
    }

    public void truncateChat(long chatid, long messageid)
    {
        megaChatApi.truncateChat(chatid, messageid);
    }

    public void truncateChat(long chatid, long messageid, MegaChatRequestListenerInterface listener)
    {
        megaChatApi.truncateChat(chatid, messageid, createDelegateRequestListener(listener));
    }

    /**
     * Allows a logged in operator/moderator to clear the entire history of a chat
     *
     * The latest message gets overridden with a management message.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_TRUNCATE_HISTORY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * On the onTransferFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to truncate the chat history
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chatid or user handle are invalid
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void clearChatHistory(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.clearChatHistory(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Allows to set the title of a group chat
     *
     * Only participants with privilege level MegaChatPeerList::PRIV_MODERATOR are allowed to
     * set the title of a chat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_EDIT_CHATROOM_NAME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getText - Returns the title of the chat.
     *
     * On the onTransferFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers.
     * - MegaChatError::ERROR_ARGS - If there's a title and it's not Base64url encoded.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the title of the chat that was actually saved.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param title Null-terminated character string with the title that wants to be set. If the
     * title is longer than 30 characters, it will be truncated to that maximum length.
     * @param listener MegaChatRequestListener to track this request
     */
    public void setChatTitle(long chatid, String title, MegaChatRequestListenerInterface listener){
        megaChatApi.setChatTitle(chatid, title, createDelegateRequestListener(listener));
    }

    /**
     * This method should be called when a chat is opened
     *
     * The second parameter is the listener that will receive notifications about
     * events related to the specified chatroom.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRoomListener to track events on this chatroom
     *
     * @return True if success, false if the chatroom was not found.
     */
//    public boolean openChatRoom(long chatid, MegaChatRoomListenerInterface listener){
    public boolean openChatRoom(long chatid, MegaChatRoomListenerInterface listener){

        return megaChatApi.openChatRoom(chatid, createDelegateChatRoomListener(listener));
    }

    /**
     * This method should be called when a chat is closed.
     *
     * It automatically unregisters the listener to stop receiving the related events.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRoomListener to be unregistered.
     */
    public void closeChatRoom(long chatid, MegaChatRoomListenerInterface listener){

        DelegateMegaChatRoomListener listenerToDelete=null;

        Iterator<DelegateMegaChatRoomListener> itr = activeChatRoomListeners.iterator();
        while(itr.hasNext()) {
            DelegateMegaChatRoomListener item = itr.next();
            if(item.getUserListener() == listener){
                listenerToDelete = item;
                itr.remove();
                break;
            }
        }

        megaChatApi.closeChatRoom(chatid, listenerToDelete);
    }

    /**
     * Initiates fetching more history of the specified chatroom.
     *
     * The loaded messages will be notified one by one through the MegaChatRoomListener
     * specified at MegaChatApi::openChatRoom (and through any other listener you may have
     * registered by calling MegaChatApi::addChatRoomListener).
     *
     * The corresponding callback is MegaChatRoomListener::onMessageLoaded.
     *
     * @note The actual number of messages loaded can be less than \c count. One reason is
     * the history being shorter than requested, the other is due to internal protocol
     * messages that are not intended to be displayed to the user. Additionally, if the fetch
     * is local and there's no more history locally available, the number of messages could be
     * lower too (and the next call to MegaChatApi::loadMessages will fetch messages from server).
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param count The number of requested messages to load.
     *
     * @return Return the source of the messages that is going to be fetched. The possible values are:
     *   - MegaChatApi::SOURCE_NONE = 0: there's no more history available (not even int the server)
     *   - MegaChatApi::SOURCE_LOCAL: messages will be fetched locally (RAM or DB)
     *   - MegaChatApi::SOURCE_REMOTE: messages will be requested to the server. Expect some delay
     *
     * The value MegaChatApi::SOURCE_REMOTE can be used to show a progress bar accordingly when network operation occurs.
     */
    public int loadMessages(long chatid, int count){
        return megaChatApi.loadMessages(chatid, count);
    }

    /**
     * Returns the MegaChatMessage specified from the chat room.
     *
     * Only the messages that are already loaded and notified
     * by MegaChatRoomListener::onMessageLoaded can be requested. For any
     * other message, this function will return NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @return The MegaChatMessage object, or NULL if not found.
     */
    public MegaChatMessage getMessage(long chatid, long msgid){
        return megaChatApi.getMessage(chatid, msgid);
    }

    /**
     * Returns the MegaChatMessage specified from manual sending queue.
     *
     * The identifier of messages in manual sending status is notified when the
     * message is moved into that queue or while loading history. In both cases,
     * the callback MegaChatRoomListener::onMessageLoaded will be received with a
     * message object including the row id.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param rowid Manual sending queue id of the message
     * @return The MegaChatMessage object, or NULL if not found.
     */
    public MegaChatMessage getManualSendingMessage(long chatid, long rowid){
        return megaChatApi.getManualSendingMessage(chatid, rowid);
    }

    /**
     * Sends a new message to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId()
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to INVALID_HANDLE.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msg Content of the message
     * application-specific type like link, share, picture etc.) @see MegaChatMessage::Type.
     *
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    public MegaChatMessage sendMessage(long chatid, String msg){
        return megaChatApi.sendMessage(chatid, msg);
    }

    /**
     * Sends a contact or a group of contacts to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId()
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param handles MegaChatHandleList with contacts to be attached
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    public MegaChatMessage attachContacts(long chatid, MegaHandleList handles){
        return megaChatApi.attachContacts(chatid, handles);
    }

    /**
     * Sends a node or a group of nodes to the specified chatroom
     *
     * In contrast to other functions to send messages, such as
     * MegaChatApi::sendMessage or MegaChatApi::attachContacts, this function
     * is asynchronous and does not return a MegaChatMessage directly. Instead, the
     * MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getNodeList - Returns the list of nodes
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodes Array of nodes that the user want to attach
     * @param listener MegaChatRequestListener to track this request
     */
    public void attachNodes(long chatid, MegaNodeList nodes, MegaChatRequestListenerInterface listener){
        megaChatApi.attachNodes(chatid, nodes, createDelegateRequestListener(listener));
    }

    /**
     * Revoke the access to a node in the specified chatroom
     *
     * In contrast to other functions to send messages, such as
     * MegaChatApi::sendMessage or MegaChatApi::attachContacts, this function
     * is asynchronous and does not return a MegaChatMessage directly. Instead, the
     * MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::geUserHandle - Returns the handle of the node
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodeHandle MegaChatHandle that identifies the node to revoke access to
     * @param listener MegaChatRequestListener to track this request
     */
    public void revokeAttachment(long chatid, long nodeHandle, MegaChatRequestListenerInterface listener){
        megaChatApi.revokeAttachment(chatid, nodeHandle, createDelegateRequestListener(listener));
    }

    /**
     * Sends a node to the specified chatroom
     *
     * The attachment message includes information about the node, so the receiver can download
     * or import the node.
     *
     * In contrast to other functions to send messages, such as
     * MegaChatApi::sendMessage or MegaChatApi::attachContacts, this function
     * is asynchronous and does not return a MegaChatMessage directly. Instead, the
     * MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the handle of the node
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getMegaChatMessage - Returns the message that has been sent
     *
     * When the server confirms the reception of the message, the MegaChatRoomListener::onMessageUpdate
     * is called, including the definitive id and the new status: MegaChatMessage::STATUS_SERVER_RECEIVED.
     * At this point, the app should refresh the message identified by the temporal id and move it to
     * the final position in the history, based on the reported index in the callback.
     *
     * If the message is rejected by the server, the message will keep its temporal id and will have its
     * a message id set to MEGACHAT_INVALID_HANDLE.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodehandle Handle of the node that the user wants to attach
     * @param listener MegaChatRequestListener to track this request
     */
    public void attachNode(long chatid, long nodehandle, MegaChatRequestListenerInterface listener){
        megaChatApi.attachNode(chatid, nodehandle, createDelegateRequestListener(listener));
    }

    /**
     * Revoke the access to a node granted by an attachment message
     *
     * The attachment message will be deleted as any other message. Therefore,
     *
     * The revoke is actually a deletion of the former message. Hence, the behavior is the
     * same than a regular deletion.
     * @see MegaChatApi::editMessage or MegaChatApi::deleteMessage for more information.
     *
     * If the revoke is rejected because the attachment message is too old, or if the message is
     * not an attachment message, this function returns NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return MegaChatMessage that will be modified. NULL if the message cannot be edited (too old)
     */
    public MegaChatMessage revokeAttachmentMessage(long chatid, long msgid){
        return megaChatApi.revokeAttachmentMessage(chatid, msgid);
    }

    /** Returns whether the logged in user has been granted access to the node
     *
     * Access to attached nodes received in chatrooms is granted when the message
     * is sent, but it can be revoked afterwards.
     *
     * This convenience method allows to check if you still have access to a node
     * or it was revoked. Usually, apps will show the attachment differently when
     * access has been revoked.
     *
     * @note The returned value will be valid only for nodes attached to messages
     * already loaded in an opened chatroom. The list of revoked nodes is updated
     * accordingly while the chatroom is open, based on new messages received.
     *
     * @deprecated This function must NOT be used in new developments. It will eventually become obsolete.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param nodeHandle MegaChatHandle that identifies the node to check its access
     *
     * @return True if the user has access to the node in this chat.
     */
    public boolean isRevoked(long chatid, long nodeHandle){
        return megaChatApi.isRevoked(chatid, nodeHandle);
    }

    /**
     * Edits an existing message
     *
     * Message's edits are only allowed during a short timeframe, usually 1 hour.
     * Message's deletions are equivalent to message's edits, but with empty content.
     *
     * There is only one pending edit for not-yet confirmed edits. Therefore, this function will
     * discard previous edits that haven't been notified via MegaChatRoomListener::onMessageUpdate
     * where the message has MegaChatMessage::hasChanged(MegaChatMessage::CHANGE_TYPE_CONTENT).
     *
     * If the edits is rejected... // TODO:
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @param msg New content of the message
     *
     * @return MegaChatMessage that will be modified. NULL if the message cannot be edited (too old)
     */
    public MegaChatMessage editMessage(long chatid, long msgid, String msg){
        return megaChatApi.editMessage(chatid, msgid, msg);
    }

    /**
     * Deletes an existing message
     *
     * @note Message's deletions are equivalent to message's edits, but with empty content.
     * @see \c MegaChatapi::editMessage for more information.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return MegaChatMessage that will be deleted. NULL if the message cannot be deleted (too old)
     */
    public MegaChatMessage deleteMessage(long chatid, long msgid){
        return megaChatApi.deleteMessage(chatid, msgid);
    }

    /**
     * Sets the last-seen-by-us pointer to the specified message
     *
     * The last-seen-by-us pointer is persisted in the account, so every client will
     * be aware of the last-seen message.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     *
     * @return False if the \c chatid is invalid or the message is older
     * than last-seen-by-us message. True if success.
     */
    public boolean setMessageSeen(long chatid, long msgid){
        return megaChatApi.setMessageSeen(chatid, msgid);
    }

    /**
     * Returns the last-seen-by-us message
     *
     * @param chatid MegaChatHandle that identifies the chat room
     *
     * @return The last-seen-by-us MegaChatMessage, or NULL if error.
     */
    public MegaChatMessage getLastMessageSeen(long chatid){
        return megaChatApi.getLastMessageSeen(chatid);
    }

    /**
     *  Returns message id of the last-seen-by-us message
     *
     * @param chatid MegaChatHandle that identifies the chat room
     *
     * @return Message id for the last-seen-by-us, or invalid handle if \c chatid is invalid or
     * the user has not seen any message in that chat
     */
    public long getLastMessageSeenId(long chatid){
        return megaChatApi.getLastMessageSeenId(chatid);
    }

    /**
     * Removes the unsent message from the queue
     *
     * Messages with status MegaChatMessage::STATUS_SENDING_MANUAL should be
     * removed from the manual send queue after user discards them or resends them.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param tempId Temporal id of the message, as returned by MegaChatMessage::getTempId.
     */
    public void removeUnsentMessage(long chatid, long tempId){
        megaChatApi.removeUnsentMessage(chatid, tempId);
    }

    /**
     * Send a notification to the chatroom that the user is typing
     *
     * Other peers in the chatroom will receive a notification via
     * \c MegaChatRoomListener::onChatRoomUpdate with the change type
     * \c MegaChatRoom::CHANGE_TYPE_USER_TYPING. \see MegaChatRoom::getUserTyping.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SEND_TYPING_NOTIF
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListenerInterface to track this request
     */
    public void sendTypingNotification(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.sendTypingNotification(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Send a notification to the chatroom that the user is typing
     *
     * Other peers in the chatroom will receive a notification via
     * \c MegaChatRoomListener::onChatRoomUpdate with the change type
     * \c MegaChatRoom::CHANGE_TYPE_USER_TYPING. \see MegaChatRoom::getUserTyping.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     */
    public void sendTypingNotification(long chatid){
        megaChatApi.sendTypingNotification(chatid);
    }

    /**
     * Send a notification to the chatroom that the user has stopped typing
     *
     * This method has to be called when the text edit label is cleared
     *
     * Other peers in the chatroom will receive a notification via
     * \c MegaChatRoomListener::onChatRoomUpdate with the change type
     * \c MegaChatRoom::CHANGE_TYPE_USER_STOP_TYPING. \see MegaChatRoom::getUserTyping.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SEND_TYPING_NOTIF
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void sendStopTypingNotification(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.sendStopTypingNotification(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Send a notification to the chatroom that the user has stopped typing
     *
     * This method has to be called when the text edit label is cleared
     *
     * Other peers in the chatroom will receive a notification via
     * \c MegaChatRoomListener::onChatRoomUpdate with the change type
     * \c MegaChatRoom::CHANGE_TYPE_USER_STOP_TYPING. \see MegaChatRoom::getUserTyping.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SEND_TYPING_NOTIF
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     */
    public void sendStopTypingNotification(long chatid){
        megaChatApi.sendStopTypingNotification(chatid);
    }

    /**
     * Saves the current state
     *
     * The DB cache works with transactions. In order to prevent losing recent changes when the app
     * dies abruptly (usual case in mobile apps), it is recommended to call this method, so the
     * transaction is committed.
     *
     * This method should be called ONLY when the app is prone to be killed, whether by the user or the
     * operative system. Otherwise, transactions are committed regularly.
     */
    public void saveCurrentState(){
            megaChatApi.saveCurrentState();
    }

    // Call management
    /**
     * Start a call in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_START_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true if it is a video-audio call or false for audio call
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param listener MegaChatRequestListener to track this request
     */
    public void startChatCall(long chatid, boolean enableVideo, MegaChatRequestListenerInterface listener)
    {
        megaChatApi.startChatCall(chatid, enableVideo, createDelegateRequestListener(listener));
    }

    /**
     * Answer a call received in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ANSWER_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true if it is a video-audio call or false for audio call
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param listener MegaChatRequestListener to track this request
     */
    public void answerChatCall(long chatid, boolean enableVideo, MegaChatRequestListenerInterface listener)
    {
        megaChatApi.answerChatCall(chatid, enableVideo, createDelegateRequestListener(listener));
    }

    /**
     * Hang a call in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */

    public void hangChatCall(long chatid, MegaChatRequestListenerInterface listener)
    {
        megaChatApi.hangChatCall(chatid, createDelegateRequestListener(listener));
    }
    /**
     * Hang all active calls
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void hangAllChatCalls(MegaChatRequestListenerInterface listener)
    {
        megaChatApi.hangAllChatCalls(createDelegateRequestListener(listener));
    }

    /**
     * Enable audio for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getParamType - Returns MegaChatRequest::AUDIO
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void enableAudio(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.enableAudio(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Disable audio for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getParamType - Returns MegaChatRequest::AUDIO
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void disableAudio(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.disableAudio(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Enable video for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getParamType - MegaChatRequest::VIDEO
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void enableVideo(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.enableVideo(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Disable video for a call that is in progress
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DISABLE_AUDIO_VIDEO_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getParamType - Returns MegachatRequest::VIDEO
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void disableVideo(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.disableVideo(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Search all audio and video devices at the system at that moment.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES
     * After call this funciton, available devices can be obtained calling getChatAudioInDevices
     * or getChatVideoInDevices
     *
     * @param listener MegaChatRequestListener to track this request
     */

    /**
     * Search all audio and video devices at the system at that moment.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES
     * After call this funciton, available devices can be obtained calling getChatAudioInDevices
     * or getChatVideoInDevices
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void loadAudioVideoDeviceList(MegaChatRequestListenerInterface listener)
    {
        megaChatApi.loadAudioVideoDeviceList(createDelegateRequestListener(listener));
    }

    /**
     * Get the MegaChatCall associated with a chatRoom
     *
     * If chatId is invalid or there isn't any MegaChatCall associated with the chatroom, NULL is
     * returned
     *
     * You take the ownership of the returned value
     *
     * @param chatId MegaChatHandle that identifies the chat room
     * @return MegaChatCall object associated with chatid or NULL if it doesn't exist
     */
    public MegaChatCall getChatCall(long chatId){
        return megaChatApi.getChatCall(chatId);
    }

    /**
     * Mark as ignored the MegaChatCall associated with a chatroom
     *
     * @param chatid MegaChatHandle that identifies the chat room
     */
    public void setIgnoredCall(long chatid){
        megaChatApi.setIgnoredCall(chatid);
    }

    /**
     * Get the MegaChatCall that has a specific handle
     *
     * You can get the handle of  a MegaChatCall using MegaChatCall::getId().
     *
     * You take the ownership of the returned value
     *
     * @param callId MegaChatHandle that identifies the call
     * @return MegaChatCall object for the specified \c chatid. NULL if call doesn't exist
     */
    public MegaChatCall getChatCallByCallId(long callId){
        return megaChatApi.getChatCallByCallId(callId);
    }

    /**
     * Returns number of calls that there are at the system
     * @return number of calls in the system
     */
    public int getNumCalls(){
        return megaChatApi.getNumCalls();
    }

    /**
     * Get MegaChatHandle list that contains chatrooms identifier where there is an active call
     *
     * You take the ownership of the returned value
     *
     * @return A list of handles with chatroom identifier where there is an active call
     */
    public MegaHandleList getChatCalls(){
        return megaChatApi.getChatCalls();
    }

    /**
     * Get a list with the ids of active calls
     *
     * You take the ownership of the returned value.
     *
     * @return A list of ids of active calls
     */
    public MegaHandleList getChatCallsIds(){
        return megaChatApi.getChatCallsIds();
    }

    public static void setCatchException(boolean enable) {
        MegaChatApi.setCatchException(enable);
    }

    /**
     * Set the active log level.
     * <p>
     * This function sets the log level of the logging system. If you set a log listener using
     * MegaApiJava.setLoggerObject(), you will receive logs with the same or a lower level than
     * the one passed to this function.
     *
     * @param logLevel
     *            Active log level. These are the valid values for this parameter: <br>
     *                Valid values are:
     * - MegaChatApi::LOG_LEVEL_ERROR   = 1
     * - MegaChatApi::LOG_LEVEL_WARNING = 2
     * - MegaChatApi::LOG_LEVEL_INFO    = 3
     * - MegaChatApi::LOG_LEVEL_VERBOSE = 4
     * - MegaChatApi::LOG_LEVEL_DEBUG   = 5
     * - MegaChatApi::LOG_LEVEL_MAX     = 6
     *            - MegaApiJava.LOG_LEVEL_FATAL = 0. <br>
     *            - MegaApiJava.LOG_LEVEL_ERROR = 1. <br>
     *            - MegaApiJava.LOG_LEVEL_WARNING = 2. <br>
     *            - MegaApiJava.LOG_LEVEL_INFO = 3. <br>
     *            - MegaApiJava.LOG_LEVEL_DEBUG = 4. <br>
     *            - MegaApiJava.LOG_LEVEL_MAX = 5.
     */
    public static void setLogLevel(int logLevel) {
        MegaChatApi.setLogLevel(logLevel);
    }

    /**
     * Set a MegaLogger implementation to receive SDK logs.
     * <p>
     * Logs received by this objects depends on the active log level.
     * By default, it is MegaApiJava.LOG_LEVEL_INFO. You can change it
     * using MegaApiJava.setLogLevel().
     *
     * @param megaLogger
     *            MegaChatLogger implementation.
     */
    public static void setLoggerObject(MegaChatLoggerInterface megaLogger) {
        DelegateMegaChatLogger newLogger = new DelegateMegaChatLogger(megaLogger);
        MegaChatApi.setLoggerObject(newLogger);
        logger = newLogger;
    }

    private MegaChatRequestListener createDelegateRequestListener(MegaChatRequestListenerInterface listener) {
        DelegateMegaChatRequestListener delegateListener = new DelegateMegaChatRequestListener(this, listener, true);
        activeRequestListeners.add(delegateListener);
        return delegateListener;
    }

    private MegaChatRequestListener createDelegateRequestListener(MegaChatRequestListenerInterface listener, boolean singleListener) {
        DelegateMegaChatRequestListener delegateListener = new DelegateMegaChatRequestListener(this, listener, singleListener);
        activeRequestListeners.add(delegateListener);
        return delegateListener;
    }

    private MegaChatRoomListener createDelegateChatRoomListener(MegaChatRoomListenerInterface listener) {
        DelegateMegaChatRoomListener delegateListener = new DelegateMegaChatRoomListener(this, listener);
        activeChatRoomListeners.add(delegateListener);
        return delegateListener;
    }

    private MegaChatListener createDelegateChatListener(MegaChatListenerInterface listener) {
        DelegateMegaChatListener delegateListener = new DelegateMegaChatListener(this, listener);
        activeChatListeners.add(delegateListener);
        return delegateListener;
    }

    private MegaChatCallListener createDelegateChatCallListener(MegaChatCallListenerInterface listener) {
        DelegateMegaChatCallListener delegateListener = new DelegateMegaChatCallListener(this, listener);
        activeChatCallListeners.add(delegateListener);
        return delegateListener;
    }

    private MegaChatVideoListener createDelegateChatVideoListener(MegaChatVideoListenerInterface listener, boolean remote) {
        DelegateMegaChatVideoListener delegateListener = new DelegateMegaChatVideoListener(this, listener, remote);
        activeChatVideoListeners.add(delegateListener);
        return delegateListener;
    }

    private MegaChatNotificationListener createDelegateChatNotificationListener(MegaChatNotificationListenerInterface listener) {
        DelegateMegaChatNotificationListener delegateListener = new DelegateMegaChatNotificationListener(this, listener);
        activeChatNotificationListeners.add(delegateListener);
        return delegateListener;
    }

    void privateFreeRequestListener(DelegateMegaChatRequestListener listener) {
        activeRequestListeners.remove(listener);
    }

    static ArrayList<MegaChatRoom> chatRoomListToArray(MegaChatRoomList chatRoomList) {

        if (chatRoomList == null) {
            return null;
        }

        ArrayList<MegaChatRoom> result = new ArrayList<MegaChatRoom>((int)chatRoomList.size());
        for (int i = 0; i < chatRoomList.size(); i++) {
            result.add(chatRoomList.get(i).copy());
        }

        return result;
    }

    static ArrayList<MegaChatListItem> chatRoomListItemToArray(MegaChatListItemList chatRoomItemList) {

        if (chatRoomItemList == null) {
            return null;
        }

        ArrayList<MegaChatListItem> result = new ArrayList<MegaChatListItem>((int)chatRoomItemList.size());
        for (int i = 0; i < chatRoomItemList.size(); i++) {
            result.add(chatRoomItemList.get(i).copy());
        }

        return result;
    }
};
