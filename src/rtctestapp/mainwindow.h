#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <mstrophepp.h>
#include "../strophe.disco.h"
#include <ui_mainwindow.h>
#include "karereCommon.h"
#include "IJingleSession.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    std::shared_ptr<strophe::Connection> mConn;
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    Ui::MainWindow *ui;
public slots:
    void buttonPushed();
    void megaMessageSlot(void* msg);
};

class AppDelegate: public QObject
{
    Q_OBJECT
public slots:
    void onAppTerminate();
};

class RtcEventHandler: public rtcModule::IEventHandler
{
protected:
    disco::DiscoPlugin& mDisco;
    MainWindow* mMainWindow;
public:
    RtcEventHandler(MainWindow* mainWindow)
        :mMainWindow(mainWindow), mDisco(mainWindow->mConn->plugin<disco::DiscoPlugin>("disco"))
    {}
    virtual void addDiscoFeature(const char* feature)
    {
        mDisco.addFeature(feature);
    }
    virtual void onLocalStreamObtained(IVideoRenderer** renderer)
    {
        *renderer = mMainWindow->ui->localRenderer;
    }
    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, IVideoRenderer** rendererRet)
    {
        KR_LOG_COLOR(31, "custom onRemoteSdpRecv");
        *rendererRet = mMainWindow->ui->remoteRenderer;
    }

};

#endif // MAINWINDOW_H
