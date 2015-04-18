//
//  ViewController.m
//  testapp
//
//  Created by Alex Vasilev on 4/5/15.
//  Copyright (c) 2015 Alex Vasilev. All rights reserved.
//
#include "AppDelegate.h"
#import "mainWindow.h"
#include <services.h>
#include <gcm.h>
#include "rtcModule/IRtcModule.h"
#include <rapidjson/document.h>
#include <sdkApi.h>
#include <chatClient.h>
#include <messageBus.h>

@interface MainWindow()

@end

MainWindow* gMainWin;
extern std::unique_ptr<karere::ChatClient> gClient;

using namespace std;
using namespace mega;

bool inCall = false;


@implementation MainWindow

- (void)viewDidLoad {
    [super viewDidLoad];
    self.btnCall.layer.cornerRadius = 4;
    self.btnCall.layer.borderWidth = 1;
    self.btnCall.layer.borderColor = [UIColor blueColor].CGColor;
    [self.btnCall.titleLabel setTextAlignment: NSTextAlignmentCenter];

    // Do any additional setup after loading the view, typically from a nib.
    [self startClient];
    
    message_bus::MessageListener<M_MESS_PARAMS> l = {
        "guiRoomListener",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
               message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string rm(ROOM_ADDED_JID);
            
            std::string roomJid = message->getValue<std::string>(rm);
            [self roomAdded: roomJid];
        }
    };
    
    message_bus::SharedMessageBus<M_BUS_PARAMS>::
    getMessageBus()->addListener(ROOM_ADDED_EVENT, l);
    
    message_bus::MessageListener<M_MESS_PARAMS> c = {
        "guiContactListener",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
               message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string contactJid = message->getValue<std::string>(CONTACT_JID);
            karere::Presence oldState = message->getValue<karere::Presence>(CONTACT_OLD_STATE);
            karere::Presence newState = message->getValue<karere::Presence>(CONTACT_STATE);
            [self contactStateChange: contactJid oldState: oldState  newState: newState];
        }
    };
    
    message_bus::SharedMessageBus<M_BUS_PARAMS>::
    getMessageBus()->addListener(CONTACT_CHANGED_EVENT, c);
    
    message_bus::MessageListener<M_MESS_PARAMS> ca = {
        "guiContactListener",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
               message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string contactJid = message->getValue<std::string>(CONTACT_JID);
            [self contactAdded: contactJid];
        }
    };
    
    message_bus::SharedMessageBus<M_BUS_PARAMS>::
    getMessageBus()->addListener(CONTACT_ADDED_EVENT, ca);
    
    message_bus::MessageListener<M_MESS_PARAMS> o = {
        "guiErrorListener",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
               message_bus::MessageListener<M_MESS_PARAMS> &listener) {
            std::string errMsg =
            message->getValue<std::string>(ERROR_MESSAGE_CONTENTS);
            [self log: [NSString stringWithFormat: @"Error: %s",errMsg.c_str()]];
        }
    };
    
    message_bus::SharedMessageBus<M_BUS_PARAMS>::
    getMessageBus()->addListener(ERROR_MESSAGE_EVENT, o);
    
    message_bus::MessageListener<M_MESS_PARAMS> p = {
        "guiGeneralListener",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
               message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string generalMessage("GENERAL: ");
            generalMessage.append(message->getValue<std::string>(GENERAL_EVENTS_CONTENTS));
            [self log: [NSString stringWithFormat: @"Error: %s", generalMessage.c_str()]];
        }
    };
    
    message_bus::SharedMessageBus<M_BUS_PARAMS>::
    getMessageBus()->addListener(GENERAL_EVENTS, p);
    
    message_bus::MessageListener<M_MESS_PARAMS> q = {
        "guiWarningListener",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
               message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string warMsg =
            message->getValue<std::string>(WARNING_MESSAGE_CONTENTS);
            [self log: [NSString stringWithFormat:@"Warning: %s", warMsg.c_str()]];
        }
    };
    
    message_bus::SharedMessageBus<M_BUS_PARAMS>::
    getMessageBus()->addListener(WARNING_MESSAGE_EVENT, q);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
-(void) startClient
{
    static AppDelegate* theAppDelegate = [[UIApplication sharedApplication] delegate];
    services_init([](void* msg)
        {
            [theAppDelegate performSelectorOnMainThread: @selector(processMessage:)
                withObject: [NSValue valueWithPointer: msg]
                waitUntilDone: FALSE
            ];
        }, SVC_STROPHE_LOG);
    gClient.reset(new karere::ChatClient("lpetrov+mega14@me.com", "megarullz"));
    gClient->registerRtcHandler(new RtcEventHandler(self));
    gClient->init()
    .then([self](int)
          {
              printf("Logged in\n");
              rtcModule::IPtr<rtcModule::IDeviceList> video(gClient->rtc->getVideoInDevices());
              for (size_t i=0, len=video->size(); i<len; i++)
                  [self log: [NSString stringWithFormat: @"videoInput %zu: '%s'", i, video->name(i).c_str()]];
            
              gClient->rtc->selectVideoInDevice("Front Camera");
              gClient->rtc->updateIceServers(KARERE_DEFAULT_TURN_SERVERS);
              
              self.btnCall.enabled = YES;
              self.btnCall.titleLabel.text = @"Call";
              
              std::vector<std::string> contacts = gClient->getContactList().getContactJids();
              
              //for(size_t i=0; i<contacts.size();i++)
              //{
              //   mainWin->ui->contactList->addItem(new QListWidgetItem(QIcon("/images/online.png"), contacts[i].c_str()));
              //}
              return 0;
          })
    .fail([](const promise::Error& error)
          {
              printf("==========Client::start() promise failed:\n%s\n", error.msg().c_str());
              return error;
          });
    
}

-(void) log: (NSString*) message
{
    printf("%s\n", [message UTF8String]);
    self.logView.text = [self.logView.text stringByAppendingFormat: @"%@\n", message];
}

-(void) onCamSelect
{
    NSString* deviceName = [[self.camSelect titleForSegmentAtIndex:[self.camSelect selectedSegmentIndex]] stringByAppendingString: @" Camera"];
    if (gClient->rtc->selectVideoInDevice([deviceName UTF8String]) < 0)
    {
        [self log: [NSString stringWithFormat:@"Error selecting camera '%@'", deviceName]];
        return;
    }
    [self log: [NSString stringWithFormat:@"Selected camera '%@'", deviceName]];
}

-(void) contactStateChange: (std::string&) contactJid oldState:(karere::Presence) oldState newState: (karere::Presence) newState
{
    std::string msg = std::string("contact ") + contactJid + std::string(" changed from ") + karere::ContactList::presenceToText(oldState) + std::string(" to ") + karere::ContactList::presenceToText(newState);
    [self log: [NSString stringWithUTF8String: msg.c_str()]];
}

-(void) contactAdded: (std::string&) contactJid
{
	printf("add contact %s\n", contactJid.c_str());
}

-(void) roomAdded: (std::string&) roomJid
{
    self.chatRoomJid = roomJid;
    std::string listenEvent(ROOM_MESSAGE_EVENT);
    listenEvent.append(self.chatRoomJid);
    KR_LOG("************* eventName = %s", listenEvent.c_str());
    message_bus::MessageListener<M_MESS_PARAMS> l {
        "guiListenerNoOne",
        [self](message_bus::SharedMessage<M_MESS_PARAMS> &message,
                message_bus::MessageListener<M_MESS_PARAMS> &listener){
            std::string data = message->getValue<std::string>(ROOM_MESSAGE_CONTENTS);
            [self log: [NSString stringWithUTF8String: data.c_str()]];
        }
    };
    message_bus::SharedMessageBus<M_BUS_PARAMS>::getMessageBus()->addListener(listenEvent, l);
}

-(void) onBtnInvite
{
    std::string peerMail = [self.calleeInput.text UTF8String];
    if (peerMail.empty())
    {
        [self log: @"Error: Invalid user entered in peer input box"];
        return;
    }

    gClient->invite(peerMail);

}

-(void) onBtnLeave
{
    gClient->leaveRoom(self.chatRoomJid);
}

-(void) onBtnSend
{
    gClient->sendMessage(self.chatRoomJid, [self.msgEdit.text UTF8String]);
    self.msgEdit.text = @"";
}

-(void) onBtnCall
{
    if (inCall)
    {
        gClient->rtc->hangupAll("hangup", nullptr);
        inCall = false;
        self.btnCall.titleLabel.text = @"Call";
    }
    else
    {
        std::string peerMail = [self.calleeInput.text UTF8String];
        if (peerMail.empty())
        {
            [self log: @"Error: Invalid user entered in peer input box"];
            return;
        }
        gClient->api->call(&MegaApi::getUserData, peerMail.c_str())
        .then([self](ReqResult result)
        {
            const char* peer = result->getText();
            if (!peer)
                throw std::runtime_error("Returned peer user is NULL");

            string peerJid = string(peer)+"@"+KARERE_XMPP_DOMAIN;
            return karere::ChatRoom<MPENC_T_PARAMS>::create(*gClient, peerJid);
        })
        .then([self](shared_ptr<karere::ChatRoom<MPENC_T_PARAMS>> room)
        {
            rtcModule::AvFlags av;
            av.audio = true;
            av.video = true;
            char sid[rtcModule::RTCM_SESSIONID_LEN+2];
            gClient->rtc->startMediaCall(sid, room->peerFullJid().c_str(), av, nullptr);
            self.chatRoomJid = room->roomJid();
            gClient->chatRooms[self.chatRoomJid]->addUserToChat(room->peerFullJid());
            inCall = true;
            self.btnCall.titleLabel.text = @"Hangup";
            return nullptr;
        })
        .fail([self](const promise::Error& err)
        {
            if (err.type() == 0x3e9aab10)
                [self log: @"Error: Callee user not recognized"];
            else
                [self log: [NSString stringWithFormat:@"Error: Error calling user: %s", err.msg().c_str()]];
            return nullptr;
        });
    }

}

@end




