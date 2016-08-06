#ifndef IGUI_H
#define IGUI_H

namespace karere
{
class ChatRoom;
class GroupChatRoom;

class IGui
{
public:
    class ITitleDisplay
    {
    public:
        virtual void updateTitle(const std::string& title) = 0;
        virtual void updateOverlayCount(int count) {}
        virtual void updateOnlineIndication(karere::Presence state) = 0;
        virtual void onMembersUpdated() {} //Used only for group chats
    };
    class ICallGui{};
    class IChatWindow: public chatd::Listener, public ITitleDisplay
    {
    public:
        virtual ICallGui* callGui() = 0;
        virtual rtcModule::IEventHandler* callEventHandler() = 0;
        virtual void show() = 0;
        virtual void hide() = 0;
    };
    class ILoginDialog
    {
    public:
        enum LoginStage { kAuthenticating, kBadCredentials, kLoggingIn, kFetchingNodes, kLoginComplete, kLast=kLoginComplete};
        virtual promise::Promise<std::pair<std::string, std::string>> requestCredentials() = 0;
        virtual void setState(LoginStage state) {}
        virtual ~ILoginDialog() {}
    };
    virtual ILoginDialog* createLoginDialog() = 0;

    virtual IChatWindow* createChatWindow(karere::ChatRoom& room) = 0;
    class IContactGui: public ITitleDisplay
    {
    public:
        virtual void showChatWindow() = 0;
        virtual void onVisibilityChanged(int newVisibility) = 0;
    };
    class IContactList
    {
    public:
        virtual IContactGui* createContactItem(Contact& contact) = 0;
        virtual IContactGui* createGroupChatItem(GroupChatRoom& room) = 0;
        virtual void removeContactItem(IContactGui* item) = 0;
        virtual void removeGroupChatItem(IContactGui* item) = 0;
        virtual IChatWindow& chatWindowForPeer(uint64_t handle) = 0;
    };
    virtual IContactList& contactList() = 0;
    virtual void onOwnPresence(Presence pres) {} //may include flags
    virtual void onIncomingContactRequest(const mega::MegaContactRequest& req) = 0;
    virtual rtcModule::IEventHandler*
        createCallAnswerGui(const std::shared_ptr<rtcModule::ICallAnswer>& ans) = 0;
    virtual void notifyInvited(const ChatRoom& room) {}
    virtual void show() = 0;
    virtual bool visible() const = 0;
    virtual void onTerminate() {}
    virtual ~IGui() {}
};
}

#endif // IGUI

