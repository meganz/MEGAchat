#include "callGui.h"
#include "ui_callGui.h"
#include "chatWindow.h"
#include "MainWindow.h"
#include <QPainter>
#include <IRtcStats.h>

using namespace std;
using namespace mega;
using namespace karere;

CallGui::CallGui(ChatWindow *parent, bool video, MegaChatHandle peerid, bool local)
    : QWidget(parent), mChatWindow(parent), ui(new Ui::CallGui)
{
    ui->setupUi(this);
    mPeerid = peerid;
    connect(ui->mHupBtn, SIGNAL(clicked(bool)), this, SLOT(onHangCall(bool)));
    connect(ui->mShowChatBtn, SIGNAL(clicked(bool)), this, SLOT(onChatBtn(bool)));
    connect(ui->mMuteMicChk, SIGNAL(clicked(bool)), this, SLOT(onMuteMic(bool)));
    connect(ui->mMuteCamChk, SIGNAL(clicked(bool)), this, SLOT(onMuteCam(bool)));
    connect(ui->mAnswBtn, SIGNAL(clicked(bool)), this, SLOT(onAnswerCallBtn(bool)));
    localCallListener = NULL;
    remoteCallListener = NULL;
    mLocal = local;
    mVideo = video;
    mCall = NULL;

    setAvatar();
    ui->videoRenderer->enableStaticImage();

    if (mPeerid == mChatWindow->mMegaChatApi->getMyUserHandle())
    {
        ui->videoRenderer->setMirrored(true);
        ui->mFullScreenChk->hide();
        if (!mVideo)
        {
            ui->mMuteCamChk->setChecked(true);
        }
    }
    else
    {
        ui->mAnswBtn->hide();
        ui->mFullScreenChk->hide();
        ui->mHupBtn->hide();
        ui->mMuteCamChk->hide();
        ui->mMuteMicChk->hide();
        ui->mShowChatBtn->hide();
    }
}

void CallGui::connectPeerCallGui()
{
    MegaChatCall *auxCall = mChatWindow->mMegaChatApi->getChatCall(mChatWindow->mChatRoom->getChatId());
    setCall(auxCall);
    if (mPeerid == mChatWindow->mMegaChatApi->getMyUserHandle())
    {
        localCallListener = new LocalCallListener (mChatWindow->mMegaChatApi, this);
        ui->mAnswBtn->hide();
        if (!mVideo)
        {
            mChatWindow->mMegaChatApi->disableVideo(mChatWindow->mChatRoom->getChatId());
            setAvatar();
            ui->videoRenderer->enableStaticImage();
        }
    }
    else
    {
        remoteCallListener = new RemoteCallListener (mChatWindow->mMegaChatApi, this, mPeerid);
    }
}

MegaChatHandle CallGui::getPeer()
{
    return mPeerid;
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
    QChar letter = (!title || std::strlen(title) == 0)
        ? QChar('?')
        : QChar(title[0]);

    drawAvatar(image, letter, peerHandle);
    delete [] title;
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
    delete localCallListener;
    delete remoteCallListener;
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
   if (mPeerid == mChatWindow->mMegaChatApi->getMyUserHandle())
   {
        if (checked)
        {
            mChatWindow->mMegaChatApi->disableVideo(mChatWindow->mChatRoom->getChatId());
            setAvatar();
            ui->videoRenderer->enableStaticImage();
        }
        else
        {
            mChatWindow->mMegaChatApi->enableVideo(mChatWindow->mChatRoom->getChatId());
            ui->videoRenderer->disableStaticImage();
        }
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
        ui->videoRenderer->disableStaticImage();
    }
    else
    {
        ui->videoRenderer->enableStaticImage();
    }
}

void CallGui::onVideoRecv()
{
    ui->videoRenderer->disableStaticImage();
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

int CallGui::getIndex() const
{
    return mIndex;
}

void CallGui::setIndex(int index)
{
    mIndex = index;
}

void CallGui::setAvatar()
{
    auto image = new QImage(QSize(262, 262), QImage::Format_ARGB32);
    drawPeerAvatar(*image);
    ui->videoRenderer->setStaticImage(image);
}

void CallGui::onChatBtn(bool)
{
    auto& txtChat = *mChatWindow->ui->mTextChatWidget;
    if (txtChat.isVisible())
    {
        txtChat.setStyleSheet("background-color: #000000");
        txtChat.hide();
    }
    else
    {
        txtChat.setStyleSheet("background-color: #FFFFFF");
        txtChat.show();
    }
}

void CallGui::onLocalStreamObtained(rtcModule::IVideoRenderer *& renderer)
{
    renderer = ui->videoRenderer;
}

void CallGui::onRemoteStreamAdded(rtcModule::IVideoRenderer*& rendererRet)
{
    rendererRet = ui->videoRenderer;
}

void CallGui::onLocalMediaError(const std::string err)
{
    KR_LOG_ERROR("=============LocalMediaFail: %s", err.c_str());
}
