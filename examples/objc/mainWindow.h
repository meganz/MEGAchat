//
//  ViewController.h
//  testapp
//
//  Created by Alex Vasilev on 4/5/15.
//  Copyright (c) 2015 Alex Vasilev. All rights reserved.
//
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#import <UIKit/UIKit.h>
#include <mstrophepp.h>
#include <IRtcModule.h>
#include <IJingleSession.h>
#include <mstrophepp.h>
#include "../strophe.disco.h"
#include "chatClient.h"
#include <videoRenderer_objc.h>
extern bool inCall;
extern std::unique_ptr<karere::ChatClient> gClient;

@interface MainWindow : UIViewController

@property(nonatomic, assign) IBOutlet UITextField* calleeInput;
@property(nonatomic, assign) IBOutlet UIButton* btnCall;

@property(nonatomic, assign) IBOutlet VideoRendererObjc* localRenderer;
@property(nonatomic, assign) IBOutlet VideoRendererObjc* remoteRenderer;
@property(nonatomic, assign) IBOutlet UITextView* logView;
@property(nonatomic, assign) IBOutlet UISegmentedControl* camSelect;
@property(nonatomic, assign) IBOutlet UISwitch* audioEnabled;
@property(nonatomic, assign) IBOutlet UISwitch* videoEnabled;
@property(nonatomic, assign) IBOutlet UIButton* btnTextChat;

//text chat GUI
@property(nonatomic, assign) IBOutlet UIButton* btnSend;
@property(nonatomic, assign) IBOutlet UIButton* btnInvite;
@property(nonatomic, assign) IBOutlet UITextView* msgEdit;

-(IBAction) onBtnInvite;
-(IBAction) onBtnSend;
-(IBAction) onBtnLeave;
-(IBAction) onBtnCall;
-(IBAction) onCamSelect;

@property(nonatomic, assign) std::string chatRoomJid;

-(void) roomAdded:(std::string&) roomJid;
-(void) contactAdded:(std::string&) contactJid;
-(void) contactStateChange:(std::string&) contactJid oldState: (karere::Presence) oldState newState:(karere::Presence) newState;
-(void) log:(NSString*) message;

@end

/*
@interface CallAnswerGui: UIViewController

rtcModule::IAnswerCall* ctrl;
MainWindow* mMainWin;

std::unique_ptr<QMessageBox> msg;
-(void) init: (rtcModule::IAnswerCall*) aCtrl win:(MainWindow*) win
            :ctrl(aCtrl), mMainWin(win),
        msg(new QMessageBox(QMessageBox::Information,
        "Incoming call", QString::fromAscii(ctrl->callerFullJid())+" is calling you",
        QMessageBox::NoButton, mMainWin))
    {
        msg->setAttribute(Qt::WA_DeleteOnClose);
        answerBtn = msg->addButton("Answer", QMessageBox::AcceptRole);
        rejectBtn = msg->addButton("Reject", QMessageBox::RejectRole);
        msg->setWindowModality(Qt::NonModal);
        QObject::connect(msg.get(), SIGNAL(buttonClicked(QAbstractButton*)),
            this, SLOT(onBtnClick(QAbstractButton*)));
        msg->show();
        msg->raise();
    }
public slots:
    void onBtnClick(QAbstractButton* btn)
    {
        ctrl->setUserData(nullptr);
        msg->close();
        if (btn == answerBtn)
        {
            int ret = ctrl->answer(true, rtcModule::AvFlags(true, true), nullptr, nullptr);
            if (ret == 0)
            {
                inCall = true;
                mMainWin->ui->callBtn->setText("Hangup");
            }
        }
        else
        {
            ctrl->answer(false, rtcModule::AvFlags(true, true), "hangup", nullptr);
            inCall = false;
            mMainWin->ui->callBtn->setText("Call");
        }
    }
};
*/

class RtcEventHandler: public rtcModule::IEventHandler
{
protected:
    MainWindow* mMainWindow;
    virtual ~RtcEventHandler(){}
public:
    RtcEventHandler(MainWindow* mainWindow)
        :mMainWindow(mainWindow){}
    virtual void onLocalStreamObtained(IVideoRenderer** renderer)
    {
        inCall = true;
        mMainWindow.btnCall.titleLabel.text = @"Hangup";
        *renderer = mMainWindow.localRenderer.videoRenderer;
    }
    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, IVideoRenderer** rendererRet)
    {
        *rendererRet = mMainWindow.remoteRenderer.videoRenderer;
    }
    virtual void onCallIncomingRequest(rtcModule::IAnswerCall* ctrl, karere::AvFlags av)
    {
        //ctrl->setUserData(new CallAnswerGui(ctrl, mMainWindow));
    }
    virtual void onIncomingCallCanceled(const char *sid, const char *event, const char *by, int accepted, void **userp)
    {
/*        if(*userp)
        {
            delete static_cast<CallAnswerGui*>(*userp);
            *userp = nullptr;
        }
*/
         printf("Call canceled for reason: %s\n", event);
    }

    virtual void onCallEnded(rtcModule::IJingleSession *sess,
        const char* reason, const char* text, rtcModule::IRtcStats *stats)
    {
        printf("on call ended\n");
        inCall = false;
        mMainWindow.btnCall.titleLabel.text = @"Call";
    }
    virtual void discoAddFeature(const char *feature)
    {
        gClient->conn->plugin<disco::DiscoPlugin>("disco").addFeature(feature);
    }
    virtual void onLocalMediaFail(const char* err, int* cont = nullptr)
    {
        KR_LOG_ERROR("=============LocalMediaFail: %s", err);
    }

};

#endif // MAINWINDOW_H
