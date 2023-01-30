package nz.mega.sdk;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Set;

public class MegaChatApiJava {
    MegaChatApi megaChatApi;
    static DelegateMegaChatLogger logger;

    /**
     * MEGACHAT_INVALID_HANDLE Invalid value for a handle
     *
     * This value is used to represent an invalid handle. Several MEGA objects can have
     * a handle but it will never be MEGACHAT_INVALID_HANDLE.
     */
    public final static long MEGACHAT_INVALID_HANDLE = ~(long)0;
    public final static int MEGACHAT_INVALID_INDEX = 0x7fffffff;

    // Error information but application will continue run.
    public final static int LOG_LEVEL_ERROR = MegaChatApi.LOG_LEVEL_ERROR;
    // Information representing errors in applicationThe autoaway settings are preserved even when the auto-away mechanism  but application will keep running
    public final static int LOG_LEVEL_WARNING = MegaChatApi.LOG_LEVEL_WARNING;
    // Mainly useful to represent current progress of application.
    public final static int LOG_LEVEL_INFO = MegaChatApi.LOG_LEVEL_INFO;
    public final static int LOG_LEVEL_VERBOSE = MegaChatApi.LOG_LEVEL_VERBOSE;
    // Informational logs, that aMegaChatPresenceConfigre useful for developers. Only applicable if DEBUG is defined.
    public final static int LOG_LEVEL_DEBUG = MegaChatApi.LOG_LEVEL_DEBUG;
    public final static int LOG_LEVEL_MAX = MegaChatApi.LOG_LEVEL_MAX;

    static Set<DelegateMegaChatRequestListener> activeRequestListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatRequestListener>());
    static Set<DelegateMegaChatListener> activeChatListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatListener>());
    static Set<DelegateMegaChatRoomListener> activeChatRoomListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatRoomListener>());
    static Set<DelegateMegaChatCallListener> activeChatCallListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatCallListener>());
    static Set<DelegateMegaChatVideoListener> activeChatVideoListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatVideoListener>());
    static Set<DelegateMegaChatNotificationListener> activeChatNotificationListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatNotificationListener>());
    static Set<DelegateMegaChatNodeHistoryListener> activeChatNodeHistoryListeners = Collections.synchronizedSet(new LinkedHashSet<DelegateMegaChatNodeHistoryListener>());
    static Set<DelegateMegaChatScheduledMeetingListener> activeChatScheduledMeetingListeners = Collections.synchronizedSet(new LinkedHashSet<>());

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

    /**
     * Adds a reaction for a message in a chatroom
     *
     * The reactions updates will be notified one by one through the MegaChatRoomListener
     * specified at MegaChatApi::openChatRoom (and through any other listener you may have
     * registered by calling MegaChatApi::addChatRoomListener). The corresponding callback
     * is MegaChatRoomListener::onReactionUpdate.
     *
     * Note that receiving an onRequestFinish with the error code MegaChatError::ERROR_OK, does not ensure
     * that add reaction has been applied in chatd. As we've mentioned above, reactions updates will
     * be notified through callback MegaChatRoomListener::onReactionUpdate.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_MANAGE_REACTION
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatid that identifies the chatroom
     * - MegaChatRequest::getUserHandle - Returns the msgid that identifies the message
     * - MegaChatRequest::getText - Returns a UTF-8 NULL-terminated string that represents the reaction
     * - MegaChatRequest::getFlag - Returns true indicating that requested action is add reaction
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - if reaction is NULL or the msgid references a management message.
     * - MegaChatError::ERROR_NOENT - if the chatroom/message doesn't exists
     * - MegaChatError::ERROR_ACCESS - if our own privilege is different than MegaChatPeerList::PRIV_STANDARD
     * or MegaChatPeerList::PRIV_MODERATOR.
     * - MegaChatError::ERROR_EXIST - if our own user has reacted previously with this reaction for this message
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL-terminated string that represents the reaction
     * @param listener MegaChatRequestListener to track this request
     */
    public void addReaction(long chatid, long msgid, String reaction, MegaChatRequestListenerInterface listener) {
        megaChatApi.addReaction(chatid, msgid, reaction, createDelegateRequestListener(listener));
    }

    /**
     * Removes a reaction for a message in a chatroom
     *
     * The reactions updates will be notified one by one through the MegaChatRoomListener
     * specified at MegaChatApi::openChatRoom (and through any other listener you may have
     * registered by calling MegaChatApi::addChatRoomListener). The corresponding callback
     * is MegaChatRoomListener::onReactionUpdate.
     *
     * Note that receiving an onRequestFinish with the error code MegaChatError::ERROR_OK, does not ensure
     * that remove reaction has been applied in chatd. As we've mentioned above, reactions updates will
     * be notified through callback MegaChatRoomListener::onReactionUpdate.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_MANAGE_REACTION
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatid that identifies the chatroom
     * - MegaChatRequest::getUserHandle - Returns the msgid that identifies the message
     * - MegaChatRequest::getText - Returns a UTF-8 NULL-terminated string that represents the reaction
     * - MegaChatRequest::getFlag - Returns false indicating that requested action is remove reaction
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS: if reaction is NULL or the msgid references a management message.
     * - MegaChatError::ERROR_NOENT: if the chatroom/message doesn't exists
     * - MegaChatError::ERROR_ACCESS: if our own privilege is different than MegaChatPeerList::PRIV_STANDARD
     * or MegaChatPeerList::PRIV_MODERATOR
     * - MegaChatError::ERROR_EXIST - if your own user has not reacted to the message with the specified reaction.
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL-terminated string that represents the reaction
     * @param listener MegaChatRequestListener to track this request
     */
    public void delReaction(long chatid, long msgid, String reaction, MegaChatRequestListenerInterface listener) {
        megaChatApi.delReaction(chatid, msgid, reaction, createDelegateRequestListener(listener));
    }

    /**
     * Returns the number of users that reacted to a message with a specific reaction
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL terminated string that represents the reactiongaC
     *
     * @return return the number of users that reacted to a message with a specific reaction,
     * or -1 if the chatroom or message is not found.
     */
    public int getMessageReactionCount(long chatid, long msgid, String reaction) {
        return megaChatApi.getMessageReactionCount(chatid, msgid, reaction);
    }

    /**
     * Gets a list of reactions associated to a message
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @return return a list with the reactions associated to a message.
     */
    public MegaStringList getMessageReactions(long chatid, long msgid) {
        return megaChatApi.getMessageReactions(chatid, msgid);
    }

    /**
     * Gets a list of users that reacted to a message with a specific reaction
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chatroom
     * @param msgid MegaChatHandle that identifies the message
     * @param reaction UTF-8 NULL terminated string that represents the reaction
     *
     * @return return a list with the users that reacted to a message with a specific reaction.
     */
    public MegaHandleList getReactionUsers(long chatid, long msgid, String reaction) {
        return megaChatApi.getReactionUsers(chatid, msgid, reaction);
    }

    /**
     * Enable / disable the public key pinning
     *
     * Public key pinning is enabled by default for all sensible communications.
     * It is strongly discouraged to disable this feature.
     *
     * @param enable true to keep public key pinning enabled, false to disable it
     */
    public void setPublicKeyPinning(boolean enable) {
        megaChatApi.setPublicKeyPinning(enable);
    }

    public void addChatRequestListener(MegaChatRequestListenerInterface listener)
    {
        megaChatApi.addChatRequestListener(createDelegateRequestListener(listener, false));
    }

    public void addChatListener(MegaChatListenerInterface listener)
    {
        megaChatApi.addChatListener(createDelegateChatListener(listener));
    }

    /**
     * Register a listener to receive all events about calls
     *
     * You can use MegaChatApi::removeChatCallListener to stop receiving events.
     *
     * @param listener MegaChatCallListener that will receive all call events
     */
    public void addChatCallListener(MegaChatCallListenerInterface listener)
    {
        megaChatApi.addChatCallListener(createDelegateChatCallListener(listener));
    }

    /**
     * Register a listener to receive all events about scheduled meetings
     *
     * You can use MegaChatApi::removeSchedMeetingListener to stop receiving events.
     *
     * @param listener MegaChatScheduledMeetingListener that will receive all scheduled meetings events
     */
    public void addSchedMeetingListener(MegaChatScheduledMeetingListenerInterface listener)
    {
        megaChatApi.addSchedMeetingListener(createDelegateChatScheduledMeetingListener(listener));
    }

    /**
     * Unregister a MegaChatCallListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
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

    /**
     * Unregister a MegaChatScheduledMeetingListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */
    public void removeSchedMeetingListener(MegaChatScheduledMeetingListenerInterface listener) {
        ArrayList<DelegateMegaChatScheduledMeetingListener> listenersToRemove = new ArrayList<>();
        synchronized (activeChatScheduledMeetingListeners) {
            Iterator<DelegateMegaChatScheduledMeetingListener> it = activeChatScheduledMeetingListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatScheduledMeetingListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i=0;i<listenersToRemove.size();i++){
            megaChatApi.removeSchedMeetingListener(listenersToRemove.get(i));
        }
    }

    /**
     * Register a listener to receive video from local device for an specific chat room
     *
     * You can use MegaChatApi::removeChatLocalVideoListener to stop receiving events.
     *
     * @note if we want to receive video before start a call (openVideoDevice), we have to
     * register a MegaChatVideoListener with chatid = MEGACHAT_INVALID_HANDLE
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatVideoListener that will receive local video
     */
    public void addChatLocalVideoListener(long chatid, MegaChatVideoListenerInterface listener)
    {
        megaChatApi.addChatLocalVideoListener(chatid, createDelegateChatVideoListener(listener, false));
    }

    /**
     * Register a listener to receive video from remote device for an specific chat room and peer
     *
     * You can use MegaChatApi::removeChatRemoteVideoListener to stop receiving events.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client
     * @param hiRes boolean that identify if video is high resolution or low resolution
     * @param listener MegaChatVideoListener that will receive remote video
     */
    public void addChatRemoteVideoListener(long chatid, long clientId, boolean hiRes, MegaChatVideoListenerInterface listener)
    {
        megaChatApi.addChatRemoteVideoListener(chatid, clientId, hiRes, createDelegateChatVideoListener(listener, true));
    }

    /**
     * Unregister a MegaChatVideoListener
     *
     * This listener won't receive more events.
     * @note if we want to remove the listener added to receive video frames before start a call
     * we have to use chatid = MEGACHAT_INVALID_HANDLE
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client
     * @param hiRes boolean that identify if video is high resolution or low resolution
     * @param listener Object that is unregistered
     */
    public void removeChatVideoListener(long chatid, long clientId, boolean hiRes, MegaChatVideoListenerInterface listener) {
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
                megaChatApi.removeChatRemoteVideoListener(chatid, clientId, hiRes, delegateListener);
            }
            else {
                megaChatApi.removeChatLocalVideoListener(chatid, delegateListener);
            }
        }
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

    public int init(String sid)
    {
        return megaChatApi.init(sid);
    }

    /**
     * @brief Initializes karere in anonymous mode for preview of chat-links
     *
     * The initialization state will be MegaChatApi::INIT_ANONYMOUS if successful. In
     * case of initialization error, it will return MegaChatApi::INIT_ERROR.
     *
     * This function should be called to preview chat-links without a valid session (anonymous mode).
     *
     * The anonymous mode is going to initialize the chat engine but is not going to login in MEGA,
     * so the way to logout in anoymous mode is call MegaChatApi::logout manually.
     *
     * @return The initialization state
     */
    public int initAnonymous(){
        return megaChatApi.initAnonymous();
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
    public void retryPendingConnections(boolean disconnect, MegaChatRequestListenerInterface listener){
        megaChatApi.retryPendingConnections(disconnect, createDelegateRequestListener(listener));
    }

    /**
     * @brief Refresh URLs and establish fresh connections
     *
     * The associated request type with this request is MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS
     *
     * A disconnect will be forced automatically, followed by a reconnection to the fresh URLs
     * retrieved from API. This parameter is useful when the URL for the API is changed
     * via MegaApi::changeApiUrl.
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void refreshUrl(MegaChatRequestListenerInterface listener){
        megaChatApi.refreshUrl(createDelegateRequestListener(listener));
    }

    /**
     * @brief Refresh URLs and establish fresh connections
     *
     * The associated request type with this request is MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS
     *
     * A disconnect will be forced automatically, followed by a reconnection to the fresh URLs
     * retrieved from API. This parameter is useful when the URL for the API is changed
     * via MegaApi::changeApiUrl.
     */
    public void refreshUrl(){ megaChatApi.refreshUrl(); }

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
     * Logout of chat servers without invalidating the session
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
     * - MegaChatRequest::getPrivilege - Returns zero (private mode)
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
     * - MegaChatRequest::getPrivilege - Returns zero (private mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
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
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param listener MegaChatRequestListener to track this request
     */
    public void createChat(boolean group, MegaChatPeerList peers, String title, MegaChatRequestListenerInterface listener){
        megaChatApi.createChat(group, peers, title, createDelegateRequestListener(listener));
    }

    /**
     * Creates a group chat for one or more participants, allowing you to specify their permissions and creation chat options
     *
     * The creator of the chat will have moderator level privilege and should not be included in the
     * list of peers.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns if the new chat is a group chat or permanent chat
     * - MegaChatRequest::getPrivilege - Returns zero (private mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @note If you are trying to create a chat with more than 1 other person, then it will be forced
     * to be a group chat.
     *
     * @note If peers list contains only one person, group chat is not set and a permament chat already
     * exists with that person, then this call will return the information for the existing chat, rather
     * than a new chat.
     *
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param listener MegaChatRequestListener to track this request
     */
    public void createGroupChat(MegaChatPeerList peers, String title, boolean speakRequest, boolean waitingRoom, boolean openInvite, MegaChatRequestListenerInterface listener){
        megaChatApi.createGroupChat(peers, title, speakRequest, waitingRoom, openInvite, createDelegateRequestListener(listener));
    }

    /**
     * Creates an public chatroom for multiple participants (groupchat)
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If no peer list is provided or non groupal and public is set.
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     * - MegaChatError::ERROR_ACCESS - If no peers are provided for a 1on1 chatroom.
     *
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param listener MegaChatRequestListener to track this request
     */
    public void createPublicChat(MegaChatPeerList peers, String title, MegaChatRequestListenerInterface listener){
        megaChatApi.createPublicChat(peers, title, createDelegateRequestListener(listener));
    }

    /**
     * Creates an public chatroom for multiple participants (groupchat) allowing you to specify creation chat options
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @param peers MegaChatPeerList including other users and their privilege level
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param listener MegaChatRequestListener to track this request
     */
    public void createPublicChat(MegaChatPeerList peers, String title, boolean speakRequest, boolean waitingRoom, boolean openInvite, MegaChatRequestListenerInterface listener){
        megaChatApi.createPublicChat(peers, title, speakRequest, waitingRoom, openInvite, createDelegateRequestListener(listener));
    }

    /**
     * Creates a meeting
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getNumber - Returns always 1, since the chatroom is a meeting
     * -  MegaChatRequest::getUrl - Retruns url for the meeting
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If no peer list is provided or non groupal and public is set.
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     * - MegaChatError::ERROR_ACCESS - If no peers are provided for a 1on1 chatroom.
     *
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param listener MegaChatRequestListener to track this request
     */
    public void createMeeting(String title, MegaChatRequestListenerInterface listener){
        megaChatApi.createMeeting(title, createDelegateRequestListener(listener));
    }

    /**
     * Creates a meeting
     *
     * This function allows to create public chats, where the moderator can create chat links to share
     * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
     * moderator can create/get a public handle for the chatroom and generate a URL by using
     * \c MegaChatApi::createChatLink. The chat-link can be deleted at any time by any moderator
     * by using \c MegaChatApi::removeChatLink.
     *
     * The chatroom remains in the public mode until a moderator calls \c MegaChatApi::setPublicChatToPrivate.
     *
     * Any user can preview the chatroom thanks to the chat-link by using \c MegaChatApi::openChatPreview.
     * Any user can join the chatroom thanks to the chat-link by using \c MegaChatApi::autojoinPublicChat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns always true, since the new chat is a groupchat
     * - MegaChatRequest::getPrivilege - Returns one (public mode)
     * - MegaChatRequest::getMegaChatPeerList - List of participants and their privilege level
     * - MegaChatRequest::getText - Returns the title of the chat.
     * - MegaChatRequest::getNumber - Returns always 1, since the chatroom is a meeting
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the handle of the new chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT  - If the target user is the same user as caller
     * - MegaChatError::ERROR_ACCESS - If the target is not actually contact of the user.
     *
     * @param title Null-terminated character string with the chat title. If the title
     * is longer than 30 characters, it will be truncated to that maximum length.
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param listener MegaChatRequestListener to track this request
     */
    public void createMeeting(String title, boolean speakRequest, boolean waitingRoom, boolean openInvite, MegaChatRequestListenerInterface listener){
        megaChatApi.createMeeting(title, speakRequest, waitingRoom, openInvite, createDelegateRequestListener(listener));
    }

    /**
     * Creates a chatroom and a scheduled meeting for that chatroom
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always true as we are going to create a new chatroom
     * - MegaChatRequest::request->getNumber - Returns true if new chat is going to be a Meeting room
     * - MegaChatRequest::request->getPrivilege - Returns true is new chat is going to be a public chat room
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     *  + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     *  + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     *  + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     * - MegaChatError::ERROR_ARGS  - if isMeeting is set true but publicChat is set to false
     *
     * @param isMeeting True to create a meeting room
     * @param publicChat True to create a public chat, otherwise false
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
     * A moderator user must grant each user access to the call.
     * @param openInvite to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param timezone Timezone where we want to schedule the meeting
     * @param startDate Start date time of the meeting in Unix timestamp
     * @param endDate End date time of the meeting in Unix timestamp
     * @param title Null-terminated character string with the scheduled meeting title. Maximum allowed length is XX characters
     * @param description Null-terminated character string with the scheduled meeting description. Maximum allowed length is XX characters
     * @param flags Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     * @param attributes - not supported yet
     * @param listener MegaChatRequestListener to track this request
     */
    public void createChatAndScheduledMeeting(boolean isMeeting, boolean publicChat, boolean speakRequest, boolean waitingRoom, boolean openInvite,
                                              String timezone, long startDate, long endDate, String title, String description, MegaChatScheduledFlags flags,
                                              MegaChatScheduledRules rules, String attributes, MegaChatRequestListenerInterface listener) {
        megaChatApi.createChatAndScheduledMeeting(isMeeting, publicChat, speakRequest, waitingRoom, openInvite, timezone, startDate, endDate, title, description,
                flags, rules, attributes, createDelegateRequestListener(listener));
    }

    /**
     * Creates a chatroom and a scheduled meeting for that chatroom
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always true as we are going to create a new chatroom
     * - MegaChatRequest::request->getNumber - Returns true if new chat is going to be a Meeting room
     * - MegaChatRequest::request->getPrivilege - Returns true is new chat is going to be a public chat room
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     * + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     * + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     * + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     * - MegaChatError::ERROR_ARGS  - if isMeeting is set true but publicChat is set to false
     *
     * @param isMeeting    True to create a meeting room
     * @param publicChat   True to create a public chat, otherwise false
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom  True to set that during calls, non moderator members will be placed into a waiting room.
     *                     A moderator user must grant each user access to the call.
     * @param openInvite   to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param timezone     Timezone where we want to schedule the meeting
     * @param startDate    Start date time of the meeting in Unix timestamp
     * @param endDate      End date time of the meeting in Unix timestamp
     * @param title        Null-terminated character string with the scheduled meeting title. Maximum allowed length is XX characters
     * @param description  Null-terminated character string with the scheduled meeting description. Maximum allowed length is XX characters
     * @param flags        Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules        Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     * @param attributes   - not supported yet
     */
    public void createChatAndScheduledMeeting(boolean isMeeting, boolean publicChat, boolean speakRequest, boolean waitingRoom, boolean openInvite,
                                              String timezone, long startDate, long endDate, String title, String description, MegaChatScheduledFlags flags,
                                              MegaChatScheduledRules rules, String attributes) {
        megaChatApi.createChatAndScheduledMeeting(isMeeting, publicChat, speakRequest, waitingRoom, openInvite, timezone, startDate, endDate, title, description, flags, rules, attributes);
    }

    /**
     * Creates a chatroom and a scheduled meeting for that chatroom
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always true as we are going to create a new chatroom
     * - MegaChatRequest::request->getNumber - Returns true if new chat is going to be a Meeting room
     * - MegaChatRequest::request->getPrivilege - Returns true is new chat is going to be a public chat room
     * - MegaChatRequest::getParamType - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
     * + To check if speakRequest was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_SPEAK_REQUEST, bitmask)
     * + To check if waitingRoom was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_WAITING_ROOM, bitmask)
     * + To check if openInvite was true you need to call MegaChatApiImpl::hasChatOptionEnabled(CHAT_OPTION_OPEN_INVITE, bitmask)
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     * - MegaChatError::ERROR_ARGS  - if isMeeting is set true but publicChat is set to false
     *
     * @param isMeeting    True to create a meeting room
     * @param publicChat   True to create a public chat, otherwise false
     * @param speakRequest True to set that during calls non moderator users, must request permission to speak
     * @param waitingRoom  True to set that during calls, non moderator members will be placed into a waiting room.
     *                     A moderator user must grant each user access to the call.
     * @param openInvite   to set that users with MegaChatRoom::PRIV_STANDARD privilege, can invite other users into the chat
     * @param timezone     Timezone where we want to schedule the meeting
     * @param startDate    Start date time of the meeting in Unix timestamp
     * @param endDate      End date time of the meeting in Unix timestamp
     * @param title        Null-terminated character string with the scheduled meeting title. Maximum allowed length is XX characters
     * @param description  Null-terminated character string with the scheduled meeting description. Maximum allowed length is XX characters
     * @param flags        Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules        Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     */
    public void createChatAndScheduledMeeting(boolean isMeeting, boolean publicChat, boolean speakRequest, boolean waitingRoom, boolean openInvite,
                                              String timezone, long startDate, long endDate, String title, String description, MegaChatScheduledFlags flags,
                                              MegaChatScheduledRules rules) {
        megaChatApi.createChatAndScheduledMeeting(isMeeting, publicChat, speakRequest, waitingRoom, openInvite, timezone, startDate, endDate, title, description, flags, rules);
    }

    /**
     * Modify an existing scheduled meeting
     *
     * Note: this action won't create a child scheduled meeting
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getNumber - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getPrivilege - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     *
     * @param chatid      MegaChatHandle that identifies a chat room
     * @param schedId     MegaChatHandle that identifies the scheduled meeting
     * @param timezone    Timezone where we want to schedule the meeting
     * @param startDate   Start date time of the meeting in Unix timestamp
     * @param endDate     End date time of the meeting in Unix timestamp
     * @param title       Null-terminated character string with the scheduled meeting title. Maximum allowed length is XX characters
     * @param description Null-terminated character string with the scheduled meeting description. Maximum allowed length is XX characters
     * @param cancelled   True if scheduled meeting is going to be cancelled
     * @param flags       Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules       Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     * @param listener    MegaChatRequestListener to track this request
     */
    public void updateScheduledMeeting(long chatid, long schedId, String timezone, long startDate, long endDate, String title, String description, boolean cancelled, MegaChatScheduledFlags flags, MegaChatScheduledRules rules, MegaChatRequestListenerInterface listener) {
        megaChatApi.updateScheduledMeeting(chatid, schedId, timezone, startDate, endDate, title, description, cancelled, flags, rules, createDelegateRequestListener(listener));
    }

    /**
     * Modify an existing scheduled meeting
     *
     * Note: this action won't create a child scheduled meeting
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getNumber - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getPrivilege - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     *
     * @param chatid      MegaChatHandle that identifies a chat room
     * @param schedId     MegaChatHandle that identifies the scheduled meeting
     * @param timezone    Timezone where we want to schedule the meeting
     * @param startDate   Start date time of the meeting in Unix timestamp
     * @param endDate     End date time of the meeting in Unix timestamp
     * @param title       Null-terminated character string with the scheduled meeting title. Maximum allowed length is XX characters
     * @param description Null-terminated character string with the scheduled meeting description. Maximum allowed length is XX characters
     * @param cancelled   True if scheduled meeting is going to be cancelled
     * @param flags       Scheduled meeting flags to establish scheduled meetings flags like avoid email sending (Check MegaChatScheduledFlags class)
     * @param rules       Repetition rules for creating a recurrent meeting (Check MegaChatScheduledRules class)
     */
    public void updateScheduledMeeting(long chatid, long schedId, String timezone, long startDate, long endDate, String title, String description, boolean cancelled, MegaChatScheduledFlags flags, MegaChatScheduledRules rules) {
        megaChatApi.updateScheduledMeeting(chatid, schedId, timezone, startDate, endDate, title, description, cancelled, flags, rules);
    }

    /**
     * Modify an existing scheduled meeting occurrence
     *
     * Note: this action will create a new child scheduled meeting whose parent schedid will be the schedid provided by this method
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getNumber - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getPrivilege - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     *
     * @param chatid       MegaChatHandle that identifies a chat room
     * @param schedId      MegaChatHandle that identifies the scheduled meeting
     * @param newStartDate start date time that along with schedId identifies the occurrence in Unix timestamp
     * @param overrides    new start date time of the occurrence in Unix timestamp
     * @param newEndDate   new end date time of the occurrence in Unix timestamp
     * @param newCancelled True if scheduled meeting is going to be cancelled
     * @param listener     MegaChatRequestListener to track this request
     */
    public void updateScheduledMeetingOccurrence(long chatid, long schedId, long overrides, long newStartDate, long newEndDate, boolean newCancelled, MegaChatRequestListenerInterface listener) {
        megaChatApi.updateScheduledMeetingOccurrence(chatid, schedId, overrides, newStartDate, newEndDate, newCancelled, createDelegateRequestListener(listener));
    }

    /**
     * Modify an existing scheduled meeting occurrence
     *
     * Note: this action will create a new child scheduled meeting whose parent schedid will be the schedid provided by this method
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CREATE_OR_UPDATE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::request->getFlag - Returns always false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getNumber - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getPrivilege - Returns false as we are going to use an existing chatroom
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList instance with a MegaChatScheduledMeeting (containing the params provided by user)
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::request->getMegaChatScheduledMeetingList - returns a MegaChatScheduledMeetingList with a MegaChatScheduledMeeting (with definitive ScheduledMeeting updated from API)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if timezone, startDateTime, endDateTime, title, or description are invalid
     *
     * @param chatid       MegaChatHandle that identifies a chat room
     * @param schedId      MegaChatHandle that identifies the scheduled meeting
     * @param newStartDate start date time that along with schedId identifies the occurrence in Unix timestamp
     * @param overrides    new start date time of the occurrence in Unix timestamp
     * @param newEndDate   new end date time of the occurrence in Unix timestamp
     * @param newCancelled True if scheduled meeting is going to be cancelled
     */
    public void updateScheduledMeetingOccurrence(long chatid, long schedId, long overrides, long newStartDate, long newEndDate, boolean newCancelled) {
        megaChatApi.updateScheduledMeetingOccurrence(chatid, schedId, overrides, newStartDate, newEndDate, newCancelled);
    }

    /**
     * Removes a scheduled meeting by scheduled meeting id and chatid
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DELETE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getUserHandle - Returns the scheduled meeting id
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid or schedId are invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom or scheduled meeting does not exists
     *
     * @param chatid   MegaChatHandle that identifies a chat room
     * @param schedId  MegaChatHandle that identifies a scheduled meeting
     * @param listener MegaChatRequestListener to track this request
     */
    public void removeScheduledMeeting(long chatid, long schedId, MegaChatRequestListenerInterface listener) {
        megaChatApi.removeScheduledMeeting(chatid, schedId, createDelegateRequestListener(listener));
    }

    /**
     * Removes a scheduled meeting by scheduled meeting id and chatid
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DELETE_SCHEDULED_MEETING
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getUserHandle - Returns the scheduled meeting id
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid or schedId are invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom or scheduled meeting does not exists
     *
     * @param chatid  MegaChatHandle that identifies a chat room
     * @param schedId MegaChatHandle that identifies a scheduled meeting
     */
    public void removeScheduledMeeting(long chatid, long schedId) {
        megaChatApi.removeScheduledMeeting(chatid, schedId);
    }

    /**
     * Get a list of all scheduled meeting for a chatroom
     *
     * Important consideration:
     * A Chatroom only should have one root scheduled meeting associated, it means that for all scheduled meeting
     * returned by this method, just one should have an invalid parent sched Id (MegaChatScheduledMeeting::parentSchedId)
     *
     * You take the ownership of the returned value
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @return List of MegaChatScheduledMeeting objects for a chatroom.
     */
    public ArrayList<MegaChatScheduledMeeting> getScheduledMeetingsByChat(long chatid) {
        return chatScheduledMeetingListItemToArray(megaChatApi.getScheduledMeetingsByChat(chatid));
    }

    /**
     * Get a scheduled meeting given a chatid and a scheduled meeting id
     *
     * You take the ownership of the returned value
     *
     * @param chatid  MegaChatHandle that identifies a chat room
     * @param schedId MegaChatHandle that identifies a scheduled meeting
     * @return A MegaChatScheduledMeeting given a chatid and a scheduled meeting id
     */
    public MegaChatScheduledMeeting getScheduledMeeting(long chatid, long schedId) {
        return megaChatApi.getScheduledMeeting(chatid, schedId);
    }

    /**
     * Get a list of all scheduled meeting for all chatrooms
     *
     * Important consideration:
     * For every chatroom there should only exist one root scheduled meeting associated, it means that for all scheduled meeting
     * returned by this method, there should be just one scheduled meeting, with an invalid parent sched Id (MegaChatScheduledMeeting::parentSchedId),
     * for every different chatid.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatScheduledMeeting objects for all chatrooms.
     */
    public ArrayList<MegaChatScheduledMeeting> getAllScheduledMeetings() {
        return chatScheduledMeetingListItemToArray(megaChatApi.getAllScheduledMeetings());
    }

    /**
     * Get a list of all scheduled meeting occurrences for a chatroom
     *
     * A scheduled meetings occurrence, is a MegaChatCall that will happen in the future
     * A scheduled meeting can produce one or multiple scheduled meeting occurrences
     *
     * The associated request type with this request is MegaChatRequest::TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getMegaChatScheduledMeetingList - Returns a list of scheduled meeting occurrences
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid is invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists
     *
     * @param chatid MegaChatHandle that identifies a chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void fetchScheduledMeetingOccurrencesByChat(long chatid, MegaChatRequestListenerInterface listener) {
        megaChatApi.fetchScheduledMeetingOccurrencesByChat(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Get a list of all scheduled meeting occurrences for a chatroom
     *
     * A scheduled meetings occurrence, is a MegaChatCall that will happen in the future
     * A scheduled meeting can produce one or multiple scheduled meeting occurrences
     *
     * The associated request type with this request is MegaChatRequest::TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getMegaChatScheduledMeetingList - Returns a list of scheduled meeting occurrences
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - if chatid is invalid
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists
     *
     * @param chatid MegaChatHandle that identifies a chat room
     */
    public void fetchScheduledMeetingOccurrencesByChat(long chatid) {
        megaChatApi.fetchScheduledMeetingOccurrencesByChat(chatid);
    }

    /**
     * Check if there is an existing chat-link for an public chat
     *
     * This function allows moderators to check whether a public handle for public chats exist and,
     * if any, it returns a chat-link that any user can use to preview or join the chatroom.
     *
     * @see \c MegaChatApi::createPublicChat for more details.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHAT_LINK_HANDLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getNumRetry - Returns 0
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the chat-link for the chatroom, if it already exist
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_ACCESS - If the chat does not have topic.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void queryChatLink(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.queryChatLink(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Create a chat-link for a public chat
     *
     * This function allows moderators to create a public handle for public chats and returns
     * a chat-link that any user can use to preview or join the chatroom.
     *
     * @see \c MegaChatApi::createPublicChat for more details.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHAT_LINK_HANDLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns false
     * - MegaChatRequest::getNumRetry - Returns 1
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the chat-link for the chatroom
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_ACCESS - If the chat does not have topic.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void createChatLink(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.createChatLink(chatid, createDelegateRequestListener(listener));
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
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers
     * or the target is not actually contact of the user.
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ARGS - If the chat is not a group chat (cannot invite peers)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param userhandle MegaChatHandle that identifies the user
     * @param privs Privilege level for the new peers. Valid values are:
     * - MegaChatPeerList::PRIV_RO = 0
     * - MegaChatPeerList::PRIV_STANDARD = 2
     * - MegaChatPeerList::PRIV_MODERATOR = 3
     */
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
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have privileges to invite peers
     * or the target is not actually contact of the user.
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
     * Allow a user to add himself to an existing public chat. To do this the public chat must be in preview mode,
     * the result of a previous call to openChatPreview(), and the public handle contained in the chat-link must be still valid.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns invalid handle to identify that is an autojoin
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS  - If the chatroom is not groupal, public or is not in preview mode.
     * - MegaChatError::ERROR_NOENT - If the chat room does not exists, the chatid is not valid or the
     * public handle is not valid.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void autojoinPublicChat(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.autojoinPublicChat(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Allow a user to rejoin to an existing public chat. To do this the public chat
     * must have a valid public handle.
     *
     * This function must be called only after calling:
     * - MegaChatApi::openChatPreview and receive MegaChatError::ERROR_EXIST for a chatroom where
     * your own privilege is MegaChatRoom::PRIV_RM (You are trying to preview a public chat which
     * you were part of, so you have to rejoin it)
     *
     * The associated request type with this request is MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the public handle of the chat to identify that
     * is a rejoin
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If the chatroom is not groupal, the chatroom is not public
     * or the chatroom is in preview mode.
     * - MegaChatError::ERROR_NOENT - If the chatid is not valid, there isn't any chat with the specified
     * chatid or the chat doesn't have a valid public handle.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param ph MegaChatHandle that corresponds with the public handle of chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void autorejoinPublicChat(long chatid, long ph, MegaChatRequestListenerInterface listener){
        megaChatApi.autorejoinPublicChat(chatid, ph, createDelegateRequestListener(listener));
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
     * Enable/disable the visibility of when the logged-in user was online (green)
     *
     * If this option is disabled, the last-green won't be available for other users when it is
     * requested through MegaChatApi::requestLastGreen. The visibility is enabled by default.
     *
     * While this option is disabled and the user sets the green status temporary, the number of
     * minutes since last-green won't be updated. Once enabled back, the last-green will be the
     * last-green while the visibility was enabled (or updated if the user sets the green status).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_LAST_GREEN_VISIBLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag() - Returns true when attempt to enable visibility of last-green.
     *
     * @param enable True to enable the visibility of our last green
     * @param listener MegaChatRequestListener to track this request
     */
    public void setLastGreenVisible(boolean enable, MegaChatRequestListenerInterface listener){
        megaChatApi.setLastGreenVisible(enable, createDelegateRequestListener(listener));
    }

    /**
     * Request the number of minutes since the user was seen as green by last time.
     *
     * Apps may call this function to retrieve the minutes elapsed since the user was seen
     * as green (MegaChatApi::STATUS_ONLINE) by last time.
     * Apps must NOT call this function if the current status of the user is already green.
     *
     * The number of minutes since the user was seen as green by last time, if any, will
     * be notified in the MegaChatListener::onChatPresenceLastGreen callback. Note that,
     * if the user was never seen green by presenced or the user has disabled the visibility
     * of the last-green with MegaChatApi::setLastGreenVisible, there will be no notification
     * at all.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LAST_GREEN
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle() - Returns the handle of the user
     *
     * @param userid MegaChatHandle from user that last green has been requested
     * @param listener MegaChatRequestListener to track this request
     */
    public void requestLastGreen(long userid, MegaChatRequestListenerInterface listener){
        megaChatApi.requestLastGreen(userid, createDelegateRequestListener(listener));
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
     * Check if the online status is already confirmed by the server
     *
     * When a new online status is requested by MegaChatApi::setOnlineStatus, it's not
     * immediately set, but sent to server for confirmation. If the status is not confirmed
     * the requested online status will not be seen by other users yet.
     *
     * The apps may use this function to indicate the status is not confirmed somehow, like
     * with a slightly different icon, blinking or similar.
     *
     * @return True if the online status is confirmed by server
     */
    public boolean isOnlineStatusPending(){
        return megaChatApi.isOnlineStatusPending();
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
     * Allows to enable/disable the open invite option for a chat room
     *
     * The open invite option allows users with MegaChatRoom::PRIV_STANDARD privilege, to invite other users into the chat
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CHATROOM_OPTIONS
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of the chatroom
     * - MegaChatRequest::getPrivilege - Returns MegaChatApi::CHAT_OPTION_OPEN_INVITE
     * - MegaChatRequest::getFlag - Returns true if enabled was set true, otherwise it will return false
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ARGS - If the chatroom is a 1on1 chat
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_EXIST - If the value of enabled is the same as open invite option
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enabled True if we want to enable open invite option, otherwise false.
     * @param listener MegaChatRequestListener to track this request
     */
    public void setOpenInvite(long chatid, boolean enabled, MegaChatRequestListenerInterface listener){
        megaChatApi.setOpenInvite(chatid, enabled, createDelegateRequestListener(listener));
    }

    /**
     * Set the status of the app
     * <p>
     * Apps in mobile devices can be in different status. Typically, foreground and
     * background. The app should define its status in order to receive notifications
     * from server when the app is in background.
     * <p>
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
     * Returns the background status established in MEGAchat
     *
     * @return True if background status was set.
     */
    public int getBackgroundStatus(){
        return megaChatApi.getBackgroundStatus();
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
    public void getUserFirstname(long userhandle, String cauth, MegaChatRequestListenerInterface listener){
        megaChatApi.getUserFirstname(userhandle, cauth, createDelegateRequestListener(listener));
    }

    /**
     * Returns the current firstname of the user
     *
     * Returns NULL if data is not cached yet.
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose first name is requested.
     * @return The first name from user
     */
    public String getUserFirstnameFromCache(long userhandle) {
        return megaChatApi.getUserFirstnameFromCache(userhandle);
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
    public void getUserLastname(long userhandle, String cauth, MegaChatRequestListenerInterface listener){
        megaChatApi.getUserLastname(userhandle, cauth, createDelegateRequestListener(listener));
    }

    /**
     * Returns the current lastname of the user
     *
     * Returns NULL if data is not cached yet.
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose last name is requested.
     * @return The last name from user
     */
    public String getUserLastnameFromCache(long userhandle){
        return megaChatApi.getUserLastnameFromCache(userhandle);
    }

    /**
     * Returns the current email address of the contact
     *
     * This function is useful to get the email address of users you are NOT contact with.
     * Note that for any other user without contact relationship, this function will return NULL.
     *
     * You take the ownership of the returned value
     *
     * This function is useful to get the email address of users who participate in a groupchat with
     * you but are not your contacts.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_EMAIL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getUserHandle - Returns the handle of the user
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getText - Returns the email address of the user
     *
     * @param userhandle Handle of the user whose name is requested.
     * @param listener MegaChatRequestListener to track this request
     */
    public void getUserEmail(long userhandle, MegaChatRequestListenerInterface listener){
        megaChatApi.getUserEmail(userhandle, createDelegateRequestListener(listener));
    }

    /**
     * Returns the current email address of the user
     *
     * Returns NULL if data is not cached yet or it's not possible to get
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose email is requested.
     * @return The email from user
     */
    public String getUserEmailFromCache(long userhandle){
        return megaChatApi.getUserEmailFromCache(userhandle);
    }

    /**
     * Returns the current fullname of the user
     *
     * Returns NULL if data is not cached yet.
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose fullname is requested.
     * @return The full name from user
     */
    public String getUserFullnameFromCache(long userhandle){
        return megaChatApi.getUserFullnameFromCache(userhandle);
    }

    /**
     * Returns the known alias given to the user
     *
     * Returns NULL if data is not cached yet or it's not possible to get
     *
     * You take the ownership of returned value
     *
     * @param userhandle Handle of the user whose alias is requested.
     * @return The alias from user
     */
    public String getUserAliasFromCache(long userhandle) {
        return megaChatApi.getUserAliasFromCache(userhandle);
    }

    /**
     * Returns all the known aliases
     *
     * Returns NULL if data is not cached yet or it's not possible to get
     *
     * You take the ownership of returned value
     *
     * @return The list of aliases
     */
    public MegaStringMap getUserAliasesFromCache() {
        return megaChatApi.getUserAliasesFromCache();
    }

    /**
     * Request to server user attributes
     *
     * This function is useful to get the email address, first name, last name and full name
     * from chat link participants that they are not loaded
     *
     * After request is finished, you can call to MegaChatRoom::getPeerFirstnameByHandle,
     * MegaChatRoom::getPeerLastnameByHandle, MegaChatRoom::getPeerFullnameByHandle,
     * MegaChatRoom::getPeerEmailByHandle
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_PEER_ATTRIBUTES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of chat
     * - MegaChatRequest::getMegaHandleList - Returns the handles of user that attributes have been requested
     * - MegaChatRequest::getLink - Returns the authorization token. Previewers of chatlinks are not allowed
     * to retrieve user attributes like firstname or lastname, unless they provide a valid authorization token.
     *
     * @param chatid Handle of the chat whose member attributes requested
     * @param userList List of user whose attributes has been requested
     * @param listener MegaChatRequestListener to track this request
     */
    public void loadUserAttributes(long chatid, MegaHandleList userList, MegaChatRequestListenerInterface listener) {
        megaChatApi.loadUserAttributes(chatid, userList, createDelegateRequestListener(listener));
    }

    /**
     * Request to server user attributes
     *
     * This function is useful to get the email address, first name, last name and full name
     * from chat link participants that they are not loaded
     *
     * After request is finished, you can call to MegaChatRoom::getPeerFirstnameByHandle,
     * MegaChatRoom::getPeerLastnameByHandle, MegaChatRoom::getPeerFullnameByHandle,
     * MegaChatRoom::getPeerEmailByHandle
     *
     * The associated request type with this request is MegaChatRequest::TYPE_GET_PEER_ATTRIBUTES
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the handle of chat
     * - MegaChatRequest::getMegaHandleList - Returns the handles of user that attributes have been requested
     * - MegaChatRequest::getLink - Returns the authorization token. Previewers of chatlinks are not allowed
     * to retrieve user attributes like firstname or lastname, unless they provide a valid authorization token.
     *
     * @param chatid Handle of the chat whose member attributes requested
     * @param userList List of user whose attributes has been requested
     */
    public void loadUserAttributes(long chatid, MegaHandleList userList) {
        megaChatApi.loadUserAttributes(chatid, userList);
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
     * Returns the client id handle of the logged in user for a chatroom
     *
     * The clientId is not the same for all chatrooms. If \c chatid is invalid, this function
     * returns 0
     *
     * In offline mode (MegaChatApi::INIT_OFFLINE_SESSION), this function returns 0
     *
     * @return Own client id handle
     */
    public long getMyClientidHandle(long chatid){
        return megaChatApi.getMyClientidHandle(chatid);
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

    /**
     * @brief Get all chatrooms (1on1 and groupal) of this MEGA account
     *
     * It is needed to have successfully called MegaChatApi::init (the initialization
     * state should be MegaChatApi::INIT_OFFLINE_SESSION or MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatRoom objects with all chatrooms of this account.
     */
    public ArrayList<MegaChatRoom> getChatRooms() {
        return chatRoomListToArray(megaChatApi.getChatRooms());
    }

    /**
     * @brief Returns a list of chatrooms of this MEGA account filtered by type
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * @param type Type of the chatrooms returned by this method.
     * Valid values for param type are:
     * - MegaChatApi::CHAT_TYPE_ALL             = 0,  /// All chats types
     * - MegaChatApi::CHAT_TYPE_INDIVIDUAL      = 1,  /// 1on1 chats
     * - MegaChatApi::CHAT_TYPE_GROUP           = 2,  /// Group chats, public and private ones (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PRIVATE   = 3,  /// Private group chats (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PUBLIC    = 4,  /// Public group chats  (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_MEETING_ROOM    = 5,  /// Meeting rooms
     *
     * In case you provide an invalid value for type param, this method will returns an empty list
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatRoom objects filtered by type of this account.
     */
    public ArrayList<MegaChatRoom> getChatRoomsByType(int type) {
        return chatRoomListToArray(megaChatApi.getChatRoomsByType(type));
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
     * This function filters out archived chatrooms. You can retrieve them by using
     * the function \c getArchivedChatListItems.
     *
     * You take the ownership of the returned value
     *
     * @return List of MegaChatListItemList objects with all chatrooms of this account.
     */
    public ArrayList<MegaChatListItem> getChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getChatListItems());
    }

    /**
     * Get all chatrooms (1on1 and groupal) that contains a certain set of participants
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * This function returns even archived chatrooms.
     *
     * You take the ownership of the returned value
     *
     * @param peers MegaChatPeerList that contains the user handles of the chat participants,
     * except our own handle because MEGAchat doesn't include them in the map of members for each chatroom.
     *
     * @return List of MegaChatListItemList objects with the chatrooms that contains a certain set of participants.
     */
    public ArrayList<MegaChatListItem> getChatListItemsByPeers(MegaChatPeerList peers){
        return chatRoomListItemToArray(megaChatApi.getChatListItemsByPeers(peers));
    }

    /**
     * @brief Get all chatrooms (1on1 and groupal) with limited information filtered by type
     *
     * It is needed to have successfully called \c MegaChatApi::init (the initialization
     * state should be \c MegaChatApi::INIT_OFFLINE_SESSION or \c MegaChatApi::INIT_ONLINE_SESSION)
     * before calling this function.
     *
     * Note that MegaChatListItem objects don't include as much information as
     * MegaChatRoom objects, but a limited set of data that is usually displayed
     * at the list of chatrooms, like the title of the chat or the unread count.
     *
     * This function filters out archived chatrooms. You can retrieve them by using
     * the function \c getArchivedChatListItems.
     *
     * You take the ownership of the returned value
     *
     * @param type Type of the chatListItems returned by this method.
     * Valid values for param type are:
     * - MegaChatApi::CHAT_TYPE_ALL             = 0,  /// All chats types
     * - MegaChatApi::CHAT_TYPE_INDIVIDUAL      = 1,  /// 1on1 chats
     * - MegaChatApi::CHAT_TYPE_GROUP           = 2,  /// Group chats, public and private ones (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PRIVATE   = 3,  /// Private group chats (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_GROUP_PUBLIC    = 4,  /// Public group chats  (non meeting rooms)
     * - MegaChatApi::CHAT_TYPE_MEETING_ROOM    = 5,  /// Meeting rooms
     * - MegaChatApi::CHAT_TYPE_NON_MEETING     = 6,  /// Non meeting rooms (1on1 and groupchats public and private ones)
     *
     * In case you provide an invalid value for type param, this method will returns an empty list
     *
     * @return List of MegaChatListItemList objects with all chatrooms of this account filtered by type.
     */
    public ArrayList<MegaChatListItem> getChatListItemsByType(int type){
        return chatRoomListItemToArray(megaChatApi.getChatListItemsByType(type));
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
     *
     * Archived chatrooms with unread messages are not considered.
     *
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
     * Chatrooms became inactive when you left a groupchat or you are removed by
     * a moderator. 1on1 chats do not become inactive, just read-only.
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the active chatrooms
     */
    public ArrayList<MegaChatListItem> getInactiveChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getInactiveChatListItems());
    }

    /**
     * Return the archived chatrooms
     *
     * You take the onwership of the returned value.
     *
     * @return MegaChatListItemList including all the archived chatrooms
     */
    public ArrayList<MegaChatListItem> getArchivedChatListItems(){
        return chatRoomListItemToArray(megaChatApi.getArchivedChatListItems());
    }

    /**
     * Return the chatrooms that have unread messages
     *
     * Archived chatrooms with unread messages are not considered.
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
     * @brief Allows any user to preview a public chat without being a participant
     *
     * This function loads the required data to preview a public chat referenced by a
     * chat-link. It returns the actual \c chatid, the public handle, the number of peers
     * and also the title.
     *
     * If this request success, the caller can proceed as usual with
     * \c MegaChatApi::openChatRoom to preview the chatroom in read-only mode, followed by
     * a MegaChatApi::closeChatRoom as usual.
     *
     * The previewer may choose to join the public chat permanently, becoming a participant
     * with read-write privilege, by calling MegaChatApi::autojoinPublicChat.
     *
     * Instead, if the previewer is not interested in the chat anymore, it can remove it from
     * the list of chats by calling MegaChatApi::closeChatPreview.
     * @note If the previewer doesn't explicitely close the preview, it will be lost if the
     * app is closed. A preview of a chat is not persisted in cache.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_PREVIEW
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getLink - Returns the chat link.
     * - MegaChatRequest::getFlag - Returns true (openChatPreview)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If chatlink has not an appropiate format
     * - MegaChatError::ERROR_EXIST - If the chatroom already exists:
     *      + If the chatroom is in preview mode the user is trying to preview a public chat twice
     *      + If the chatroom is not in preview mode but is active, the user is trying to preview a
     *      chat which he is part of.
     *      + If the chatroom is not in preview mode but is inactive, the user is trying to preview a
     *      chat which he was part of. In this case the user will have to call MegaChatApi::autorejoinPublicChat to join
     *      to autojoin the chat again. Note that you won't be able to preview a public chat any more, once
     *      you have been part of the chat.
     * - MegaChatError::ERROR_NOENT - If the chatroom does not exists or the public handle is not valid.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK or MegaError::ERROR_EXIST:
     * - MegaChatRequest::getChatHandle - Returns the chatid of the chat.
     * - MegaChatRequest::getNumber - Returns the number of peers in the chat.
     * - MegaChatRequest::getText - Returns the title of the chat that was actually saved.
     * - MegaChatRequest::getUserHandle - Returns the public handle of chat.
     * - MegaChatRequest::getMegaHandleList - Returns a vector with one element (callid), if call doesn't exit it will be NULL
     * - MegaChatRequest::getParamType - Returns 1 if it's a meeting room
     *
     * On the onRequestFinish, when the error code is MegaError::ERROR_OK, you need to call
     * MegaChatApi::openChatRoom to receive notifications related to this chat
     *
     * @param link Null-terminated character string with the public chat link
     * @param listener MegaChatRequestListener to track this request
     */
    public void openChatPreview(String link, MegaChatRequestListenerInterface listener){
        megaChatApi.openChatPreview(link, createDelegateRequestListener(listener));
    }

    /**
     * @brief Allows any user to obtain basic information abouts a public chat if
     * a valid public handle exists.
     *
     * This function returns the actual \c chatid, the number of peers and also the title.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_LOAD_PREVIEW
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getLink - Returns the chat link.
     * - MegaChatRequest::getFlag - Returns false (checkChatLink)
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If chatlink has not an appropiate format
     * - MegaChatError::ERROR_NOENT - If the chatroom not exists or the public handle is not valid.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the chatid of the chat.
     * - MegaChatRequest::getNumber - Returns the number of peers in the chat.
     * - MegaChatRequest::getText - Returns the title of the chat that was actually saved.
     * - MegaChatRequest::getMegaHandleList - Returns a vector with one element (callid), if call doesn't exit it will be NULL
     * - MegaChatRequest::getParamType - Returns 1 if it's a meeting room
     *
     * @param link Null-terminated character string with the public chat link
     * @param listener MegaChatRequestListener to track this request
     */
    public void checkChatLink(String link, MegaChatRequestListenerInterface listener){
        megaChatApi.checkChatLink(link, createDelegateRequestListener(listener));
    }

    /**
     * Set the chat mode to private
     *
     * This function set the chat mode to private and invalidates the public handle if exists
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_PRIVATE_MODE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatId of the chat
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chat room does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_TOOMANY - If the chat is public and there are too many participants.
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getChatHandle - Returns the chatId of the chat
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void setPublicChatToPrivate(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.setPublicChatToPrivate(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Invalidates the currect public handle
     *
     * This function invalidates the currect public handle.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHAT_LINK_HANDLE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chatId of the chat
     * - MegaChatRequest::getFlag - Returns true
     * - MegaChatRequest::getNumRetry - Returns 0
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - If the chatroom is not groupal or public.
     * - MegaChatError::ERROR_NOENT  - If the chatroom does not exists or the chatid is invalid.
     * - MegaChatError::ERROR_ACCESS - If the caller is not an operator.
     * - MegaChatError::ERROR_ACCESS - If the chat does not have topic.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void removeChatLink(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.removeChatLink(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Allows to un/archive chats
     *
     * This is a per-chat and per-user option, and it's intended to be used when the user does
     * not care anymore about an specific chatroom. Archived chatrooms should be displayed in a
     * different section or alike, so it can be clearly identified as archived.
     *
     * Note you will stop receiving \c onChatListItemUpdate() updated for changes of type
     * MegaChatListItem::CHANGE_TYPE_UNREAD_COUNT, since the user is not anymore interested on
     * the activity of this chatroom.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ARCHIVE_CHATROOM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns if chat is to be archived or unarchived
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ENOENT - If the chatroom doesn't exists.
     * - MegaChatError::ERROR_ARGS - If chatid is invalid.he chat that was actually saved.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param archive True to set the chat as archived, false to unarchive it.
     * @param listener MegaChatRequestListener to track this request
     */
    public void archiveChat(long chatid, boolean archive, MegaChatRequestListenerInterface listener){
        megaChatApi.archiveChat(chatid, archive, createDelegateRequestListener(listener));
    }

    /**
     * This function allows a logged in operator/moderator to specify a message retention
     * timeframe in seconds, after which older messages in the chat are automatically deleted.
     * In order to disable the feature, the period of time can be set to zero (infinite).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_RETENTION_TIME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getNumber - Returns the retention timeframe in seconds
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If the chatid is invalid
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have operator privileges
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param period retention timeframe in seconds, after which older messages in the chat are automatically deleted
     * @param listener MegaChatRequestListener to track this request
     */
    public void setChatRetentionTime(long chatid, long period, MegaChatRequestListenerInterface listener) {
        megaChatApi.setChatRetentionTime(chatid, period, createDelegateRequestListener(listener));
    }

    /**
     * This function allows a logged in operator/moderator to specify a message retention
     * timeframe in seconds, after which older messages in the chat are automatically deleted.
     * In order to disable the feature, the period of time can be set to zero (infinite).
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_RETENTION_TIME
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getNumber - Returns the retention timeframe in seconds
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS - If the chatid is invalid
     * - MegaChatError::ERROR_NOENT - If there isn't any chat with the specified chatid.
     * - MegaChatError::ERROR_ACCESS - If the logged in user doesn't have operator privileges
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param period retention timeframe in seconds, after which older messages in the chat are automatically deleted
     */
    public void setChatRetentionTime(long chatid, long period) {
        megaChatApi.setChatRetentionTime(chatid, period);
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
                boolean success = listenerToDelete.invalidateUserListener();
                assert success; // failed if listener was already invalidated
                itr.remove();
                break;
            }
        }

        megaChatApi.closeChatRoom(chatid, listenerToDelete);
    }

    /**
     * This method should be called when we want to close a public chat preview
     *
     * It automatically disconnect to this chat, remove all internal data related, and make
     * a cache cleanup in order to clean all the related records.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     */
    public void closeChatPreview(long chatid){
        megaChatApi.closeChatPreview(chatid);
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
     * Checks whether the app has already loaded the full history of the chatroom
     *
     * @param chatid MegaChatHandle that identifies the chat room
     *
     * @return True the whole history is already loaded (including old messages from server).
     */
    public boolean isFullHistoryLoaded(long chatid) {
        return megaChatApi.isFullHistoryLoaded(chatid);
    }

    /**
     * Returns the MegaChatMessage specified from the chat room.
     *
     * This function allows to retrieve only those messages that are been loaded, received and/or
     * sent (confirmed and not yet confirmed). For any other message, this function
     * will return NULL.
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
     * Returns the MegaChatMessage specified from the chat room stored in node history
     *
     * This function allows to retrieve only those messages that are in the node history
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @return The MegaChatMessage object, or NULL if not found.
     */
    public MegaChatMessage getMessageFromNodeHistory(long chatid, long msgid){
        return megaChatApi.getMessageFromNodeHistory(chatid, msgid);
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
     * Sends a new giphy to the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
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
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param srcMp4 Source location of the mp4
     * @param srcWebp Source location of the webp
     * @param sizeMp4 Size in bytes of the mp4
     * @param sizeWebp Size in bytes of the webp
     * @param width Width of the giphy
     * @param height Height of the giphy
     * @param title Title of the giphy
     *
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    public MegaChatMessage sendGiphy(long chatid, String srcMp4, String srcWebp, long sizeMp4, long sizeWebp, int width, int height, String title) {
        return megaChatApi.sendGiphy(chatid, srcMp4, srcWebp, sizeMp4, sizeWebp, width, height, title);
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
     * Forward a message with attach contact
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
     * @param sourceChatid MegaChatHandle that identifies the chat room where the source message is
     * @param msgid MegaChatHandle that identifies the message that is going to be forwarded
     * @param targetChatId MegaChatHandle that identifies the chat room where the message is going to be forwarded
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    public MegaChatMessage forwardContact(long sourceChatid, long msgid, long targetChatId){
        return megaChatApi.forwardContact(sourceChatid, msgid, targetChatId);
    }

    /**
     * Share a geolocation in the specified chatroom
     *
     * The MegaChatMessage object returned by this function includes a message transaction id,
     * That id is not the definitive id, which will be assigned by the server. You can obtain the
     * temporal id with MegaChatMessage::getTempId
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
     * @param longitude from shared geolocation
     * @param latitude from shared geolocation
     * @param img Preview as a byte array encoded in Base64URL. It can be NULL
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    public MegaChatMessage sendGeolocation(long chatid, float longitude, float latitude, String img){
        return megaChatApi.sendGeolocation(chatid, longitude, latitude, img);
    }

    /**
     * Edit a geolocation message
     *
     * Message's edits are only allowed during a short timeframe, usually 1 hour.
     * Message's deletions are equivalent to message's edits, but with empty content.
     *
     * There is only one pending edit for not-yet confirmed edits. Therefore, this function will
     * discard previous edits that haven't been notified via MegaChatRoomListener::onMessageUpdate
     * where the message has MegaChatMessage::hasChanged(MegaChatMessage::CHANGE_TYPE_CONTENT).
     *
     * If the edit is rejected because the original message is too old, this function return NULL.
     *
     * When an already delivered message (MegaChatMessage::STATUS_DELIVERED) is edited, the status
     * of the message will change from STATUS_SENDING directly to STATUS_DELIVERED again, without
     * the transition through STATUS_SERVER_RECEIVED. In other words, the protocol doesn't allow
     * to know when an edit has been delivered to the target user, but only when the edit has been
     * received by the server, so for convenience the status of the original message is kept.
     * @note if MegaChatApi::isMessageReceptionConfirmationActive returns false, messages may never
     * reach the status delivered, since the target user will not send the required acknowledge to the
     * server upon reception.
     *
     * After this function, MegaChatApi::sendStopTypingNotification has to be called. To notify other clients
     * that it isn't typing
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param msgid MegaChatHandle that identifies the message
     * @param longitude from shared geolocation
     * @param latitude from shared geolocation
     * @param img Preview as a byte array encoded in Base64URL. It can be NULL
     * @return MegaChatMessage that will be sent. The message id is not definitive, but temporal.
     */
    public MegaChatMessage editGeolocation(long chatid, long msgid, float longitude, float latitude, String img) {
        return megaChatApi.editGeolocation(chatid, msgid, longitude, latitude, img);
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
     * Sends a node that contains a voice message to the specified chatroom
     *
     * The voice clip message includes information about the node, so the receiver can reproduce it online.
     *
     * In contrast to other functions to send messages, such as MegaChatApi::sendMessage or
     * MegaChatApi::attachContacts, this function is asynchronous and does not return a MegaChatMessage
     * directly. Instead, the MegaChatMessage can be obtained as a result of the corresponding MegaChatRequest.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the handle of the node
     * - MegaChatRequest::getParamType - Returns 1 (to identify the attachment as a voice message)
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
    public void attachVoiceMessage(long chatid, long nodehandle, MegaChatRequestListenerInterface listener){
        megaChatApi.attachVoiceMessage(chatid, nodehandle, createDelegateRequestListener(listener));
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

    /**
     * Notify MEGAchat a push has been received (in Android)
     *
     * This method should be called when the Android app receives a push notification.
     * As result, MEGAchat will retrieve from server the latest changes in the history
     * of every chatroom and will provide to the app the list of unread messages that
     * are suitable to create OS notifications.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_PUSH_RECEIVED
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Return if the push should beep (loud) or not (silent)     *
     * - MegaChatRequest::getChatHandle - Return MEGACHAT_INVALID_HANDLE
     * - MegaChatRequest::getParamType - Return 0
     *
     * @param beep True if push should generate a beep, false if it shouldn't.
     * @param listener MegaChatRequestListener to track this request
     */
    public void pushReceived(boolean beep, MegaChatRequestListenerInterface listener){
        megaChatApi.pushReceived(beep, createDelegateRequestListener(listener));
    }

    /**
     * Notify MEGAchat a push has been received (in Android)
     *
     * This method should be called when the Android app receives a push notification.
     * As result, MEGAchat will retrieve from server the latest changes in the history
     * of every chatroom and will provide to the app the list of unread messages that
     * are suitable to create OS notifications.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_PUSH_RECEIVED
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Return if the push should beep (loud) or not (silent)     *
     * - MegaChatRequest::getChatHandle - Return MEGACHAT_INVALID_HANDLE
     * - MegaChatRequest::getParamType - Return 0
     *
     * @param beep True if push should generate a beep, false if it shouldn't.
     */
    public void pushReceived(boolean beep){
        megaChatApi.pushReceived(beep);
    }

    /**
     * Notify MEGAchat a push has been received (in iOS)
     *
     * This method should be called when the iOS app receives a push notification.
     * As result, MEGAchat will retrieve from server the latest changes in the history
     * for one specific chatroom or for every chatroom.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_PUSH_RECEIVED
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Return if the push should beep (loud) or not (silent)
     * - MegaChatRequest::getChatHandle - Return the chatid to check for updates
     * - MegaChatRequest::getParamType - Return 1
     *
     * @param beep True if push should generate a beep, false if it shouldn't.
     * @param chatid MegaChatHandle that identifies the chat room, or MEGACHAT_INVALID_HANDLE for all chats
     * @param listener MegaChatRequestListener to track this request
     */
    public void pushReceived(boolean beep, long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.pushReceived(beep, chatid, createDelegateRequestListener(listener));
    }

    /**
     * Select the video device to be used in calls
     *
     * Video device identifiers are obtained with function MegaChatApi::getChatVideoInDevices
     *
     * The associated request type with this request is MegaChatRequest::TYPE_CHANGE_VIDEO_STREAM
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getText - Returns the device
     *
     * @param device Identifier of device to be selected
     * @param listener MegaChatRequestListener to track this request
     */
    public void setChatVideoInDevice(String device, MegaChatRequestListenerInterface listener) {
        megaChatApi.setChatVideoInDevice(device, createDelegateRequestListener(listener));
    }

    /**
     * Returns the video selected device name
     *
     * You take the ownership of the returned value
     *
     * @return Device selected name
     */
    public String getVideoDeviceSelected() {
        return megaChatApi.getVideoDeviceSelected();
    }

    // Call management
    /**
     * Start a call in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_START_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns value of param \c enableVideo
     * - MegaChatRequest::getParamType - Returns value of param \c enableAudio
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getFlag - Returns effective video flag (see note)
     *
     * The request will fail with MegaChatError::ERROR_ACCESS
     *  - if our own privilege is different than MegaChatPeerList::PRIV_STANDARD or MegaChatPeerList::PRIV_MODERATOR.
     *  - if peer of a 1on1 chatroom it's a non visible contact
     *  - if this function is called without being already connected to chatd.
     *  - if the chatroom is in preview mode.
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call and we can't join to it, or when the chat is public and there are too many participants
     * to start the call.
     *
     * The request will fail with MegaChatError::ERROR_EXISTS
     * - if there is a previous attempt still in progress (the call doesn't exist yet)
     * - if there is already another attempt to start a call for this chat, and call already exists but we don't participate
     * - if the call already exists and we already participate
     * In case that call already exists MegaChatRequest::getUserHandle will return its callid.
     *
     * The request will fail with MegaChatError::ERROR_NOENT
     * - if the chatroom doesn't exists.
     *
     * If the call has reached the maximum number of videos supported, the video-flag automatically be disabled.
     * @see MegaChatApi::getMaxVideoCallParticipants
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param enableAudio True for starting a call with audio (mute disabled)
     * @param listener MegaChatRequestListener to track this request
     */
    public void startChatCall(long chatid, boolean enableVideo, boolean enableAudio, MegaChatRequestListenerInterface listener) {
        megaChatApi.startChatCall(chatid, enableVideo, enableAudio, createDelegateRequestListener(listener));
    }

    /**
     * Start a call in a chatroom without ringing the participants (just for scheduled meeting context)
     *
     * When a scheduled meeting exists for a chatroom, and a call is started in that scheduled meeting context, it won't
     * ring the participants.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_START_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns value of param \c enableVideo
     * - MegaChatRequest::getParamType - Returns value of param \c enableAudio
     * - MegaChatRequest::getUserHandle() - Returns the scheduled meeting id;
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getFlag - Returns effective video flag (see note)
     *
     * The request will fail with MegaChatError::ERROR_ACCESS
     *  - if our own privilege is different than MegaChatPeerList::PRIV_STANDARD or MegaChatPeerList::PRIV_MODERATOR.
     *  - if peer of a 1on1 chatroom it's a non visible contact
     *  - if this function is called without being already connected to chatd.
     *  - if the chatroom is in preview mode.
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call and we can't join to it, or when the chat is public and there are too many participants
     * to start the call.
     *
     * The request will fail with MegaChatError::ERROR_EXISTS
     * - if there is a previous attempt still in progress (the call doesn't exist yet)
     * - if there is already another attempt to start a call for this chat, and call already exists but we don't participate
     * - if the call already exists and we already participate
     * In case that call already exists MegaChatRequest::getUserHandle will return its callid.
     *
     * The request will fail with MegaChatError::ERROR_NOENT
     * - if the chatroom doesn't exists.
     * - if the scheduled meeting doesn't exists
     *
     * If the call has reached the maximum number of videos supported, the video-flag automatically be disabled.
     * @see MegaChatApi::getMaxVideoCallParticipants
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param schedId MegaChatHandle scheduled meeting id that identifies the scheduled meeting context in which we will start the call
     * @param enableVideo True for audio-video call, false for audio call
     * @param enableAudio True for starting a call with audio (mute disabled)
     * @param listener MegaChatRequestListener to track this request
     */
    public void startChatCallNoRinging(long chatid, long schedId, boolean enableVideo, boolean enableAudio, MegaChatRequestListenerInterface listener) {
        megaChatApi.startChatCallNoRinging(chatid, schedId, enableVideo, enableAudio, createDelegateRequestListener(listener));
    }

    /**
     * Answer a call received in a chat room
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ANSWER_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns value of param \c enableVideo
     * - MegaChatRequest::getParamType - Returns value of param \c enableAudio
     *
     * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
     * is MegaError::ERROR_OK:
     * - MegaChatRequest::getFlag - Returns effective video flag (see note)
     *
     * The request will fail with MegaChatError::ERROR_ACCESS when this function is
     * called without being already connected to chatd.
     *
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call and we can't join to it, or when the chat is public and there are too many participants
     * to start the call.
     *
     * @note In case of group calls, if there is already too many peers sending video and there are no
     * available video slots, the request will NOT fail, but video-flag will automatically be disabled.
     *
     * To receive call notifications, the app needs to register MegaChatCallListener.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param enableVideo True for audio-video call, false for audio call
     * @param enableAudio True for answering a call with audio (mute disabled)
     * @param listener MegaChatRequestListener to track this request
     */
    public void answerChatCall(long chatid, boolean enableVideo, boolean enableAudio, MegaChatRequestListenerInterface listener) {
        megaChatApi.answerChatCall(chatid, enableVideo, enableAudio, createDelegateRequestListener(listener));
    }

    /**
     * Hang up a call
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the call identifier
     * - MegaChatRequest::getFlag - Returns false
     *
     * @param callid MegaChatHandle that identifies the call
     * @param listener MegaChatRequestListener to track this request
     */
    public void hangChatCall(long callid, MegaChatRequestListenerInterface listener) {
        megaChatApi.hangChatCall(callid, createDelegateRequestListener(listener));
    }

    /**
     * End a call in a chat room (user must be moderator)
     *
     * The associated request type with this request is MegaChatRequest::TYPE_HANG_CHAT_CALL
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the call identifier
     * - MegaChatRequest::getFlag - Returns true
     *
     * @param callid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void endChatCall(long callid, MegaChatRequestListenerInterface listener) {
        megaChatApi.endChatCall(callid, createDelegateRequestListener(listener));
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
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call sending audio already (no more audio slots are available).
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
     * The request will fail with MegaChatError::ERROR_TOOMANY when there are too many participants
     * in the call sending video already (no more video slots are available).
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
     * Request a high resolution quality level from a session
     *
     * Valid values for quality param are:
     *  + MegaChatCall::CALL_QUALITY_HIGH_DEF = 0,     // Default hi-res quality
     *  + MegaChatCall::CALL_QUALITY_HIGH_MEDIUM = 1,  // 2x lower resolution
     *  + MegaChatCall::CALL_QUALITY_HIGH_LOW = 2,     // 4x lower resolution
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIRES_QUALITY
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     * - MegaChatRequest::getParamType  - Returns the quality level requested
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client
     * @param quality The quality level requested
     * @param listener MegaChatRequestListener to track this request
     */
    public void requestHiResQuality(long chatid, long clientId, int quality, MegaChatRequestListenerInterface listener){
        megaChatApi.requestHiResQuality(chatid, clientId, quality, createDelegateRequestListener(listener));
    }

    /**
     * Remove an active speaker from the call
     *
     * This method can be called by the speaker itself (voluntary action) or by any moderator of the groupchat.
     *
     * The associated request type with this request is MegaChatRequest::TYPE_DEL_SPEAKER
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * On the onRequestFinish error, the error code associated to the MegaChatError can be:
     * - MegaChatError::ERROR_ARGS   - if specified chatid is invalid
     * - MegaChatError::ERROR_NOENT  - if there's no a call in the specified chatroom
     * - MegaChatError::ERROR_ACCESS - if clientId is not MEGACHAT_INVALID_HANDLE (own user),
     * and our own privilege is different than MegaChatPeerList::PRIV_MODERATOR
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies the client, or MEGACHAT_INVALID_HANDLE for own user
     * @param listener MegaChatRequestListener to track this request
     */
    public void removeSpeaker(long chatid, long clientId, MegaChatRequestListenerInterface listener){
        megaChatApi.removeSpeaker(chatid, clientId, createDelegateRequestListener(listener));
    }

    /**
     * Set/unset a call on hold
     *
     * The associated request type with this request is MegaChatRequest::TYPE_SET_CALL_ON_HOLD
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns true (set on hold) false (unset on hold)
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param setOnHold indicates if call is set or unset on hold
     * @param listener MegaChatRequestListener to track this request
     */
    public void setCallOnHold(long chatid, boolean setOnHold, MegaChatRequestListenerInterface listener) {
        megaChatApi.setCallOnHold(chatid, setOnHold, createDelegateRequestListener(listener));
    }

    /**
     * Open video device
     *
     * The associated request type with this request is MegaChatRequest::TYPE_OPEN_VIDEO_DEVICE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns true open device
     *
     * @note App is responsible to release device and remove MegaChatVideoListener
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void openVideoDevice(MegaChatRequestListenerInterface listener) {
        megaChatApi.openVideoDevice(createDelegateRequestListener(listener));
    }

    /**
     * Release video device
     *
     * The associated request type with this request is MegaChatRequest::TYPE_OPEN_VIDEO_DEVICE
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getFlag - Returns false close device
     *
     * @param listener MegaChatRequestListener to track this request
     */
    public void releaseVideoDevice(MegaChatRequestListenerInterface listener){
        megaChatApi.releaseVideoDevice(createDelegateRequestListener(listener));
    }

    /**
     * Get the MegaChatCall associated with a chatroom
     *
     * If \c chatid is invalid or there isn't any MegaChatCall associated with the chatroom,
     * this function returns NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return MegaChatCall object associated with chatid or NULL if it doesn't exist
     */
    public MegaChatCall getChatCall(long chatid){
        return megaChatApi.getChatCall(chatid);
    }

    /**
     * Mark as ignored the call associated with a chatroom
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return true if call can be marked as ignored, otherwise return false.
     */
    public boolean setIgnoredCall(long chatid) {
        return megaChatApi.setIgnoredCall(chatid);
    }

    /**
     * Get the MegaChatCall that has a specific id
     *
     * You can get the id of a MegaChatCall using MegaChatCall::getId().
     *
     * You take the ownership of the returned value.
     *
     * @param callId MegaChatHandle that identifies the call
     * @return MegaChatCall object for the specified \c callId. NULL if call doesn't exist
     */
    public MegaChatCall getChatCallByCallId(long callId){
        return megaChatApi.getChatCallByCallId(callId);

    }

    /**
     * Returns number of calls that are currently active
     * @note You may not participate in all those calls.
     * @return number of calls in the system
     */
    public int getNumCalls(){
        return megaChatApi.getNumCalls();
    }


    /**
     * Get a list with the ids of chatrooms where there are active calls
     *
     * The list of ids can be retrieved for calls in one specific state by setting
     * the parameter \c callState. If state is -1, it returns all calls regardless their state.
     *
     * You take the ownership of the returned value.
     *
     * @return A list of handles with the ids of chatrooms where there are active calls
     */
    public MegaHandleList getChatCalls() {
        return megaChatApi.getChatCalls(-1);
    }

    /**
     * Get a list with the ids of chatrooms where there are active calls
     *
     * The list of ids can be retrieved for calls in one specific state by setting
     * the parameter \c callState. If state is -1, it returns all calls regardless their state.
     *
     * You take the ownership of the returned value.
     *
     * @param state of calls that you want receive, -1 to consider all states
     * @return A list of handles with the ids of chatrooms where there are active calls
     */
    public MegaHandleList getChatCalls(int state) {
        return megaChatApi.getChatCalls(state);
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

    /**
     * Returns true if there is a call at chatroom with id \c chatid
     *
     * @note It's not necessary that we participate in the call, but other participants do.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return True if there is a call in a chatroom. False in other case
     */
    public boolean hasCallInChatRoom(long chatid){
        return megaChatApi.hasCallInChatRoom(chatid);
    }

    /**
     * Returns the maximum call participants
     *
     * @return Maximum call participants
     */
    public int getMaxCallParticipants() {
        return megaChatApi.getMaxCallParticipants();
    }

    /**
     * Returns the maximum video call participants
     *
     * @return Maximum video call participants
     */
    public int getMaxVideoCallParticipants() {
        return megaChatApi.getMaxVideoCallParticipants();
    }

    /**
     * Returns if audio level monitor is enabled
     *
     * It's false by default
     *
     * @note If there isn't a call in that chatroom in which user is participating,
     * audio Level monitor will be always false
     *
     * @param chatid MegaChatHandle that identifies the chat room from we want know if audio level monitor is disabled
     * @return true if audio level monitor is enabled
     */
    public boolean isAudioLevelMonitorEnabled(long chatid){
        return megaChatApi.isAudioLevelMonitorEnabled(chatid);
    }

    /**
     * Enable or disable audio level monitor
     *
     * It's false by default and it's app responsibility to enable it
     *
     * The associated request type with this request is MegaChatRequest::TYPE_ENABLE_AUDIO_LEVEL_MONITOR
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - Returns if enable or disable the audio level monitor
     *
     * @note If there isn't a call in that chatroom in which user is participating,
     * audio Level monitor won't be able established
     *
     * @param enable True for enable audio level monitor, False to disable
     * @param chatid MegaChatHandle that identifies the chat room where we can enable audio level monitor
     * @param listener MegaChatRequestListener to track this request
     */
    public void enableAudioLevelMonitor(boolean enable, long chatid, MegaChatRequestListenerInterface listener) {
        megaChatApi.enableAudioLevelMonitor(enable, chatid, createDelegateRequestListener(listener));
    }

    /**
     * Request become a speaker
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that it is a enable request operation
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void requestSpeak(long chatid, MegaChatRequestListenerInterface listener) {
        megaChatApi.requestSpeak(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Remove a request to become a speaker
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that it is a remove request operation
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatRequestListener to track this request
     */
    public void removeRequestSpeak(long chatid, MegaChatRequestListenerInterface listener){
        megaChatApi.removeRequestSpeak(chatid, createDelegateRequestListener(listener));
    }

    /**
     * Approve speak request
     *
     * This method has to be called only by a user with moderator role
     *
     * The associated request type with this request is MegaChatRequest::TYPE_APPROVE_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that approve the request
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * @note This functionality is ready but it shouldn't be used at this moment
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param listener MegaChatRequestListener to track this request
     */
    public void approveSpeakRequest(long chatid, long clientId, MegaChatRequestListenerInterface listener) {
        megaChatApi.approveSpeakRequest(chatid, clientId, createDelegateRequestListener(listener));
    }

    /**
     * Reject speak request
     *
     * This method has to be called only by a user with moderator role
     *
     * The associated request type with this request is MegaChatRequest::TYPE_APPROVE_SPEAK
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that reject the request
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param listener MegaChatRequestListener to track this request
     */
    public void rejectSpeakRequest(long chatid, long clientId, MegaChatRequestListenerInterface listener) {
        megaChatApi.rejectSpeakRequest(chatid, clientId, createDelegateRequestListener(listener));
    }

    /**
     * Request high resolution video from a client
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIGH_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that request high resolution video
     * - MegaChatRequest::getUserHandle - Returns the clientId of the user
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientId MegaChatHandle that identifies client
     * @param listener MegaChatRequestListener to track this request
     */
    public void requestHiResVideo(long chatid, long clientId, MegaChatRequestListenerInterface listener) {
        megaChatApi.requestHiResVideo(chatid, clientId, createDelegateRequestListener(listener));
    }

    /**
     * Stop high resolution video from a list of clients
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_HIGH_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that stop high resolution video
     * - MegaChatRequest::getMegaHandleList - Returns the list of clients Ids
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientIds List of clients Ids
     * @param listener MegaChatRequestListener to track this request
     */
    public void stopHiResVideo(long chatid, MegaHandleList clientIds, MegaChatRequestListenerInterface listener) {
        megaChatApi.stopHiResVideo(chatid, clientIds, createDelegateRequestListener(listener));
    }

    /**
     * Request low resolution video from a list of clients
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_LOW_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - true -> indicate that request low resolution video
     * - MegaChatRequest::getMegaHandleList - Returns the list of client Ids
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientIds List of clients Ids
     * @param listener MegaChatRequestListener to track this request
     */
    public void requestLowResVideo(long chatid, MegaHandleList clientIds, MegaChatRequestListenerInterface listener) {
        megaChatApi.requestLowResVideo(chatid, clientIds, createDelegateRequestListener(listener));
    }

    /**
     * Stop low resolution video from a list of clients
     *
     * The associated request type with this request is MegaChatRequest::TYPE_REQUEST_LOW_RES_VIDEO
     * Valid data in the MegaChatRequest object received on callbacks:
     * - MegaChatRequest::getChatHandle - Returns the chat identifier
     * - MegaChatRequest::getFlag - false -> indicate that stop low resolution video
     * - MegaChatRequest::getMegaHandleList - Returns the list of clients Ids
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param clientIds List of clients Ids
     * @param listener MegaChatRequestListener to track this request
     */
    public void stopLowResVideo(long chatid, MegaHandleList clientIds, MegaChatRequestListenerInterface listener) {
        megaChatApi.stopLowResVideo(chatid, clientIds, createDelegateRequestListener(listener));
    }

    public static void setCatchException(boolean enable) {
        MegaChatApi.setCatchException(enable);
    }

    /**
     * This method should be called when a node history is opened
     *
     * One node history only can be opened once before it will be closed
     * The same listener should be provided at MegaChatApi::closeChatRoom to unregister it
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatNodeHistoryListener to receive node history events. NULL is not allowed.
     *
     * @return True if success, false if listener is NULL or the chatroom is not found
     */
    public boolean openNodeHistory(long chatid, MegaChatNodeHistoryListenerInterface listener){
        return megaChatApi.openNodeHistory(chatid, createDelegateNodeHistoryListener(listener));
    }

    /**
     * @brief This method should be called when a node history is closed
     *
     * Note that this listener should be the one registered by MegaChatApi::openNodeHistory
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener MegaChatNodeHistoryListener to receive node history events. NULL is not allowed.
     *
     * @return True if success, false if listener is NULL or the chatroom is not found
     */
    public boolean closeNodeHistory(long chatid, MegaChatNodeHistoryListenerInterface listener){
        return megaChatApi.closeNodeHistory(chatid, createDelegateNodeHistoryListener(listener));
    }

    /**
     * Register a listener to receive all events about a specific node history
     *
     * You can use MegaChatApi::removeNodeHistoryListener to stop receiving events.
     *
     * Note this listener is feeded with data from a node history that is opened. It
     * is required to call \c MegaChatApi::openNodeHistory. Otherwise, the listener
     * will NOT receive any callback.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param listener Listener that will receive node history events
     */
    public void addNodeHistoryListener(long chatid, MegaChatNodeHistoryListenerInterface listener){
        megaChatApi.addNodeHistoryListener(chatid, createDelegateNodeHistoryListener(listener));
    }

    /**
     * Unregister a MegaChatNodeHistoryListener
     *
     * This listener won't receive more events.
     *
     * @param listener Object that is unregistered
     */

    public void removeNodeHistoryListener(long chatid, MegaChatNodeHistoryListenerInterface listener){
        ArrayList<DelegateMegaChatNodeHistoryListener> listenersToRemove = new ArrayList<DelegateMegaChatNodeHistoryListener>();
        synchronized (activeChatNodeHistoryListeners) {
            Iterator<DelegateMegaChatNodeHistoryListener> it = activeChatNodeHistoryListeners.iterator();
            while (it.hasNext()) {
                DelegateMegaChatNodeHistoryListener delegate = it.next();
                if (delegate.getUserListener() == listener) {
                    listenersToRemove.add(delegate);
                    it.remove();
                }
            }
        }

        for (int i=0;i<listenersToRemove.size();i++){
            megaChatApi.removeNodeHistoryListener(chatid, listenersToRemove.get(i));
        }
    }

    /**
     * Initiates fetching more node history of the specified chatroom.
     *
     * The loaded messages will be notified one by one through the MegaChatNodeHistoryListener
     * specified at MegaChatApi::openNodeHistory (and through any other listener you may have
     * registered by calling MegaChatApi::addNodeHistoryListener).
     *
     * The corresponding callback is MegaChatNodeHistoryListener::onAttachmentLoaded.
     *
     * Messages are always loaded and notified in strict order, from newest to oldest.
     *
     * @note The actual number of messages loaded can be less than \c count. Because
     * the history being shorter than requested. Additionally, if the fetch is local
     * and there's no more history locally available, the number of messages could be
     * lower too (and the next call to MegaChatApi::loadMessages will fetch messages from server).
     *
     * When there are no more history available from the reported source of messages
     * (local / remote), or when the requested \c count has been already loaded,
     * the callback  MegaChatNodeHistoryListener::onAttachmentLoaded will be called with a NULL message.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @param count The number of requested messages to load.
     *
     * @return Return the source of the messages that is going to be fetched. The possible values are:
     *   - MegaChatApi::SOURCE_ERROR = -1: we are not logged in yet
     *   - MegaChatApi::SOURCE_NONE = 0: there's no more history available (not even in the server)
     *   - MegaChatApi::SOURCE_LOCAL: messages will be fetched locally (RAM or DB)
     *   - MegaChatApi::SOURCE_REMOTE: messages will be requested to the server. Expect some delay
     *
     * The value MegaChatApi::SOURCE_REMOTE can be used to show a progress bar accordingly when network operation occurs.
     */
    public int loadAttachments(long chatid, int count){
        return megaChatApi. loadAttachments(chatid, count);
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

    private MegaChatScheduledMeetingListener createDelegateChatScheduledMeetingListener(MegaChatScheduledMeetingListenerInterface listener) {
        DelegateMegaChatScheduledMeetingListener delegateListener = new DelegateMegaChatScheduledMeetingListener(this, listener);
        activeChatScheduledMeetingListeners.add(delegateListener);
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

    private MegaChatNodeHistoryListener createDelegateNodeHistoryListener(MegaChatNodeHistoryListenerInterface listener) {
        DelegateMegaChatNodeHistoryListener delegateListener = new DelegateMegaChatNodeHistoryListener(this, listener);
        activeChatNodeHistoryListeners.add(delegateListener);
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

    static ArrayList<MegaChatScheduledMeeting> chatScheduledMeetingListItemToArray(MegaChatScheduledMeetingList chatScheduledMeetingList) {

        if (chatScheduledMeetingList == null) {
            return null;
        }

        ArrayList<MegaChatScheduledMeeting> result = new ArrayList<MegaChatScheduledMeeting>((int) chatScheduledMeetingList.size());
        for (int i = 0; i < chatScheduledMeetingList.size(); i++) {
            result.add(chatScheduledMeetingList.at(i).copy());
        }
        return result;
    }
};
