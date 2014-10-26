#include <webrtcAdapter.h> //must be before any Qt headers because Qt defines an 'emit' macro which conicides with a method name in webrtc, resulting in compile error
#include <talk/app/webrtc/test/fakeconstraints.h>
#include <streamPlayer.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>
#include "videoRenderer_Qt.h"

#undef emit
#define THROW_IF_FALSE(statement) \
    if (!(statement)) {\
    throw std::runtime_error("'" #statement "' failed (returned false)\n At " __FILE__ ":"+std::to_string(__LINE__)); \
    }

extern MainWindow* mainWin;
rtc::scoped_refptr<webrtc::MediaStreamInterface> localStream;
std::unique_ptr<artc::StreamPlayer> localPlayer;
class Session;

std::shared_ptr<Session> s1;
std::shared_ptr<Session> s2;

class Session
{
 public:
    artc::myPeerConnection<Session> pc;
    artc::StreamPlayer player;
    std::string id;
    Session(const std::string& aId, const webrtc::PeerConnectionInterface::IceServers& servers,
      IVideoRenderer* renderer, webrtc::MediaConstraintsInterface* options=NULL)
    :pc(servers, *this, options), player(renderer, NULL, NULL), id(aId),
    onAddStream([this](artc::tspMediaStream stream) mutable
    {
        const auto& vts = stream->GetVideoTracks();
        if (vts.size() > 0)
            player.attachVideo(vts[0]);
        const auto& ats = stream->GetAudioTracks();
        if (ats.size() > 0)
            player.attachAudio(ats[0]);
        player.start();
    }),
    onError([this]()
    {
        printf("%s: onError\n", id.c_str());
    }),
    onRemoveStream([this](artc::tspMediaStream stream)
    {
        printf("%s: onRemoveStream\n", id.c_str());
    }),
    onIceCandidate([this](std::shared_ptr<artc::IceCandText> candidate)
    {
        printf("%s: onIceCandidate\n", id.c_str());
    }),
    onIceComplete([this]()
    {
        printf("%s: onIceComplete\n", id.c_str());
    }),
    onSignalingChange([this](webrtc::PeerConnectionInterface::SignalingState newState)
    {
        printf("%s: onSignalingChange\n", id.c_str());
    }),
    onIceConnectionChange([this](webrtc::PeerConnectionInterface::IceConnectionState newState)
    {
        printf("%s: onIceConnectionChange\n", id.c_str());
    }),
    onRenegotiationNeeded([this]()
    {
        printf("%s: onRenegotiationNeeded\n", id.c_str());
    })
    {}

    std::function<void()> onError;
    std::function<void(artc::tspMediaStream stream)> onAddStream;
    std::function<void(artc::tspMediaStream stream)> onRemoveStream;
    std::function<void(std::shared_ptr<artc::IceCandText> candidate)>
    onIceCandidate;
    std::function<void()> onIceComplete;
    std::function<void(webrtc::PeerConnectionInterface::SignalingState newState)>
    onSignalingChange;
    std::function<void(webrtc::PeerConnectionInterface::IceConnectionState newState)>
    onIceConnectionChange;
    std::function<void()> onRenegotiationNeeded;
};


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    artc::init(NULL);
}

void MainWindow::buttonPushed()
{
    artc::DeviceManager devMgr;
    auto devices = devMgr.getInputDevices();
    for (auto& dev: devices->audio)
        printf("Audio: %s\n", dev.name.c_str());
    for (auto& dev: devices->video)
        printf("Video: %s\n", dev.name.c_str());
    if (devices->video.empty() || devices->audio.empty())
        throw std::runtime_error("No audio and/or video input present");
    artc::MediaGetOptions options(&(devices->video[0]));
    auto localVideo = devMgr.getUserVideo(options);
    auto localAudio = devMgr.getUserAudio(artc::MediaGetOptions(&(devices->audio[0])));
    localPlayer.reset(new artc::StreamPlayer(ui->localRenderer, localAudio, localVideo));
  //  localPlayer->start();
    webrtc::PeerConnectionInterface::IceServers servers;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:stun.l.google.com:19302";
    servers.push_back(server);
    webrtc::FakeConstraints pcmc;
    pcmc.AddOptional(::webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, true);
    s1.reset(new Session("s1", servers, ui->remoteRenderer1, &pcmc));
    s2.reset(new Session("s2", servers, ui->remoteRenderer2, &pcmc));
    localStream = artc::gWebrtcContext->CreateLocalMediaStream("localStream");
    if(!localStream.get())
        throw std::runtime_error("Could not create local stream");
    THROW_IF_FALSE((localStream->AddTrack(localAudio)));
    THROW_IF_FALSE((localStream->AddTrack(localVideo)));
    THROW_IF_FALSE((s1->pc.get()->AddStream(localStream, NULL)));
    THROW_IF_FALSE((s2->pc.get()->AddStream(localStream, NULL)));
    s1->onIceCandidate = [](std::shared_ptr<artc::IceCandText> cand)
    {
        THROW_IF_FALSE((s2.get()->pc->AddIceCandidate(cand->createObject())));
        printf("s1: onIceCandidate\n%s\n", cand->candidate.c_str());
    };
    s2->onIceCandidate = [](std::shared_ptr<artc::IceCandText> cand)
    {
        THROW_IF_FALSE((s1.get()->pc->AddIceCandidate(cand->createObject())));
        printf("%s: onInceCandidate\n", s2->id.c_str());
    };

    s1->pc.createOffer(NULL)
    .then([](webrtc::SessionDescriptionInterface* sdp)
    {
        printf("s1: createdOffer\n");
        artc::sspSdpText sdpText(new artc::SdpText(sdp));
        return s1->pc.setLocalDescription(sdp)
        .then([sdpText](int)
        {
            return sdpText;
        });
    })
    .then([](artc::sspSdpText sdpText)
    {
        printf("s1: setLocalDesc\n");
        return s2->pc.setRemoteDescription(sdpText->createObject());
    })
    .then([](int)
    {
        printf("s2: setRemoteDesc\n");
        return s2->pc.createAnswer(NULL);
    })
    .then([](webrtc::SessionDescriptionInterface* sdp)
    {
        printf("s2: created Answer\n");
        return when(s2->pc.setLocalDescription(sdp), s1->pc.setRemoteDescription(sdp));
    })
    .then([](int)
    {
        printf("s2: set local desc, s1: set remote desc\n");
        return 0;
    });
}

MainWindow::~MainWindow()
{
    artc::cleanup();
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
