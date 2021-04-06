#include "peerWidget.h"
#include <cstring>
#include <QPainter>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMenu>

PeerWidget::PeerWidget(megachat::MegaChatApi &megaChatApi, megachat::MegaChatHandle chatid, Cid_t cid, bool hiRes, bool local)
    : mMegaChatApi(megaChatApi)
    , mChatid(chatid)
    , mCid(cid)
    , mHiRes(hiRes)
    , mLocal(local)
{
    mMegaChatVideoListenerDelegate = new QTMegaChatVideoListener(&mMegaChatApi, this);
    mVideoRender = new VideoRendererQt(this);
    QHBoxLayout* layout = new QHBoxLayout();
    layout->addWidget(mVideoRender);
    setLayout(layout);

    if (mLocal)
    {
        mMegaChatApi.addChatLocalVideoListener(mChatid, mMegaChatVideoListenerDelegate);
    }
    else
    {
        mMegaChatApi.addChatRemoteVideoListener(mChatid, mCid, mHiRes, mMegaChatVideoListenerDelegate);
    }

    setMinimumSize(minimumSizeHint());
}

PeerWidget::~PeerWidget()
{
    removeVideoListener();
}

void PeerWidget::setOnHold(bool isOnHold)
{
    auto image = new QImage(QSize(262, 262), QImage::Format_ARGB32);
    image->fill(Qt::black);
    mVideoRender->disableStaticImage();
    if (isOnHold)
    {
        drawAvatar(*image, 'H', 0, true);
    }
    mVideoRender->setStaticImage(image);
    mVideoRender->enableStaticImage();
}

void PeerWidget::onChatVideoData(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    QImage *auxImg = CreateFrame(width, height, buffer, size);
    if(auxImg)
    {
        mVideoRender->setStaticImage(auxImg);
        mVideoRender->enableStaticImage();
    }
}

Cid_t PeerWidget::getCid() const
{
    return mCid;
}

QSize PeerWidget::sizeHint() const
{
    if (mLocal)
    {
        return QSize(200, 150);
    }

    if (mHiRes)
    {
        return QSize(960, 540);
    }
    else
    {
        return QSize(160, 120);
    }
}

QSize PeerWidget::minimumSizeHint() const
{
    if (mLocal)
    {
        return QSize(200, 150);
    }

    if (mHiRes)
    {
        return QSize(200, 150);
    }
    else
    {
        return QSize(80, 60);
    }
}

QImage *PeerWidget::CreateFrame(int width, int height, char *buffer, size_t size)
{
    if((width == 0) || (height == 0))
    {
        return NULL;
    }

    unsigned char *copyBuff = new unsigned char[size];
    memcpy(copyBuff, buffer, size);
    QImage *auxImg = new QImage(copyBuff, width, height, QImage::Format_RGBA8888, myImageCleanupHandler, copyBuff);
    if(auxImg->isNull())
    {
        delete [] copyBuff;
        return NULL;
    }
    return auxImg;
}

void PeerWidget::myImageCleanupHandler(void *info)
{
    if (info)
    {
        unsigned char *auxBuf = reinterpret_cast<unsigned char*> (info);
        delete [] auxBuf;
    }
}

void PeerWidget::showMenu(const QPoint &pos)
{
    if (mHiRes)
    {
        QMenu contextMenu(tr("High Resolution Menu"), this);

        QAction action1("Stop HiRes", this);
        connect(&action1, SIGNAL(triggered()), this, SLOT(onHiResStop()));
        contextMenu.addAction(&action1);

        contextMenu.exec(mapToGlobal(pos));
    }
    else
    {
        QMenu contextMenu(tr("VThumb Menu"), this);

        QAction action1("Request HiRes", this);
        connect(&action1, SIGNAL(triggered()), this, SLOT(onHiResRequest()));
        contextMenu.addAction(&action1);

        contextMenu.exec(mapToGlobal(pos));
    }
}

void PeerWidget::onHiResStop()
{
    mMegaChatApi.stopHiResVideo(mChatid, mCid);
}

void PeerWidget::onHiResRequest()
{
    mMegaChatApi.requestHiResVideo(mChatid, mCid);
}

void PeerWidget::drawPeerAvatar(QImage &image)
{
    std::unique_ptr<megachat::MegaChatRoom> chatroom = std::unique_ptr<megachat::MegaChatRoom>(mMegaChatApi.getChatRoom(mChatid));
    int nPeers = chatroom->getPeerCount();
    megachat::MegaChatHandle peerHandle = megachat::MEGACHAT_INVALID_HANDLE;
    const char *title = NULL;
    for (int i = 0; i < nPeers; i++)
    {
        if (chatroom->getPeerHandle(i) != mMegaChatApi.getMyUserHandle())
        {
            peerHandle = chatroom->getPeerHandle(i);
            title = chatroom->getPeerFullname(i);
            break;
        }
    }
    QChar letter = (!title || std::strlen(title) == 0)
        ? QChar('?')
        : QChar(title[0]);

    drawAvatar(image, letter, peerHandle);
    delete [] title;
}

void PeerWidget::drawAvatar(QImage &image, QChar letter, uint64_t userid, bool onHold)
{
    uint64_t auxId = mMegaChatApi.getMyUserHandle();
    image.fill(Qt::black);

    QColor color;
    if (onHold)
    {
        color = QColor("orange");
    }
    else {
        (userid != auxId)
                ? color = QColor("blue")
                : color = QColor("green");
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

bool PeerWidget::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
       QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
       if (mouseEvent->button() == Qt::RightButton)
       {
            if (mLocal)
            {
                return true;
            }

            showMenu(mouseEvent->pos());
       }
    }

    return true;
}

void PeerWidget::removeVideoListener()
{
    if (!mMegaChatVideoListenerDelegate)
    {
        return;
    }

    if (mLocal)
    {
        mMegaChatApi.removeChatLocalVideoListener(mChatid, mMegaChatVideoListenerDelegate);
    }
    else
    {
        mMegaChatApi.removeChatRemoteVideoListener(mChatid, mCid, mHiRes, mMegaChatVideoListenerDelegate);
    }

    delete mMegaChatVideoListenerDelegate;
    mMegaChatVideoListenerDelegate = nullptr;
}
