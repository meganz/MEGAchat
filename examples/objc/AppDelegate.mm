//
//  AppDelegate.m
//  testapp
//
//  Created by Alex Vasilev on 4/5/15.
//  Copyright (c) 2015 Alex Vasilev. All rights reserved.
//

#import "AppDelegate.h"
#include <chatClient.h>
#include <services.h>

UIApplication* gTheApp = nil;
std::shared_ptr<karere::ChatClient> gClient;

@interface AppDelegate ()

@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    gTheApp = application;
    services_init([](void* msg)
    {
        printf("even recv\n");
        [gTheApp performSelectorOnMainThread: @selector(processMessage)
                    withObject: [NSValue valueWithPointer: msg]
                    waitUntilDone:FALSE
        ];
    }, SVC_STROPHE_LOG);
    gClient.reset(new karere::ChatClient("lpetrov+mega14@me.com", "megarullz"));
//    gClient->registerRtcHandler(new RtcEventHandler(mainWin));
    gClient->init()
    .then([](int)
          {
              printf("logged in\n");
              rtcModule::IPtr<rtcModule::IDeviceList> audio(gClient->rtc->getAudioInDevices());
 //             for (size_t i=0, len=audio->size(); i<len; i++)
 //                 mainWin->ui->audioInCombo->addItem(audio->name(i).c_str());
              rtcModule::IPtr<rtcModule::IDeviceList> video(gClient->rtc->getVideoInDevices());
 //             for (size_t i=0, len=video->size(); i<len; i++)
 //                 mainWin->ui->videoInCombo->addItem(video->name(i).c_str());
              gClient->rtc->updateIceServers(KARERE_DEFAULT_TURN_SERVERS);
              
 //             mainWin->ui->callBtn->setEnabled(true);
 //             mainWin->ui->callBtn->setText("Call");
              
              std::vector<std::string> contacts = gClient->getContactList().getContactJids();
              
 //             for(size_t i=0; i<contacts.size();i++)
 //             {
 //                 mainWin->ui->contactList->addItem(new QListWidgetItem(QIcon("/images/online.png"), contacts[i].c_str()));
 //             }
              return 0;
          })
    .fail([](const promise::Error& error)
          {
              printf("==========Client::start() promise failed:\n%s\n", error.msg().c_str());
              return error;
          });
    
    return YES;
}
- (void)processMessage:(NSValue*) wrappedPtr
{
    megaProcessMessage([wrappedPtr pointerValue]);
}
- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end
