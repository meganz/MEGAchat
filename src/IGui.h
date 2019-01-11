#ifndef IAPP_H
#define IAPP_H
#ifndef KARERE_DISABLE_WEBRTC
    #include <webrtc.h>
#endif
#include <chatd.h>
#include <presenced.h>
#include <autoHandle.h>

namespace karere
{
class ChatRoom;
class PeerChatRoom;
class GroupChatRoom;
class Contact;

/**
 * @brief The karere chat application class that the app needs to
 * implement in order to receive (mostly GUI) events.
 */
class IApp : public rtcModule::IGlobalHandler
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
         * to update the displayed contact/groupchat name. For contacts (and only there),
         * this string has a special layout - the first byte is the length of
         * the first name, then the first name follows, then the second name.
         * This allows passing taking apart the full name into first and second
         * name.
         */
        virtual void onTitleChanged(const std::string& title) = 0;

        /**
         * @brief The number of unread messages for that chat has changed. It can be used
         * to display an unread message counter next to the contact/groupchat
         * name.
         *
         * @param count If count == 0, then the indicator should be
         * removed, if count > 0, the indicator should show the exact value of
         * count, if count < 0, then there are *at least* \c count unread messages,
         * and possibly more. In that case the indicator should show e.g. '2+'
         */
        virtual void onUnreadCountChanged(int /*count*/) {}
    };

    /** @brief This interface must be implemented to receive events related to a chat.
     * It inherits chatd::Listener in order to receive chatd events,
     * and ITitleHandler, in order to receive chat title and online status change events
     */
    class IChatHandler: public chatd::Listener, public ITitleHandler
    {
    public:

        virtual ~IChatHandler() {}

#ifndef KARERE_DISABLE_WEBRTC
        /**
         * @brief Returns the ICallHandler instance associated with that chat, in
         * case there is an ongoing call. If there is no call,
         * NULL should be returned
         */
        virtual rtcModule::ICallHandler* callHandler() = 0;
#endif

        /** @brief Called when the name of a member changes
         * @param userid The member user handle
         * @param newName The new name. The first char of the name
         */
        virtual void onMemberNameChanged(uint64_t /*userid*/, const std::string& /*newName*/){}

        /** @brief Called when the chat is un/archived */
        virtual void onChatArchived(bool /*archived*/) {}

        /** @brief Returns an optionally associated user data pointer */
        void* userp = nullptr;
    };

    /** @brief
     * Implemented by contactlist and chat list items
     */
    class IListItem: public virtual ITitleHandler
    {
    public:
        virtual ~IListItem() {}

        /** @brief Returns a user data pointer */
        void* userp = nullptr;
    };

    /**
     * @brief The IChatListItem class represents an interface to a 1on1 or group
     * chat entry displayed in the application's chat list
     */
    class IChatListItem: public virtual IListItem
    {
    public:
        /**
         * @brief Returns whether the item represents a group chat or a 1on1 chat.
         * Based on that, it can be cast to \c IPeerChatListItem or
         * \c IGroupChatListItem
         */
        virtual bool isGroup() const = 0;

        /** @brief We were excluded from this chat - either because
         * we left, or because someone excluded us
         */
        virtual void onExcludedFromChat() {}

        /** @brief We were included in a chat again - we have the chat object,
         * so that means we were part of the chat before, hence we re-joined
         */
        virtual void onRejoinedChat() {}

        /** @brief The last message in the history sequence has changed.
         * This means that either a new message has been received, or the last
         * message of existing history was just fetched (this is the first message
         * received when fetching history, because it is fetched from newest to oldest).
         * @param msg Contains the properties of the last text message
         */
        virtual void onLastMessageUpdated(const chatd::LastTextMsg& /*msg*/) {}

        /** @brief Called when the timestamp of the most-recent message has changed.
         * This happens when a new message is received, or when there were no locally
         * known messages and the first old message is received
         */
        virtual void onLastTsUpdated(uint32_t /*ts*/) {}

        /** @brief Called when the connection state to the chatroom shard changes.
         */
        virtual void onChatOnlineState(const chatd::ChatState /*state*/) {}

        /** @brief Called when the chat is un/archived */
        virtual void onChatArchived(bool /*archived*/) {}
    };

    /**
     * @brief The IPeerChatListItem class is a specialization of IChatListItem for
     * 1on1 chat item
     */
    class IPeerChatListItem: public virtual IChatListItem
    {
    public:
        virtual bool isGroup() const { return false; }
    };

    /**
     * @brief The IPeerChatListItem class is a specialization of IChatListItem for
     * group chat item
     */
    class IGroupChatListItem: public virtual IChatListItem
    {
    public:
        virtual bool isGroup() const { return true; }

        /**
         * @brief Called when a user has joined the group chatroom
         * @param userid The handle of the user
         * @param priv The privilege of the joined user - look at chatd::Priv
         */
        virtual void onUserJoin(uint64_t /*userid*/, chatd::Priv /*priv*/) {}
        /** @brief User has left the chat.
         * @param userid - the user handle of the user who left the chat.
         */
        virtual void onUserLeave(uint64_t /*userid*/) {}
    };

    class IChatListHandler
    {
    public:
        virtual ~IChatListHandler() {}
        /**
         * @brief Called when a groupchat is added to the contactlist
         */
        virtual IGroupChatListItem* addGroupChatItem(GroupChatRoom& room) = 0;
        /**
         * @brief Called when a group chat needs to be added to the list
         */
        virtual void removeGroupChatItem(IGroupChatListItem& item) = 0;
        /**
         * @brief Called when a 1on1 chat needs to be added to the chatroom list
         */
        virtual IPeerChatListItem* addPeerChatItem(PeerChatRoom& room) = 0;
        /**
         * @brief Called when a 1on1 chat needs to be removed from the list
         */
        virtual void removePeerChatItem(IPeerChatListItem& item) = 0;
    };

    /** @brief Returns the interface to the chat list */
    virtual IChatListHandler* chatListHandler() = 0;

    /**
     * @brief Called when our own online status (presence) has changed.
     *
     * This can be used to update the indicator that shows the online status
     * of a contact/peer (online, offline, busy, away)
     *
     * @param userid User id whose presence has changed
     * @param pres The presence code
     * @param inProgress Whether the presence is being set or not
     */
    virtual void onPresenceChanged(Id /*userid*/, Presence /*pres*/, bool /*inProgress*/) {}

    /**
     * @brief Called when the presence preferences have changed due to
     * our or another client of our account updating them.
     * @param state - the new state of the preferences
     * @param pending - whether the preferences have actually been applied
     * on the server (\c false), or we have just sent them and they are not yet
     * confirmed by the server (\c true). When setAutoaway(), setPersist() or
     * setPresence() are called, a new presence config is sent to the server,
     * and this callback is immediately called with the new settings and \pending
     * set to true. When the server confirms the settings, this callback will
     * be called with the same config, but with \pending equal to \c false
     * Thus, the app can display a blinking online status when the user changes
     * it, until the server confirms it, at which point the status GUI will stop
     * blinking
     */
    virtual void onPresenceConfigChanged(const presenced::Config& config, bool pending) = 0;

    /**
     * @brief Called when client receives from presenced last time that a user has been green
     *
     * @note If the requested user has disabled the visibility of last-green or has never been green,
     * this callback will NOT be triggered at all.
     *
     * If the value of \c lastGreen is 65535 minutes (the maximum), apps should show "long time ago"
     * or similar, rather than the specific time period.
     *
     * @param userid User id whose last green is notified
     * @param lastGreen Time elapsed (minutes) since the last time user was green
     */
    virtual void onPresenceLastGreenUpdated(karere::Id userid, uint16_t lastGreen) = 0;

#ifndef KARERE_DISABLE_WEBRTC
    /**
     * @brief Called by karere when there is an incoming call.
     *
     * The app must create a rtcModule::ICallHandler to handle events related to
     * that call.
     * @param call The \c rtcModule::ICall instance that represents the call. To
     * answer, do `call.answer()`, to reject, do `call.hangup()`
     */
    virtual rtcModule::ICallHandler* onIncomingCall(rtcModule::ICall& call, karere::AvFlags av) = 0;
#endif

    /** @brief Called when the karere::Client changes its initialization or termination state.
     * Look at karere::Client::InitState for the possible values of the client init
     * state and their meaning.
     */
    virtual void onInitStateChange(int /*newState*/) {}

    virtual void onChatNotification(karere::Id /*chatid*/, const chatd::Message &/*msg*/, chatd::Message::Status /*status*/, chatd::Idx /*idx*/) {}

    virtual ~IApp() {}
};
}

#endif // IGUI

