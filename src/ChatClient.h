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
    std::unique_ptr<strophe::Connection> conn;
    ContactList contactList;
protected:
    std::unique_ptr<MyMegaApi> mApi;
    rtcModule::IPtr<rtcModule::IEventHandler> mRtcHandler;
public:
    rtcModule::IPtr<rtcModule::IRtcModule> mRtc;
    std::function<void()> onRtcInitialized;
    std::map<std::string, ChatRoom> chatRooms;
    Client(const std::string& email, const std::string& password,
           rtcModule::IEventHandler* rtcHandler);
    promise::Promise<int> start();
    void startSelfPings(int intervalSec=100);
};
}
#endif // CHATCLIENT_H
