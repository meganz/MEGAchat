#ifndef PEERWIDGET_H
#define PEERWIDGET_H

#include <megachatapi.h>
#include <videoRenderer_Qt.h>

#include <QWidget>
#include <webrtc.h>

class PeerWidget : public QWidget, public megachat::MegaChatVideoListener
{
    Q_OBJECT
public:
    PeerWidget(megachat::MegaChatApi &megaChatApi, megachat::MegaChatHandle chatid, Cid_t cid, bool hiRes, bool local = false);
    ~PeerWidget();
    void onChatVideoData(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int width, int height, char *buffer, size_t size) override;
    Cid_t getCid() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void drawPeerAvatar(QImage &image);
    void drawAvatar(QImage &image, QChar letter, uint64_t userid);
    bool event(QEvent *event) override;

protected:
    VideoRendererQt* mVideoRender;
    megachat::MegaChatApi &mMegaChatApi;
    megachat::MegaChatHandle mChatid;
    Cid_t mCid;
    bool mHiRes = false;
    bool mLocal = false;

    QImage* CreateFrame(int width, int height, char *buffer, size_t size);
    static void myImageCleanupHandler(void *info);

    void showMenu(const QPoint &pos);

protected slots:
    void onHiResStop();
    void onHiResRequest();
};

#endif // PEERWIDGET_H
