#include "webrtcAdapter.h" //must be before any Qt headers because Qt defines an 'emit' macro which conicides with a method name in webrtc, resulting in compile error
#include <talk/app/webrtc/test/fakeconstraints.h>
#include <streamPlayer.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>
#include "videoRenderer_Qt.h"

#undef emit

extern MainWindow* mainWin;
talk_base::scoped_refptr<webrtc::MediaStreamInterface> localStream;
std::unique_ptr<rtcModule::StreamPlayer> localPlayer;

class PcHandler
{
 public:
    void onError()
    {
        printf("onError\n");
    }
    void onAddStream(talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream)
    {
        printf("onAddStream\n");
    }
    void onRemoveStream(talk_base::scoped_refptr<webrtc::MediaStreamInterface> spStream)
    {
        printf("onRemoveStream\n");
    }
    void onIceCandidate(const std::string& candidate)
    {
        printf("onIceCandidate\n");
    }
    void onIceComplete()
    {
        printf("onIceComplete\n");
    }
    void onSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
    {
        printf("onSignalingChange\n");
    }
    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
    {
        printf("onIceConnectionChange\n");
    }
    void onRenegotiationNeeded()
    {
        printf("onRenegotiationNeeded\n");
    }
};
PcHandler handler;

std::shared_ptr<rtcModule::myPeerConnection<PcHandler> > pc1;
std::shared_ptr<rtcModule::myPeerConnection<PcHandler> > pc2;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    rtcModule::init(NULL);
}

void MainWindow::buttonPushed()
{
    rtcModule::DeviceManager devMgr;
    auto devices = rtcModule::getInputDevices(devMgr);
    for (auto& dev: devices->audio)
        printf("Audio: %s\n", dev.name.c_str());
    for (auto& dev: devices->video)
        printf("Video: %s\n", dev.name.c_str());
    rtcModule::MediaGetOptions options(&(devices->video[0]));
    auto localVideo = rtcModule::getUserVideo(options, devMgr);
    auto localAudio = rtcModule::getUserAudio(rtcModule::MediaGetOptions(&(devices->audio[0])), devMgr);
    localPlayer.reset(new rtcModule::StreamPlayer(localAudio, localVideo, ui->localRenderer));
    localPlayer->start();
    webrtc::PeerConnectionInterface::IceServers servers;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:stun.l.google.com:19302";
    servers.push_back(server);
    webrtc::FakeConstraints pcmc;
    pcmc.AddOptional(::webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);
    pc1.reset(new rtcModule::myPeerConnection<PcHandler>(servers, handler, NULL));
    pc2.reset(new rtcModule::myPeerConnection<PcHandler>(servers, handler, NULL));
    localStream = rtcModule::gWebrtcContext->CreateLocalMediaStream("localStream");
    if(!localStream.get())
        throw std::runtime_error("Could not create local stream");
    localStream->AddTrack(localAudio);
    localStream->AddTrack(localVideo);
    pc1->get()->AddStream(localStream, NULL);
    pc2->get()->AddStream(localStream, NULL);
  //  return;
    pc1->createOffer(NULL)
    .then([](webrtc::SessionDescriptionInterface* sdp)
    {
        printf("createdOffer\n");
        return pc1->setLocalDescription(sdp);
    })
    .then([](rtcModule::sspSdpText sdpText)
    {
        printf("setLocalDesc\n");
        return pc2->setRemoteDescription(rtcModule::parseSdp(sdpText));
    })
    .then([](rtcModule::sspSdpText sdp)
    {
        printf("setRemoteDesc\n");
        return pc2->createAnswer(NULL);
    })
    .then([](webrtc::SessionDescriptionInterface* sdp)
    {
        printf("created Answer\n");
        return pc1->setRemoteDescription(sdp);
    })
    .then([](rtcModule::sspSdpText sdp)
    {
        printf("set remote desc\n");
        return 0;
    });
}

MainWindow::~MainWindow()
{
    rtcModule::cleanup();
    delete ui;
}

void MainWindow::megaMessageSlot(void* msg)
{
    mega::processMessage(msg);
}
struct MyThread: public QThread
{
    virtual void run()
    {
        QMetaObject::invokeMethod(mainWin,
          "slotKarereEvent", Qt::QueuedConnection,
          Q_ARG(void*, (void*)"this is a test message"));
    }
};
/*
void MainWindow::buttonPushed()
{
    QMessageBox::information(this, "",
        ("button pushed, thread = "+std::to_string(QThread::currentThreadId())).c_str());
    MyThread thread;
    thread.run();
}
*/
