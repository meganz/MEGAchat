#ifndef IAPP_H
#define IAPP_H

namespace karere
{
class ChatRoom;
class GroupChatRoom;

/** @brief The karere chat application class that the app needs to
 * implement in order to receive (mostly GUI) events.
 */
class IApp
{
public:

    /**
     * @brief Interface that receives contact name updates and groupchat name updates.
     * Each contactlist item implements it, as well as each chat view
     */
    class ITitleHandler
    {
    public:

        virtual ~ITitleHandler() {}

        /**
         * @brief Called by karere when the title has changed. It can be used to e.g.
         * to update the displayed contact/groupchat name
         */
        virtual void onTitleChanged(const std::string& title) = 0;

        /** @brief
         * The number of unread messages for that chat has changed. It can be used
         * to display an unread message counter next to the contact/groupchat
         * name.
         *
         * If count == 0, then the indicator should be
         * removed, if count > 0, the indicator should show the exact value of
         * count, if count < 0, then there are *at least* \c count unread messages,
         * and possibly more. In that case the indicator should show e.g. '2+' */
        virtual void onUnreadCountChanged(int count) {}

        /** @brief
         * The online state of the person/chatroom has changed. This can be used
         * to update the indicator that shows the online status
         * of the contact/groupchat (online, offline, busy, etc)
         * @param state The presence code
         */
        virtual void onPresenceChanged(karere::Presence state) = 0;

        /** @brief
         * For group chats, tells the application that there has been a change
         * in the group composition. \c onTitleChanged() will be received as well,
         * so this event is not critical for name display updates
        */
        virtual void onMembersUpdated() {}
    };

    /** @brief
     * This is the interface that receives events about an ongoing call.
     *
     * As currently there are no additional methods besides the inherited from
     * \c  IEventHandler, the class is empty.
     */
    class ICallHandler: public rtcModule::IEventHandler
    {
    public:
        virtual ~ICallHandler() {}
    };

    /** @brief This interface must be implemented to receive events related to a chat.
     * It inherits chatd::Listener in order to receive chatd events,
     * and ITitleHandler, in order to receive chat title and online status change events
     */
    class IChatHandler: public chatd::Listener, public ITitleHandler
    {
    public:

        virtual ~IChatHandler() {}

        /**
         * @brief Returns the ICallHandler instance associated with that chat, in
         * case there is an ongoing call. If there is no call,
         * NULL should be returned
         */
        virtual ICallHandler* callHandler() = 0;

        /** @brief Returns an optionally associated user data pointer */
        virtual void* userp() { return nullptr; }
    };

    /** @brief
     * This is the interafce that must be implemented by the
     * login dialog implementation, in case the app uses karere to login the
     * SDK instance via \c karere::Client::sdkLoginNewSession()
     *
     * If that method is never used, then the application does not need to
     * implement this interface, and can return NULL from
     * \c karere::IGui::createLoginDialog()
     */
    class ILoginDialog
    {
    public:

        enum LoginStage {
            kAuthenticating,
            kBadCredentials,
            kLoggingIn,
            kFetchingNodes,
            kLoginComplete,
            kLast=kLoginComplete
        };

        /**
         * @brief This is the method that karere calls when it needs the dialog shown
         * and credentials entered. It should return the username and password
         * via the returned promise
         */
        virtual promise::Promise<std::pair<std::string, std::string>> requestCredentials() = 0;
        /** @brief
         * Called when the state of the login operation changes,
         * to inform the user about the progress of the login operation.
         */
        virtual void setState(LoginStage state) {}
        virtual ~ILoginDialog() {}
    };
    /** @brief
     * Called when karere needs to create a login dialog.
     *
     * This is only needed if the app uses karere to log in the SDK instance,
     * by calling \c karere::Client::sdkLoginNewSession().
     * This method can just return NULL if the app never calls
     * \c karere::Client::sdkLoginNewSession()
     */
    virtual ILoginDialog* createLoginDialog() = 0;

    /** @brief
     * Called when karere needs to instantiate a chat window for that 1on1 or
     * group chatroom
     * @param room The chat room object.
     */
    virtual IChatHandler* createChatHandler(karere::ChatRoom& room) = 0;

    /** @brief
     * Implemented by contactlist items, including groupchat items
     */
    class IContactListItem: public ITitleHandler
    {
    public:

        virtual ~IContactListItem() {}

        /** @brief Called when the contact's visibility has changed, i.e. the
         * contact was removed or added. Used only for contacts (not groupchats).
         *
         * The contact itself is never removed from the contactlist,
         * because it is hard-linked with its chatroom, which must exist
         * even if the contact is removed (for viewing the history in read-only
         * mode)
         * @param newVisibility The new visibility code, as defined in the Mega SDK
         * class mega::MegaUser
         */
        virtual void onVisibilityChanged(int newVisibility) = 0;

        /** @brief Returns a user data pointer */
        void* userp() { return nullptr; }
    };

    /** @brief Manages contactlist items that in turn receive events
      *
      * Note that both contacts and group chatrooms are considered contactlist
      * items. However the app may choose to present them separately to the user,
      * i.e. a contact list and a chat list view. In that case, a contact's 1on1
      * chatroom can be obtained by
      * \c PeerChatRoom* karere::Contact::chatRoom()
      * which will return NULL in case there is no existing chat with that contact.
      */
    class IContactListHandler
    {
    public:
        virtual ~IContactListHandler() {}

        /**
         * @brief Called when a contact is added to the contactlist
         *
         * @param contact
         * @return
         */
        virtual IContactListItem* addContactItem(Contact& contact) = 0;
        /** @brief Called when a groupchat is added to the contactlist */
        virtual IContactListItem* addGroupChatItem(GroupChatRoom& room) = 0;
        /** @brief Called when a contact is removed from contactlist */
        virtual void removeContactItem(IContactListItem* item) = 0;
        /** @brief Called when a groupchat is removed from the contactlist */
        virtual void removeGroupChatItem(IContactListItem* item) = 0;

        /** @brief Must return the 1on1 IChatHandler instance for the specified
         * user handle. If one does not exist, it must be created and returned
         */
        virtual IChatHandler& chatHandlerForPeer(uint64_t handle) = 0;
    };

    /** @brief Returns the interface to the contactlist */
    virtual IContactListHandler& contactListHandler() = 0;

    /** @brief Called by karere when our own online state/presence has changed. */
    virtual void onOwnPresence(Presence pres) {} //may include flags

    /** @brief Called when an incoming contact request has been received.
     *
     *  To accept or decline the request, the GUI should call
     * \c mega::MegaApi::replyContactRequest() with the \c req object
     * @param req The mega SDK contact request object
     */
    virtual void onIncomingContactRequest(const mega::MegaContactRequest& req) = 0;

    /** @brief Called by karere when there is an incoming call.
     *
     * The app must create a rtcModule::IEventHandler to handle events related to
     * that incoming call request (such as cancel or timeout of the call request).
     * Normally this rtcModule::IEventHandler instance is a class that displays
     * an incoming call GUI, that has a shared pointer to the rtcModule::ICallAnswer
     * object that is used to answer or reject the call.
     * @param ans The \c rtcModule::ICallAnswer object that is used to answer or
     * reject the call
     */
    virtual rtcModule::IEventHandler*
        onIncomingCall(const std::shared_ptr<rtcModule::ICallAnswer>& ans) = 0;

    /** @brief Called by karere when we become participants
     * in a 1on1 or a group chat.
     */
    virtual void notifyInvited(const ChatRoom& room) {}

    /** @brief Called when karere init is complete and the app can show it  UI */
    virtual void onInitComplete() = 0;

    /** @brief Called when karere is about to terminate */
    virtual void onTerminate() {}
    virtual ~IApp() {}
};
}

#endif // IGUI

