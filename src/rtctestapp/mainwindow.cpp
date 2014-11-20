#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>
#include "videoRenderer_Qt.h"
#include "../base/guiCallMarshaller.h"
#include "../IRtcModule.h"

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;
extern karere::rtcModule::IRtcModule* rtc;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

void MainWindow::megaMessageSlot(void* msg)
{
    mega::processMessage(msg);
}

void MainWindow::buttonPushed()
{
    karere::rtcModule::AvFlags av;
    av.audio = true;
    av.video = true;
    char sid[karere::rtcModule::RTCM_SESSIONID_LEN+2];
    rtc->startMediaCall(sid, "alex1@j100.server.lu", av, nullptr);
}

MainWindow::~MainWindow()
{
    delete ui;
}
