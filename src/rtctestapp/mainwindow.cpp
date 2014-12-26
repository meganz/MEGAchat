#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>
#include "videoRenderer_Qt.h"
#include "../base/gcm.h"
#include "../IRtcModule.h"
#include "../base/services-dns.hpp"
#include "../base/services-http.hpp"
#include <iostream>
#include <mega/json.h>
#include <mega/utils.h>

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;
extern rtcModule::IRtcModule* rtc;
extern std::string peer;

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
    mega::JSON json;
    json.begin("{\"url\":\"http://example.com:1234/?udp=1\", \"user\":\"testuser\", \"pass\":\"testpass\"}");
    json.enterobject();
    mega::nameid name;
    while ((name = json.getnameid()) != EOO)
    {
        if (name == MAKENAMEID3('u', 'r', 'l'))
            printf("url = %s\n", json.getvalue());
        else if (name == MAKENAMEID4('u','s','e','r'))
            printf("user = %s\n", json.getvalue());
        else if (name == MAKENAMEID4('p','a','s','s'))
            printf("pass = %s\n", json.getvalue());
        else
            printf("UNKNOWN nameid\n");
    }
    if (inCall)
    {
        rtc->hangupAll("hangup", nullptr);
    }
    else
    {
        rtcModule::AvFlags av;
        av.audio = true;
        av.video = true;
        char sid[rtcModule::RTCM_SESSIONID_LEN+2];
        rtc->startMediaCall(sid, peer.c_str(), av, nullptr);
        inCall = true;
        ui->button->setText("Hangup");
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

