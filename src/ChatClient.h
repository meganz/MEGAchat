#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include "ContactList.h"
#include "ChatRoom.h"
#include "ITypes.h" //for IPtr

namespace strophe {class Connection;}
namespace rtcModule
{
    class IRtcModule;
    class IEventHandler;
};
class MyMegaApi;

namespace karere
{
class Client
{
protected:
    std::string mEmail;
    std::string mPassword;
public:
    std::shared_ptr<strophe::Connection> conn;
    ContactList contactList;
protected:
    rtcModule::IPtr<rtcModule::IEventHandler> mRtcHandler;
public:
    std::unique_ptr<MyMegaApi> api;
    rtcModule::IPtr<rtcModule::IRtcModule> rtc;
    std::function<void()> onRtcInitialized;
    std::map<std::string, ChatRoom> chatRooms;
    Client(const std::string& email, const std::string& password,
           rtcModule::IEventHandler* rtcHandler);
    promise::Promise<int> start();
    void startSelfPings(int intervalSec=100);
};
}
#endif // CHATCLIENT_H
