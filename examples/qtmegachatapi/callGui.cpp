#include "callGui.h"
#include "ui_callGui.h"
#include "chatWindow.h"
#include "MainWindow.h"
#include <QPainter>
#include <IRtcStats.h>

using namespace std;
using namespace mega;
using namespace karere;

CallGui::CallGui(ChatWindow *parent, bool video)
    : QWidget(parent), mChatWindow(parent), ui(new Ui::CallGui)
{
    ui->setupUi(this);
    ui->localRenderer->setMirrored(true);
    connect(ui->mHupBtn, SIGNAL(clicked(bool)), this, SLOT(onHangCall(bool)));
    connect(ui->mShowChatBtn, SIGNAL(clicked(bool)), this, SLOT(onChatBtn(bool)));
    connect(ui->mMuteMicChk, SIGNAL(clicked(bool)), this, SLOT(onMuteMic(bool)));
    connect(ui->mMuteCamChk, SIGNAL(clicked(bool)), this, SLOT(onMuteCam(bool)));
    connect(ui->mFullScreenChk, SIGNAL(clicked(bool)), this, SLOT(onFullScreenChk(bool)));
    connect(ui->mAnswBtn, SIGNAL(clicked(bool)), this, SLOT(onAnswerCallBtn(bool)));
    setAvatarOnRemote();
    setAvatarOnLocal();
    ui->mFullScreenChk->hide();
    ui->localRenderer->enableStaticImage();
    ui->remoteRenderer->enableStaticImage();

    mVideo = video;
    if (!mVideo)
    {
        ui->mMuteCamChk->setChecked(true);
    }
    mCall = NULL;
    remoteCallListener = NULL;
    localCallListener = NULL;
}

void CallGui::connectCall()
{
    //remoteCallListener = new RemoteCallListener (mChatWindow->mMegaChatApi, callthis);
    localCallListener = new LocalCallListener (mChatWindow->mMegaChatApi, this);
    setCall(mChatWindow->mMegaChatApi->getChatCall(mChatWindow->mChatRoom->getChatId()));
    ui->mAnswBtn->hide();

    if(!mVideo)
    {
        mChatWindow->mMegaChatApi->disableVideo(mChatWindow->mChatRoom->getChatId());
        setAvatarOnLocal();
        ui->localRenderer->enableStaticImage();
    }
}

void CallGui::onAnswerCallBtn(bool)
{
    mChatWindow->mMegaChatApi->answerChatCall(mChatWindow->mChatRoom->getChatId(), true);
}

void CallGui::drawPeerAvatar(QImage &image)
{
    int nPeers = mChatWindow->mChatRoom->getPeerCount();
    megachat::MegaChatHandle peerHandle = megachat::MEGACHAT_INVALID_HANDLE;
    const char *title = NULL;
    for (int i = 0; i < nPeers; i++)
    {
        if (mChatWindow->mChatRoom->getPeerHandle(i) != mChatWindow->mMegaChatApi->getMyUserHandle())
        {
            peerHandle = mChatWindow->mChatRoom->getPeerHandle(i);
            title = mChatWindow->mChatRoom->getPeerFullname(i);
            break;
        }
    }    
    QChar letter = (std::strlen(title) == 0)
        ? QChar('?')
        : QChar(title[0]);

    drawAvatar(image, letter, peerHandle);
    delete [] title;
}

void CallGui::drawOwnAvatar(QImage &image)
{   
    const char *myName = mChatWindow->mMegaChatApi->getMyFirstname();
    QChar letter = (std::strlen(myName) == 0) ? QChar('?') : QString::fromStdString(myName)[0];
    drawAvatar(image, letter, mChatWindow->mMegaChatApi->getMyUserHandle());
    delete [] myName;
}

void CallGui::drawAvatar(QImage &image, QChar letter, uint64_t userid)
{
    uint64_t auxId = mChatWindow->mMegaChatApi->getMyUserHandle();
    image.fill(Qt::black);
    auto color = QColor("green");
    if (userid != auxId)
    {
        color = QColor("blue");
    }

    int cx = image.rect().width()/2;
    int cy = image.rect().height()/2;
    int w = image.width();
    int h = image.height();

    QPainter painter(&image);
    painter.setRenderHints(QPainter::TextAntialiasing|QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    painter.setBrush(QBrush(color));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, w, h, 6, 6, Qt::RelativeSize);
    painter.setBrush(QBrush(Qt::white));
    painter.drawEllipse(QPointF(cx, cy), (float)w/3.7, (float)h/3.7);

    QFont font("Helvetica", h/3.3);
    font.setWeight(QFont::Light);
    painter.setFont(font);
    painter.setPen(QPen(color));
    QFontMetrics metrics(font, &image);
    auto rect = metrics.boundingRect(letter);
    painter.drawText(cx-rect.width()/2,cy-rect.height()/2, rect.width(), rect.height()+2,
                     Qt::AlignHCenter|Qt::AlignVCenter, letter);
}

void CallGui::onHangCall(bool)
{
     mChatWindow->mMegaChatApi->hangChatCall(mChatWindow->mChatRoom->getChatId());
}

void CallGui::hangCall()
{
   mChatWindow->deleteCallGui();
}

CallGui:: ~CallGui()
{
    delete remoteCallListener;
    delete localCallListener;
    delete mCall;
    delete ui;
}

void CallGui::onMuteMic(bool checked)
{
    if (checked)
    {
        mChatWindow->mMegaChatApi->disableAudio(mChatWindow->mChatRoom->getChatId());
    }
    else
    {
        mChatWindow->mMegaChatApi->enableAudio(mChatWindow->mChatRoom->getChatId());
    }
}

void CallGui::onMuteCam(bool checked)
{
    if (checked)
    {
        mChatWindow->mMegaChatApi->disableVideo(mChatWindow->mChatRoom->getChatId());
        setAvatarOnLocal();
        ui->localRenderer->enableStaticImage();
    }
    else
    {
        mChatWindow->mMegaChatApi->enableVideo(mChatWindow->mChatRoom->getChatId());
        ui->localRenderer->disableStaticImage();
    }
}

void CallGui::onDestroy(rtcModule::TermCode code, bool byPeer, const std::string& text)
{
    mChatWindow->deleteCallGui();
}

void CallGui::onPeerMute(AvFlags state, AvFlags oldState)
{
    bool hasVideo = state.video();
    if (hasVideo == oldState.video())
    {
        return;
    }
    if (hasVideo)
    {
        ui->remoteRenderer->disableStaticImage();
    }
    else
    {
        ui->remoteRenderer->enableStaticImage();
    }
}

void CallGui::onVideoRecv()
{
    ui->remoteRenderer->disableStaticImage();
}

megachat::MegaChatCall *CallGui::getCall() const
{
    return mCall;
}

void CallGui::setCall(megachat::MegaChatCall *call)
{
    if (mCall)
    {
        delete mCall;
    }

    mCall = call;
}

void CallGui::setAvatarOnRemote()
{
    auto image = new QImage(QSize(262, 262), QImage::Format_ARGB32);
    drawPeerAvatar(*image);
    ui->remoteRenderer->setStaticImage(image);
}

void CallGui::setAvatarOnLocal()
{
    auto image = new QImage(QSize(160, 150), QImage::Format_ARGB32);
    drawOwnAvatar(*image);    
    ui->localRenderer->setStaticImage(image);
}

void CallGui::onChatBtn(bool)
{
    auto& txtChat = *mChatWindow->ui->mTextChatWidget;
    if (txtChat.isVisible())
        txtChat.hide();
    else
        txtChat.show();
}

void CallGui::onLocalStreamObtained(rtcModule::IVideoRenderer *& renderer)
{
    renderer = ui->localRenderer;
}

void CallGui::onRemoteStreamAdded(rtcModule::IVideoRenderer*& rendererRet)
{
    rendererRet = ui->remoteRenderer;
}

void CallGui::onLocalMediaError(const std::string err)
{
    KR_LOG_ERROR("=============LocalMediaFail: %s", err.c_str());
}
