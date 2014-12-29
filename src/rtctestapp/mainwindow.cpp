#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <string>
#include "videoRenderer_Qt.h"
#include "../base/gcm.h"
#include "../IRtcModule.h"
#include "../base/services-dns.hpp"
#include "../base/services-http.hpp"
#include <iostream>
#include <rapidjson/document.h>
#include <sdkApi.h>

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;
extern rtcModule::IRtcModule* rtc;
extern std::string peermail;
extern const std::string jidDomain;
extern std::unique_ptr<MyMegaApi> api;
using namespace std;
using namespace mega;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}
mega::http::Client* client = nullptr;

extern bool inCall;
void MainWindow::buttonPushed()
{
/*    if (!client)
        client = new mega::http::Client;
    client->get<std::string>("http://www.osnews.com/")
    .then([](std::shared_ptr<std::string> data)
    {
        cout << "response:" <<endl<<*data<<endl;
        return nullptr;
    })
    .fail([](const promise::Error& err)
    {
        return nullptr;
    });

return;
*/

    if (inCall)
    {
        rtc->hangupAll("hangup", nullptr);
    }
    else
    {
        api->call(&MegaApi::getUserData, peermail.c_str())
        .then([this](ReqResult result)
        {
            const char* peer = result->getText();
            if (!peer)
                throw std::runtime_error("Error getting peer's jid");
            string peerJid = string(peer)+"@developers.mega.co.nz";

            rtcModule::AvFlags av;
            av.audio = true;
            av.video = true;
            char sid[rtcModule::RTCM_SESSIONID_LEN+2];
            rtc->startMediaCall(sid, peerJid.c_str(), av, nullptr);
            inCall = true;
            ui->button->setText("Hangup");
            return nullptr;
        })
        .fail([](const promise::Error& err)
        {
            printf("Error calling user: %s\n", err.msg().c_str());
            return nullptr;
        });
    }

    /*
    mega::dnsLookup("google.com", 0)
    .then([](std::shared_ptr<mega::AddrInfo> result)
    {
        printf("Canonical name: %s\n", result->canonName().c_str());
        auto& ip4s = result->ip4addrs();
        for (auto& ip: ip4s)
            printf("ipv4: %s\n", ip.toString());
        auto& ip6s = result->ip6addrs();
        for (auto& ip: ip6s)
            printf("ipv6: %s\n", ip.toString());

        return nullptr;
    })
    .fail([](const promise::Error& err)
    {
        printf("DNS lookup error: %s\n", err.msg().c_str());
        return nullptr;
    });
    */
}

MainWindow::~MainWindow()
{
    delete ui;
}

