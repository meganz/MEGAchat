#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>
#include "videoRenderer_Qt.h"
#include "../base/gcm.h"
#include "../IRtcModule.h"
#include "../base/services-dns.hpp"

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;
extern rtcModule::IRtcModule* rtc;
extern std::string peer;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

extern bool inCall;
void MainWindow::buttonPushed()
{
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
    mega::dnsLookup("google.com")
    .then([](std::shared_ptr<mega::DnsResult> result)
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
}

MainWindow::~MainWindow()
{
    delete ui;
}

