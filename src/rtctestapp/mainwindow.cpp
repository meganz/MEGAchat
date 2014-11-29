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
extern rtcModule::IRtcModule* rtc;
extern char* peer;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

void MainWindow::megaMessageSlot(void* msg)
{
    megaProcessMessage(msg);
}
extern bool inCall;
void MainWindow::buttonPushed()
{
    if (inCall)
    {
        rtc->hangupAll("hangup", nullptr);
        inCall = false;
        ui->button->setText("Call");
    }
    else
    {
        rtcModule::AvFlags av;
        av.audio = true;
        av.video = true;
        char sid[rtcModule::RTCM_SESSIONID_LEN+2];
        rtc->startMediaCall(sid, peer, av, nullptr);
        inCall = true;
        ui->button->setText("Hangup");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

