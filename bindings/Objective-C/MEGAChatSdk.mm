#import "MEGAChatSdk.h"
#import "megachatapi.h"
#import "MEGASdk+init.h"
#import "MEGAChatSdk+init.h"
#import "MEGAChatError+init.h"
#import "MEGAChatRoom+init.h"
#import "MEGAChatRoomList+init.h"
#import "MEGAChatPeerList+init.h"
#import "MEGAChatMessage+init.h"
#import "MEGAChatListItem+init.h"
#import "MEGAChatListItemList+init.h"
#import "MEGAChatScheduledMeeting+init.h"
#import "MEGAChatPresenceConfig+init.h"
#import "MEGANodeList+init.h"
#import "MEGAHandleList+init.h"
#import "MEGAStringList+init.h"
#import "MEGAChatCall+init.h"
#import "DelegateMEGAChatRequestListener.h"
#import "DelegateMEGAChatLoggerListener.h"
#import "DelegateMEGAChatRoomListener.h"
#import "DelegateMEGAChatListener.h"
#import "DelegateMEGAChatCallListener.h"
#import "DelegateMEGAChatVideoListener.h"
#import "DelegateMEGAChatNotificationListener.h"
#import "DelegateMEGAChatNodeHistoryListener.h"
#import "DelegateMEGAChatScheduledMeetingListener.h"

#import <set>
#import <pthread.h>

using namespace megachat;

@interface MEGAChatSdk () {
    pthread_mutex_t listenerMutex;
}

@property (nonatomic, assign) std::set<DelegateMEGAChatRequestListener *>activeRequestListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatRoomListener *>activeChatRoomListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatListener *>activeChatListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatCallListener *>activeChatCallListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatVideoListener *>activeChatLocalVideoListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatVideoListener *>activeChatRemoteVideoListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatNotificationListener *>activeChatNotificationListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatNodeHistoryListener *>activeChatNodeHistoryListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatScheduledMeetingListener *>activeChatScheduledMeetingListeners;

@property (nonatomic, nullable) MegaChatApi *megaChatApi;
- (MegaChatApi *)getCPtr;

@end

@implementation MEGAChatSdk

static DelegateMEGAChatLoggerListener *externalLogger = NULL;

#pragma mark - Init

- (instancetype)init:(MEGASdk *)megaSDK {
    
    if (!externalLogger) {
        externalLogger = new DelegateMEGAChatLoggerListener(nil);
    }
    
    self.megaChatApi = new MegaChatApi((::mega::MegaApi *)[megaSDK getCPtr]);
    
    if (pthread_mutex_init(&listenerMutex, NULL)) {
        return nil;
    }
    
    return self;
}

- (MEGAChatInit)initKarereWithSid:(NSString *)sid {
    if (self.megaChatApi == nil) return MEGAChatInitError;
    return (MEGAChatInit) self.megaChatApi->init(sid.UTF8String);
}

- (MEGAChatInit)initKarereLeanModeWithSid:(NSString *)sid {
    if (self.megaChatApi == nil) return MEGAChatInitError;
    return (MEGAChatInit) self.megaChatApi->initLeanMode(sid.UTF8String);
}

- (void)importMessagesFromPath:(NSString *)externalDbPath delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->importMessages(externalDbPath.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)importMessagesFromPath:(NSString *)externalDbPath {
    if (self.megaChatApi) {
        self.megaChatApi->importMessages(externalDbPath.UTF8String);
    }
}

- (MEGAChatInit)initAnonymous {
    if (self.megaChatApi == nil) return MEGAChatInitError;
    return (MEGAChatInit) self.megaChatApi->initAnonymous();
}

- (void)resetClientId {
    if (self.megaChatApi) {
        self.megaChatApi->resetClientid();
    }
}

- (MEGAChatInit)initState {
    if (self.megaChatApi == nil) return MEGAChatInitError;
    return (MEGAChatInit) self.megaChatApi->getInitState();
}

- (MEGAChatConnection)chatConnectionState:(uint64_t)chatId {
    if (self.megaChatApi == nil) return MEGAChatConnectionOffline;
    return (MEGAChatConnection) self.megaChatApi->getChatConnectionState(chatId);
}

- (BOOL)areAllChatsLoggedIn {
    if (self.megaChatApi == nil) return NO;
    return self.megaChatApi->areAllChatsLoggedIn();
}

- (BOOL)isOnlineStatusPending {
    if (self.megaChatApi == nil) return NO;
    return self.megaChatApi->isOnlineStatusPending();
}

- (void)retryPendingConnections {
    if (self.megaChatApi) {
        self.megaChatApi->retryPendingConnections();
    }
}

- (void)reconnect {
    if (self.megaChatApi) {
        self.megaChatApi->retryPendingConnections(true);
    }
}

- (void)refreshUrls {
    if (self.megaChatApi) {
        self.megaChatApi->refreshUrl();
    }
}

- (void)dealloc {
    delete _megaChatApi;
    _megaChatApi = nil;
    pthread_mutex_destroy(&listenerMutex);
}

- (void)deleteMegaChatApi {
    delete _megaChatApi;
    _megaChatApi = nil;
    pthread_mutex_destroy(&listenerMutex);
}

- (MegaChatApi *)getCPtr {
    return _megaChatApi;
}

- (uint64_t)myUserHandle {
    if (self.megaChatApi == nil) return MEGACHAT_INVALID_HANDLE;
    return self.megaChatApi->getMyUserHandle();
}

#pragma mark - Logout

- (void)logoutWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->logout([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)logout {
    if (self.megaChatApi) {
        self.megaChatApi->logout();
    }
}

- (void)localLogoutWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->localLogout([self createDelegateMEGAChatRequestListener:delegate singleListener:YES queueType:ListenerQueueTypeCurrent]);
    }
}

- (void)localLogout {
    if (self.megaChatApi) {
        self.megaChatApi->localLogout();
    }
}

#pragma mark - Presence

- (void)setOnlineStatus:(MEGAChatStatus)status delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setOnlineStatus((int)status, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setOnlineStatus:(MEGAChatStatus)status {
    if (self.megaChatApi) {
        self.megaChatApi->setOnlineStatus((int)status);
    }
}

- (MEGAChatStatus)onlineStatus {
    if (self.megaChatApi == nil) return MEGAChatStatusInvalid;
    return (MEGAChatStatus)self.megaChatApi->getOnlineStatus();
}

- (void)setPresenceAutoaway:(BOOL)enable timeout:(NSInteger)timeout {
    if (self.megaChatApi) {
        self.megaChatApi->setPresenceAutoaway(enable, (int)timeout);
    }
}

- (void)setPresencePersist:(BOOL)enable {
    if (self.megaChatApi) {
        self.megaChatApi->setPresencePersist(enable);
    }
}

- (void)setLastGreenVisible:(BOOL)enable delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setLastGreenVisible(enable, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setLastGreenVisible:(BOOL)enable {
    if (self.megaChatApi) {
        self.megaChatApi->setLastGreenVisible(enable);
    }
}

- (void)requestLastGreen:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->requestLastGreen(userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)requestLastGreen:(uint64_t)userHandle {
    if (self.megaChatApi) {
        self.megaChatApi->requestLastGreen(userHandle);
    }
}

- (BOOL)isSignalActivityRequired {
    if (self.megaChatApi == nil) return NO;
    return self.megaChatApi->isSignalActivityRequired();
}

- (void)signalPresenceActivity {
    if (self.megaChatApi) {
        self.megaChatApi->signalPresenceActivity();
    }
}

- (MEGAChatPresenceConfig *)presenceConfig {
    return self.megaChatApi ? [[MEGAChatPresenceConfig alloc] initWithMegaChatPresenceConfig:self.megaChatApi->getPresenceConfig() cMemoryOwn:YES] : nil;
}

- (MEGAChatStatus)userOnlineStatus:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return MEGAChatStatusInvalid;
    return (MEGAChatStatus)self.megaChatApi->getUserOnlineStatus(userHandle);
}

- (void)setBackgroundStatus:(BOOL)status delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setBackgroundStatus(status, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setBackgroundStatus:(BOOL)status {
    if (self.megaChatApi) {
        self.megaChatApi->setBackgroundStatus(status);
    }
}

#pragma mark - Add and remove delegates

- (void)addChatRoomDelegate:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatRoomListener(chatId, [self createDelegateMEGAChatRoomListener:delegate singleListener:NO queueType:ListenerQueueTypeCurrent]);
    }
}

- (void)removeChatRoomDelegate:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    std::vector<DelegateMEGAChatRoomListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatRoomListener *>::iterator it = _activeChatRoomListeners.begin();
    while (it != _activeChatRoomListeners.end()) {
        DelegateMEGAChatRoomListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatRoomListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeChatRoomListener(chatId, listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

- (void)addChatRequestDelegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatRequestListener([self createDelegateMEGAChatRequestListener:delegate singleListener:NO]);
    }
}

- (void)removeChatRequestDelegate:(id<MEGAChatRequestDelegate>)delegate {
    std::vector<DelegateMEGAChatRequestListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatRequestListener *>::iterator it = _activeRequestListeners.begin();
    while (it != _activeRequestListeners.end()) {
        DelegateMEGAChatRequestListener  *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeRequestListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeChatRequestListener(listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

- (void)addChatDelegate:(id<MEGAChatDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatListener([self createDelegateMEGAChatListener:delegate singleListener:NO]);
    }
}

- (void)removeChatDelegate:(id<MEGAChatDelegate>)delegate {
    std::vector<DelegateMEGAChatListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatListener *>::iterator it = _activeChatListeners.begin();
    while (it != _activeChatListeners.end()) {
        DelegateMEGAChatListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeChatListener(listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}



- (void)addChatNotificationDelegate:(id<MEGAChatNotificationDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatNotificationListener([self createDelegateMEGAChatNotificationListener:delegate singleListener:NO]);
    }
}

- (void)removeChatNotificationDelegate:(id<MEGAChatNotificationDelegate>)delegate {
    std::vector<DelegateMEGAChatNotificationListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatNotificationListener *>::iterator it = _activeChatNotificationListeners.begin();
    while (it != _activeChatNotificationListeners.end()) {
        DelegateMEGAChatNotificationListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatNotificationListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeChatNotificationListener(listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

- (void)addChatScheduledMeetingDelegate:(id<MEGAChatScheduledMeetingDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addSchedMeetingListener([self createDelegateMEGAChatScheduledMeetingListener:delegate singleListener:YES]);
    }
}

- (void)addChatScheduledMeetingDelegate:(id<MEGAChatScheduledMeetingDelegate>)delegate queueType:(ListenerQueueType)queueType {
    if (self.megaChatApi) {
        self.megaChatApi->addSchedMeetingListener([self createDelegateMEGAChatScheduledMeetingListener:delegate singleListener:YES queueType:queueType]);
    }
}

- (void)removeChatScheduledMeetingDelegate:(id<MEGAChatScheduledMeetingDelegate>)delegate {
    std::vector<DelegateMEGAChatScheduledMeetingListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatScheduledMeetingListener *>::iterator it = _activeChatScheduledMeetingListeners.begin();
    while (it != _activeChatScheduledMeetingListeners.end()) {
        DelegateMEGAChatScheduledMeetingListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatScheduledMeetingListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeSchedMeetingListener(listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

#ifndef KARERE_DISABLE_WEBRTC

- (void)addChatCallDelegate:(id<MEGAChatCallDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatCallListener([self createDelegateMEGAChatCallListener:delegate singleListener:NO]);
    }
}

- (void)removeChatCallDelegate:(id<MEGAChatCallDelegate>)delegate {
    std::vector<DelegateMEGAChatCallListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatCallListener *>::iterator it = _activeChatCallListeners.begin();
    while (it != _activeChatCallListeners.end()) {
        DelegateMEGAChatCallListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatCallListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeChatCallListener(listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

- (void)addChatLocalVideo:(uint64_t)chatId delegate:(id<MEGAChatVideoDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatLocalVideoListener(chatId, [self createDelegateMEGAChatLocalVideoListener:delegate singleListener:YES]);
    }
}

- (void)removeChatLocalVideo:(uint64_t)chatId delegate:(id<MEGAChatVideoDelegate>)delegate {
    std::vector<DelegateMEGAChatVideoListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatVideoListener *>::iterator it = _activeChatLocalVideoListeners.begin();
    while (it != _activeChatLocalVideoListeners.end()) {
        DelegateMEGAChatVideoListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatLocalVideoListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeChatLocalVideoListener(chatId, listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

- (void)addChatRemoteVideo:(uint64_t)chatId cliendId:(uint64_t)clientId hiRes:(BOOL)hiRes delegate:(id<MEGAChatVideoDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addChatRemoteVideoListener(chatId, clientId, hiRes, [self createDelegateMEGAChatRemoteVideoListener:delegate singleListener:YES]);
    }
}

- (void)removeChatRemoteVideo:(uint64_t)chatId cliendId:(uint64_t)clientId hiRes:(BOOL)hiRes delegate:(id<MEGAChatVideoDelegate>)delegate {
    std::vector<DelegateMEGAChatVideoListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatVideoListener *>::iterator it = _activeChatRemoteVideoListeners.begin();
    while (it != _activeChatRemoteVideoListeners.end()) {
        DelegateMEGAChatVideoListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatRemoteVideoListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {        if (self.megaChatApi) {
            self.megaChatApi->removeChatRemoteVideoListener(chatId, clientId, hiRes, listenersToRemove[i]);
        }
        delete listenersToRemove[i];
    }
}

#endif

#pragma mark - My user attributes

- (NSString *)myFirstname {
    if (self.megaChatApi == nil) return nil;
    char *val = self.megaChatApi->getMyFirstname();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)myLastname {
    if (self.megaChatApi == nil) return nil;
    char *val = self.megaChatApi->getMyLastname();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)myFullname {
    if (self.megaChatApi == nil) return nil;
    char *val = self.megaChatApi->getMyFullname();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)myEmail {
    if (self.megaChatApi == nil) return nil;
    char *val = self.megaChatApi->getMyEmail();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

#pragma mark - Chat rooms and chat list items

- (MEGAChatRoomList *)chatRooms {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatRoomList alloc] initWithMegaChatRoomList:self.megaChatApi->getChatRooms() cMemoryOwn:YES];
}

- (MEGAChatRoomList *)chatRoomsByType:(MEGAChatType)type {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatRoomList alloc] initWithMegaChatRoomList:self.megaChatApi->getChatRoomsByType(type) cMemoryOwn:YES];
}

- (MEGAChatRoom *)chatRoomForChatId:(uint64_t)chatId {
    if (self.megaChatApi == nil) return nil;
    MegaChatRoom *chatRoom = self.megaChatApi->getChatRoom(chatId);
    return chatRoom ? [[MEGAChatRoom alloc] initWithMegaChatRoom:chatRoom cMemoryOwn:YES] : nil;
}

- (MEGAChatRoom *)chatRoomByUser:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return nil;
    MegaChatRoom *chatRoom = self.megaChatApi->getChatRoomByUser(userHandle);
    return chatRoom ? [[MEGAChatRoom alloc] initWithMegaChatRoom:chatRoom cMemoryOwn:YES] : nil;
}

- (MEGAChatListItemList *)chatListItemsByType:(MEGAChatType)type {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getChatListItemsByType(type) cMemoryOwn:YES];
}

- (MEGAChatListItemList *)chatListItems {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getChatListItems() cMemoryOwn:YES];
}

- (NSInteger)unreadChats {
    if (self.megaChatApi == nil) return 0;
    return self.megaChatApi->getUnreadChats();
}

- (MEGAChatListItemList *)activeChatListItems {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getActiveChatListItems() cMemoryOwn:YES];
}

- (MEGAChatListItemList *)archivedChatListItems {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getArchivedChatListItems() cMemoryOwn:YES];
}

- (MEGAChatListItemList *)inactiveChatListItems {
    if (self.megaChatApi == nil) return nil;
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getInactiveChatListItems() cMemoryOwn:YES];
}

- (MEGAChatListItem *)chatListItemForChatId:(uint64_t)chatId {
    if (self.megaChatApi == nil) return nil;
    MegaChatListItem *item = self.megaChatApi->getChatListItem(chatId);
    return item ? [[MEGAChatListItem alloc] initWithMegaChatListItem:item cMemoryOwn:YES] : nil;
}

- (uint64_t)chatIdByUserHandle:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return MEGACHAT_INVALID_HANDLE;
    return self.megaChatApi->getChatHandleByUser(userHandle);
}

#pragma mark - Users attributes

- (void)userEmailByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->getUserEmail(userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)userEmailByUserHandle:(uint64_t)userHandle {
    if (self.megaChatApi) {
        self.megaChatApi->getUserEmail(userHandle);
    }
}

- (void)userFirstnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->getUserFirstname(userHandle, authorizationToken.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)userFirstnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken {
    if (self.megaChatApi) {
        self.megaChatApi->getUserFirstname(userHandle, authorizationToken.UTF8String);
    }
}

- (void)userLastnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->getUserLastname(userHandle, authorizationToken.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)userLastnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken {
    if (self.megaChatApi) {
        self.megaChatApi->getUserLastname(userHandle, authorizationToken.UTF8String);
    }
}

- (NSString *)userEmailFromCacheByUserHandle:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return nil;
    const char *val = self.megaChatApi->getUserEmailFromCache(userHandle);
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)userFirstnameFromCacheByUserHandle:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return nil;
    const char *val = self.megaChatApi->getUserFirstnameFromCache(userHandle);
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)userLastnameFromCacheByUserHandle:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return nil;
    const char *val = self.megaChatApi->getUserLastnameFromCache(userHandle);
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)userFullnameFromCacheByUserHandle:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return nil;
    const char *val = self.megaChatApi->getUserFullnameFromCache(userHandle);
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)contactEmailByHandle:(uint64_t)userHandle {
    if (self.megaChatApi == nil) return nil;
    const char *val = self.megaChatApi->getContactEmail(userHandle);
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (uint64_t)userHandleByEmail:(NSString *)email {
    if (self.megaChatApi == nil) return MEGACHAT_INVALID_HANDLE;
    return self.megaChatApi->getUserHandleByEmail([email UTF8String]);
}

- (void)loadUserAttributesForChatId:(uint64_t)chatId usersHandles:(NSArray<NSNumber *> *)usersHandles delegate:(id<MEGAChatRequestDelegate>)delegate {
    MEGAHandleList *handleList = [MEGAHandleList.alloc initWithMemoryOwn:YES];
    
    for (NSNumber *handle in usersHandles) {
        [handleList addMegaHandle:handle.unsignedLongLongValue];
    }
    
    if (self.megaChatApi) {
        self.megaChatApi->loadUserAttributes(chatId, handleList.getCPtr, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)loadUserAttributesForChatId:(uint64_t)chatId usersHandles:(NSArray<NSNumber *> *)usersHandles {
    MEGAHandleList *handleList = [MEGAHandleList.alloc initWithMemoryOwn:YES];
    
    for (NSNumber *handle in usersHandles) {
        [handleList addMegaHandle:handle.unsignedLongLongValue];
    }
    
    if (self.megaChatApi) {
        self.megaChatApi->loadUserAttributes(chatId, handleList.getCPtr);
    }
}

#pragma mark - Chat management

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createChat(group, peers.getCPtr, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers {
    if (self.megaChatApi) {
        self.megaChatApi->createChat(group, peers.getCPtr);
    }
}

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers title:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createChat(group, peers.getCPtr, title.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers title:(NSString *)title {
    if (self.megaChatApi) {
        self.megaChatApi->createChat(group, peers.getCPtr, title.UTF8String);
    }
}

- (void)createPublicChatWithPeers:(MEGAChatPeerList *)peers title:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createPublicChat(peers.getCPtr, title.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)createChatGroupWithPeers:(MEGAChatPeerList *)peers
                           title:(NSString *)title
                    speakRequest:(BOOL)speakRequest
                     waitingRoom:(BOOL)waitingRoom
                      openInvite:(BOOL)openInvite
                        delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createGroupChat(peers.getCPtr,
                                          title.UTF8String,
                                          speakRequest,
                                          waitingRoom,
                                          openInvite,
                                          [self createDelegateMEGAChatRequestListener:delegate
                                                                       singleListener:YES
                                                                            queueType:ListenerQueueTypeGlobalBackground]);
    }
}

- (void)createPublicChatWithPeers:(MEGAChatPeerList *)peers
                            title:(NSString *)title
                     speakRequest:(BOOL)speakRequest
                      waitingRoom:(BOOL)waitingRoom
                       openInvite:(BOOL)openInvite
                         delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createPublicChat(peers.getCPtr,
                                           title.UTF8String,
                                           speakRequest,
                                           waitingRoom,
                                           openInvite,
                                           [self createDelegateMEGAChatRequestListener:delegate
                                                                        singleListener:YES
                                                                             queueType:ListenerQueueTypeGlobalBackground]);
    }
}

- (void)createMeetingWithTitle:(NSString *)title {
    if (self.megaChatApi) {
        self.megaChatApi->createMeeting(title.UTF8String);
    }
}

- (void)createMeetingWithTitle:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createMeeting(title.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)createMeetingWithTitle:(NSString *)title
                  speakRequest:(BOOL)speakRequest
                   waitingRoom:(BOOL)waitingRoom
                    openInvite:(BOOL)openInvite
                      delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createMeeting(title.UTF8String,
                                        speakRequest,
                                        waitingRoom,
                                        openInvite,
                                        [self createDelegateMEGAChatRequestListener:delegate
                                                                     singleListener:YES
                                                                          queueType:ListenerQueueTypeGlobalBackground]);
    }
}


- (void)createPublicChatWithPeers:(MEGAChatPeerList *)peers title:(NSString *)title {
    if (self.megaChatApi) {
        self.megaChatApi->createPublicChat(peers.getCPtr, title.UTF8String);
    }
}

- (void)queryChatLink:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->queryChatLink(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)queryChatLink:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->queryChatLink(chatId);
    }
}

- (void)createChatLink:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->createChatLink(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)createChatLink:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->createChatLink(chatId);
    }
}

- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->inviteToChat(chatId, userHandle, (int)privilege, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege {
    if (self.megaChatApi) {
        self.megaChatApi->inviteToChat(chatId, userHandle, (int)privilege);
    }
}

- (void)autojoinPublicChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->autojoinPublicChat(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)autojoinPublicChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->autojoinPublicChat(chatId);
    }
}

- (void)autorejoinPublicChat:(uint64_t)chatId publicHandle:(uint64_t)publicHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->autorejoinPublicChat(chatId, publicHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)autorejoinPublicChat:(uint64_t)chatId publicHandle:(uint64_t)publicHandle {
    if (self.megaChatApi) {
        self.megaChatApi->autorejoinPublicChat(chatId, publicHandle);
    }}

- (void)removeFromChat:(uint64_t)chatId userHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->removeFromChat(chatId, userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)removeFromChat:(uint64_t)chatId userHandle:(uint64_t)userHandle {
    if (self.megaChatApi) {
        self.megaChatApi->removeFromChat(chatId, userHandle);
    }
}

- (void)leaveChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->leaveChat(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)leaveChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->leaveChat(chatId);
    }
}

- (void)updateChatPermissions:(uint64_t)chatId userHandle:(uint64_t)userHandle privilege:(NSInteger)privilege delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->updateChatPermissions(chatId, userHandle, (int)privilege, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)updateChatPermissions:(uint64_t)chatId userHandle:(uint64_t)userHandle privilege:(NSInteger)privilege {
    if (self.megaChatApi) {
        self.megaChatApi->updateChatPermissions(chatId, userHandle, (int)privilege);
    }
}

- (void)truncateChat:(uint64_t)chatId messageId:(uint64_t)messageId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->truncateChat(chatId, messageId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)truncateChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    if (self.megaChatApi) {
        self.megaChatApi->truncateChat(chatId, messageId);
    }
}

- (void)clearChatHistory:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->clearChatHistory(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)clearChatHistory:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->clearChatHistory(chatId);
    }
}

- (void)setChatTitle:(uint64_t)chatId title:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setChatTitle(chatId, title.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setChatTitle:(uint64_t)chatId title:(NSString *)title {
    if (self.megaChatApi) {
        self.megaChatApi->setChatTitle(chatId, title.UTF8String);
    }
}

- (void)openChatPreview:(NSURL *)link delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->openChatPreview(link.absoluteString.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)openChatPreview:(NSURL *)link {
    if (self.megaChatApi) {
        self.megaChatApi->openChatPreview(link.absoluteString.UTF8String);
    }
}

- (void)checkChatLink:(NSURL *)link delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->checkChatLink(link.absoluteString.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)checkChatLink:(NSURL *)link {
    if (self.megaChatApi) {
        self.megaChatApi->checkChatLink(link.absoluteString.UTF8String);
    }
}

- (void)setPublicChatToPrivate:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setPublicChatToPrivate(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setPublicChatToPrivate:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->setPublicChatToPrivate(chatId);
    }
}

-(void)removeChatLink:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->removeChatLink(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

-(void)removeChatLink:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->removeChatLink(chatId);
    }
}

- (void)archiveChat:(uint64_t)chatId archive:(BOOL)archive delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->archiveChat(chatId, archive, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)archiveChat:(uint64_t)chatId archive:(BOOL)archive {
    if (self.megaChatApi) {
        self.megaChatApi->archiveChat(chatId, archive);
    }
}

- (void)setChatRetentionTime:(uint64_t)chatID period:(NSUInteger)period delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setChatRetentionTime(chatID, (unsigned)period, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setChatRetentionTime:(uint64_t)chatID period:(NSUInteger)period {
    if (self.megaChatApi) {
        self.megaChatApi->setChatRetentionTime(chatID, (unsigned)period);
    }
}

- (BOOL)openChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    if (self.megaChatApi == nil) return NO;
    return self.megaChatApi->openChatRoom(chatId, [self createDelegateMEGAChatRoomListener:delegate singleListener:YES queueType:ListenerQueueTypeMain]);
}

- (void)closeChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    for (std::set<DelegateMEGAChatRoomListener *>::iterator it = _activeChatRoomListeners.begin() ; it != _activeChatRoomListeners.end() ; it++) {
        self.megaChatApi->closeChatRoom(chatId, (*it));
        if ((*it)->getUserListener() == delegate) {
            [self freeChatRoomListener:(*it)];
            break;
        }
    }
}

- (void)closeChatPreview:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->closeChatPreview(chatId);
    }
}

- (MEGAChatSource)loadMessagesForChat:(uint64_t)chatId count:(NSInteger)count {
    if (self.megaChatApi == nil) return MEGAChatSourceNone;
    return (MEGAChatSource) self.megaChatApi->loadMessages(chatId, (int)count);
}

- (BOOL)isFullHistoryLoadedForChat:(uint64_t)chatId {
    if (self.megaChatApi == nil) return NO;
    return self.megaChatApi->isFullHistoryLoaded(chatId);
}

- (MEGAChatMessage *)messageForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    if (self.megaChatApi == nil) return nil;
    MegaChatMessage *message = self.megaChatApi->getMessage(chatId, messageId);
    return message ? [[MEGAChatMessage alloc] initWithMegaChatMessage:message cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)messageFromNodeHistoryForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    if (self.megaChatApi == nil) return nil;
    MegaChatMessage *message = self.megaChatApi->getMessageFromNodeHistory(chatId, messageId);
    return message ? [[MEGAChatMessage alloc] initWithMegaChatMessage:message cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)sendMessageToChat:(uint64_t)chatId message:(NSString *)message {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->sendMessage(chatId, message.UTF8String) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)attachContactsToChat:(uint64_t)chatId contacts:(NSArray *)contacts {
    MEGAHandleList *handleList = [[MEGAHandleList alloc] init];
    
    for (NSInteger i = 0; i < contacts.count; i++) {
        [handleList addMegaHandle:[[contacts objectAtIndex:i] handle]];
    }
    
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->attachContacts(chatId, handleList.getCPtr) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)forwardContactFromChat:(uint64_t)sourceChatId messageId:(uint64_t)messageId targetChatId:(uint64_t)targetChatId {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->forwardContact(sourceChatId, messageId, targetChatId) cMemoryOwn:YES] : nil;
}

- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        MEGANodeList *nodeList = [[MEGANodeList alloc] init];
        NSUInteger count = nodesArray.count;
        for (NSUInteger i = 0; i < count; i++) {
            MEGANode *node = [nodesArray objectAtIndex:i];
            [nodeList addNode:node];
        }
        
        self.megaChatApi->attachNodes(chatId, nodeList.getCPtr, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray {
    if (self.megaChatApi) {
        MEGANodeList *nodeList = [[MEGANodeList alloc] init];
        NSUInteger count = nodesArray.count;
        for (NSUInteger i = 0; i < count; i++) {
            MEGANode *node = [nodesArray objectAtIndex:i];
            [nodeList addNode:node];
        }
        
        self.megaChatApi->attachNodes(chatId, nodeList.getCPtr);
    }
}

- (MEGAChatMessage *)sendGiphyToChat:(uint64_t)chatId srcMp4:(NSString *)srcMp4 srcWebp:(NSString *)srcWebp sizeMp4:(uint64_t)sizeMp4 sizeWebp:(uint64_t)sizeWebp  width:(int)width height:(int)height title:(NSString *)title {
    if (self.megaChatApi == nil) return nil;
    MegaChatMessage *message = self.megaChatApi->sendGiphy(chatId, srcMp4.UTF8String, srcWebp.UTF8String, sizeMp4, sizeWebp, width, height, title.UTF8String);
    return message ? [[MEGAChatMessage alloc] initWithMegaChatMessage:message cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)sendGeolocationToChat:(uint64_t)chatId longitude:(float)longitude latitude:(float)latitude image:(NSString *)image {
    if (self.megaChatApi == nil) return nil;
    MegaChatMessage *message = self.megaChatApi->sendGeolocation(chatId, longitude, latitude, image.UTF8String);
    return message ? [[MEGAChatMessage alloc] initWithMegaChatMessage:message cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)editGeolocationForChat:(uint64_t)chatId messageId:(uint64_t)messageId longitude:(float)longitude latitude:(float)latitude image:(NSString *)image {
    if (self.megaChatApi == nil) return nil;
    MegaChatMessage *message = self.megaChatApi->editGeolocation(chatId, messageId, longitude, latitude, image.UTF8String);
    return message ? [[MEGAChatMessage alloc] initWithMegaChatMessage:message cMemoryOwn:YES] : nil; 
}

- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->revokeAttachment(chatId, nodeHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle {
    if (self.megaChatApi) {
        self.megaChatApi->revokeAttachment(chatId, nodeHandle);
    }
}

- (void)attachNodeToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->attachNode(chatId, nodeHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)attachNodeToChat:(uint64_t)chatId node:(uint64_t)nodeHandle {
    if (self.megaChatApi) {
        self.megaChatApi->attachNode(chatId, nodeHandle);
    }
}

- (MEGAChatMessage *)revokeAttachmentMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->revokeAttachmentMessage(chatId, messageId) cMemoryOwn:YES] : nil;
}

- (BOOL)isRevokedNode:(uint64_t)nodeHandle inChat:(uint64_t)chatId {
    return self.megaChatApi ? self.megaChatApi->isRevoked(chatId, nodeHandle) : NO;
}

- (void)attachVoiceMessageToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->attachVoiceMessage(chatId, nodeHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)attachVoiceMessageToChat:(uint64_t)chatId node:(uint64_t)nodeHandle {
    if (self.megaChatApi) {
        self.megaChatApi->attachVoiceMessage(chatId, nodeHandle);
    }
}

- (MEGAChatMessage *)editMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId message:(NSString *)message {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->editMessage(chatId, messageId, message.UTF8String) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)deleteMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->deleteMessage(chatId, messageId) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)removeRichLinkForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->removeRichLink(chatId, messageId) cMemoryOwn:YES] : nil;
}

- (BOOL)setMessageSeenForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi ? self.megaChatApi->setMessageSeen(chatId, messageId) : NO;
}

- (MEGAChatMessage *)lastChatMessageSeenForChat:(uint64_t)chatId {
    if (self.megaChatApi == nil) return nil;
    MegaChatMessage *message = self.megaChatApi->getLastMessageSeen(chatId);
    return message ? [[MEGAChatMessage alloc] initWithMegaChatMessage:message cMemoryOwn:YES] : nil;
}

- (void)removeUnsentMessageForChat:(uint64_t)chatId rowId:(uint64_t)rowId {
    if (self.megaChatApi) {
        self.megaChatApi->removeUnsentMessage(chatId, rowId);
    }
}

- (void)addReactionForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction {
    if (self.megaChatApi) {
        self.megaChatApi->addReaction(chatId, messageId, reaction.UTF8String);
    }
}

- (void)deleteReactionForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction {
    if (self.megaChatApi) {
        self.megaChatApi->delReaction(chatId, messageId, reaction.UTF8String);
    }
}

- (NSInteger)messageReactionCountForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction {
    return self.megaChatApi ? self.megaChatApi->getMessageReactionCount(chatId, messageId, reaction.UTF8String) : 0;
}

- (MEGAStringList *)messageReactionsForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi ? [MEGAStringList.alloc initWithMegaStringList:self.megaChatApi->getMessageReactions(chatId, messageId) cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)reactionUsersForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction {
    return self.megaChatApi ? [MEGAHandleList.alloc initWithMegaHandleList:self.megaChatApi->getReactionUsers(chatId, messageId, reaction.UTF8String) cMemoryOwn:YES] : nil;
}

- (void)setPublicKeyPinning:(BOOL)enable {
    if (self.megaChatApi) {
        self.megaChatApi->setPublicKeyPinning(enable);
    }
}

- (void)sendTypingNotificationForChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->sendTypingNotification(chatId);
    }
}

- (void)sendStopTypingNotificationForChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->sendStopTypingNotification(chatId);
    }
}

- (void)saveCurrentState {
    if (self.megaChatApi) {
        self.megaChatApi->saveCurrentState();
    }
}

- (void)pushReceivedWithBeep:(BOOL)beep delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->pushReceived(beep, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)pushReceivedWithBeep:(BOOL)beep {
    if (self.megaChatApi) {
        self.megaChatApi->pushReceived(beep);
    }
}

- (void)pushReceivedWithBeep:(BOOL)beep chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->pushReceived(beep, chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)pushReceivedWithBeep:(BOOL)beep chatId:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->pushReceived(beep, chatId);
    }
}

- (void)openInvite:(BOOL)enabled chatId:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->setOpenInvite(chatId, enabled);
    }
}

- (void)openInvite:(BOOL)enabled chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setOpenInvite(chatId,
                                        enabled,
                                        [self createDelegateMEGAChatRequestListener:delegate
                                                                     singleListener:YES
                                                                          queueType:ListenerQueueTypeGlobalBackground]);
    }
}

- (BOOL)hasChatOptionEnabledForChatOption:(MEGAChatOption)option chatOptionsBitMask:(NSInteger)chatOptionsBitMask {
    if (self.megaChatApi) {
        return self.megaChatApi->hasChatOptionEnabled(option, chatOptionsBitMask);
    }
    return NO;
}

#pragma mark - Scheduled meetings

- (void)createChatAndScheduledMeeting:(BOOL)isMeeting publicChat:(BOOL)publicChat speakRequest:(BOOL)speakRequest waitingRoom:(BOOL)waitingRoom openInvite:(BOOL)openInvite timezone:(NSString *)timezone startDate:(NSString *)startDate endDate:(NSString *)endDate title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes {
    if (!self.megaChatApi) { return; }
    self.megaChatApi -> createChatAndScheduledMeeting(isMeeting, publicChat, speakRequest, waitingRoom, openInvite, timezone.UTF8String, startDate.UTF8String, endDate.UTF8String, title.UTF8String, description.UTF8String, MegaChatScheduledFlags::createInstance(emailsDisabled), MegaChatScheduledRules::createInstance(frequency), attributes.UTF8String);
}

- (void)createChatAndScheduledMeeting:(BOOL)isMeeting publicChat:(BOOL)publicChat speakRequest:(BOOL)speakRequest waitingRoom:(BOOL)waitingRoom openInvite:(BOOL)openInvite timezone:(NSString *)timezone startDate:(NSString *)startDate endDate:(NSString *)endDate title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) { return; }
    self.megaChatApi -> createChatAndScheduledMeeting(isMeeting, publicChat, speakRequest, waitingRoom, openInvite, timezone.UTF8String, startDate.UTF8String, endDate.UTF8String, title.UTF8String, description.UTF8String, MegaChatScheduledFlags::createInstance(emailsDisabled), MegaChatScheduledRules::createInstance(frequency), attributes.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)updateScheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId timezone:(NSString *)timezone title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->updateScheduledMeeting(chatId, scheduledId, timezone.UTF8String, title.UTF8String, description.UTF8String, MegaChatScheduledFlags::createInstance(emailsDisabled), MegaChatScheduledRules::createInstance(frequency), attributes.UTF8String);
}

- (void)updateScheduledMeeting:(uint64_t)chatId  scheduledId:(uint64_t)scheduledId timezone:(NSString *)timezone title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->updateScheduledMeeting(chatId, scheduledId, timezone.UTF8String, title.UTF8String, description.UTF8String, MegaChatScheduledFlags::createInstance(emailsDisabled), MegaChatScheduledRules::createInstance(frequency), attributes.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)updateScheduledMeetingOccurrence:(uint64_t)chatId  scheduledId:(uint64_t)scheduledId overrides:(NSString *)overrides newStartDate:(NSString *)newStartDate newEndDate:(NSString *)newEndDate newCancelled:(BOOL)newCancelled {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->updateScheduledMeetingOccurrence(chatId, scheduledId, overrides.UTF8String, newStartDate.UTF8String, newEndDate.UTF8String, newCancelled);
}

- (void)updateScheduledMeetingOccurrence:(uint64_t)chatId  scheduledId:(uint64_t)scheduledId overrides:(NSString *)overrides newStartDate:(NSString *)newStartDate newEndDate:(NSString *)newEndDate newCancelled:(BOOL)newCancelled delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->updateScheduledMeetingOccurrence(chatId, scheduledId, overrides.UTF8String, newStartDate.UTF8String, newEndDate.UTF8String, newCancelled, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)removeScheduledMeeting:(uint64_t)chatId  scheduledId:(uint64_t)scheduledId {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->removeScheduledMeeting(chatId, scheduledId);
}

- (void)removeScheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->removeScheduledMeeting(chatId, scheduledId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (NSArray<MEGAChatScheduledMeeting *> *)scheduledMeetingsByChat:(uint64_t)chatId {
    if (!self.megaChatApi) { return [NSArray new]; }
    MegaChatScheduledMeetingList *scheduledMeetingsList = self.megaChatApi->getScheduledMeetingsByChat(chatId);
    NSMutableArray<MEGAChatScheduledMeeting *> *scheduledMeetings = [NSMutableArray arrayWithCapacity:scheduledMeetingsList->size()];

    for (int i = 0; i < scheduledMeetingsList->size(); i++)
    {
        MegaChatScheduledMeeting *megaChatScheduledMeeting = scheduledMeetingsList->at(i)->copy();

        MEGAChatScheduledMeeting *scheduledMeeting = [[MEGAChatScheduledMeeting alloc] initWithMegaChatScheduledMeeting: megaChatScheduledMeeting cMemoryOwn:YES];
        [scheduledMeetings addObject:scheduledMeeting];
    }

    delete scheduledMeetingsList;
    return scheduledMeetings;
}

- (MEGAChatScheduledMeeting *)scheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId {
    if (!self.megaChatApi) { return nil; }
    MegaChatScheduledMeeting *megaChatScheduledMeeting = self.megaChatApi->getScheduledMeeting(chatId, scheduledId);
    return [[MEGAChatScheduledMeeting alloc] initWithMegaChatScheduledMeeting:megaChatScheduledMeeting cMemoryOwn:YES];
}

- (NSArray<MEGAChatScheduledMeeting *> *)getAllScheduledMeetings {
    if (!self.megaChatApi) { return nil; }
    MegaChatScheduledMeetingList *scheduledMeetingsList = self.megaChatApi->getAllScheduledMeetings();;
    NSMutableArray<MEGAChatScheduledMeeting *> *scheduledMeetings = [NSMutableArray arrayWithCapacity:scheduledMeetingsList->size()];

    for (int i = 0; i < scheduledMeetingsList->size(); i++)
    {
        MegaChatScheduledMeeting *megaChatScheduledMeeting = scheduledMeetingsList->at(i)->copy();

        MEGAChatScheduledMeeting *scheduledMeeting = [[MEGAChatScheduledMeeting alloc] initWithMegaChatScheduledMeeting: megaChatScheduledMeeting cMemoryOwn:YES];
        [scheduledMeetings addObject:scheduledMeeting];
    }

    delete scheduledMeetingsList;
    return scheduledMeetings;
}

- (void)fetchScheduledMeetingOccurrencesByChat:(uint64_t)chatId {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->fetchScheduledMeetingOccurrencesByChat(chatId);
}

- (void)fetchScheduledMeetingOccurrencesByChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) { return; }
    self.megaChatApi->fetchScheduledMeetingOccurrencesByChat(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

#pragma mark - Audio and video calls

#ifndef KARERE_DISABLE_WEBRTC

- (MEGAStringList *)chatVideoInDevices {
    return self.megaChatApi ? [[MEGAStringList alloc] initWithMegaStringList:self.megaChatApi->getChatVideoInDevices() cMemoryOwn:YES] : nil;
}

- (void)setChatVideoInDevices:(NSString *)devices {
    if (self.megaChatApi) {
        return self.megaChatApi->setChatVideoInDevice(devices.UTF8String);
    }
}

- (void)setChatVideoInDevices:(NSString *)devices delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        return self.megaChatApi->setChatVideoInDevice(devices.UTF8String, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (NSString *)videoDeviceSelected {
    if (self.megaChatApi == nil) return nil;
    char *selectedVideoDevice = self.megaChatApi->getVideoDeviceSelected();
    if (!selectedVideoDevice) return nil;
    NSString *selectedVideoDeviceString = [NSString.alloc initWithUTF8String:selectedVideoDevice];
    delete selectedVideoDevice;
    return selectedVideoDeviceString;
}

- (void)startChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->startChatCall(chatId, enableVideo, enableAudio ,[self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)startChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio {
    if (self.megaChatApi) {
        self.megaChatApi->startChatCall(chatId, enableVideo, enableAudio);
    }
}

- (void)answerChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->answerChatCall(chatId, enableVideo, enableAudio, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)answerChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo {
    if (self.megaChatApi) {
        self.megaChatApi->answerChatCall(chatId, enableVideo);
    }
}

-(void)hangChatCall:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->hangChatCall(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

-(void)hangChatCall:(uint64_t)callId {
    if (self.megaChatApi) {
        self.megaChatApi->hangChatCall(callId);
    }
}

- (void)endChatCall:(uint64_t)callId {
    if (self.megaChatApi) {
        self.megaChatApi->endChatCall(callId);
    }
}

- (void)enableAudioForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->enableAudio(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)enableAudioForChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->enableAudio(chatId);
    }
}

- (void)disableAudioForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->disableAudio(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)disableAudioForChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->disableAudio(chatId);
    }
}

- (void)enableVideoForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->enableVideo(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)enableVideoForChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->enableVideo(chatId);
    }
}

- (void)disableVideoForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->disableVideo(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)disableVideoForChat:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->disableVideo(chatId);
    }
}

- (void)setCallOnHoldForChat:(uint64_t)chatId onHold:(BOOL)onHold delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->setCallOnHold(chatId, onHold, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)setCallOnHoldForChat:(uint64_t)chatId onHold:(BOOL)onHold {
    if (self.megaChatApi) {
        self.megaChatApi->setCallOnHold(chatId, onHold);
    }
}

- (void)openVideoDevice {
    if (self.megaChatApi) {
        self.megaChatApi->openVideoDevice();
    }
}

- (void)openVideoDeviceWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->openVideoDevice([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)releaseVideoDevice {
    if (self.megaChatApi) {
        self.megaChatApi->releaseVideoDevice();
    }
}

- (void)releaseVideoDeviceWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->releaseVideoDevice([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (MEGAChatCall *)chatCallForCallId:(uint64_t)callId {
    if (self.megaChatApi == nil) return nil;
    MegaChatCall *chatCall = self.megaChatApi->getChatCallByCallId(callId);
    return chatCall ? [[MEGAChatCall alloc] initWithMegaChatCall:chatCall cMemoryOwn:YES] : nil;
}

- (MEGAChatCall *)chatCallForChatId:(uint64_t)chatId {
    if (self.megaChatApi != nil && self.megaChatApi->hasCallInChatRoom(chatId)) {
        MegaChatCall *chatCall = self.megaChatApi->getChatCall(chatId);
        return chatCall ? [[MEGAChatCall alloc] initWithMegaChatCall:chatCall cMemoryOwn:YES] : nil;
    }
    
    return nil;
}

- (NSInteger)numCalls {
    return self.megaChatApi ? self.megaChatApi->getNumCalls() : 0;
}

- (MEGAHandleList *)chatCallsWithState:(MEGAChatCallStatus)callState {
    return self.megaChatApi ? [MEGAHandleList.alloc initWithMegaHandleList:self.megaChatApi->getChatCalls((int)callState) cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)chatCallsIds {
    return self.megaChatApi ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatApi->getChatCallsIds() cMemoryOwn:YES] : nil;
}

- (BOOL)hasCallInChatRoom:(uint64_t)chatId {
    return self.megaChatApi ? self.megaChatApi->hasCallInChatRoom(chatId) : NO;
}

- (NSInteger)getMaxVideoCallParticipants {
    return self.megaChatApi ? self.megaChatApi->getMaxVideoCallParticipants() : 0;
}

- (NSInteger)getMaxCallParticipants {
    return self.megaChatApi ? self.megaChatApi->getMaxCallParticipants() : 0;
}

- (uint64_t)myClientIdHandleForChatId:(uint64_t)chatId {
    return self.megaChatApi ? self.megaChatApi->getMyClientidHandle(chatId) : MEGACHAT_INVALID_HANDLE;
}

- (BOOL)isAudioLevelMonitorEnabledForChatId:(uint64_t)chatId {
    return self.megaChatApi ? self.megaChatApi->isAudioLevelMonitorEnabled(chatId) : NO;
}

- (void)enableAudioMonitor:(BOOL)enable chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->enableAudioLevelMonitor(enable, chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)enableAudioMonitor:(BOOL)enable chatId:(uint64_t)chatId {
    if (self.megaChatApi) {
        self.megaChatApi->enableAudioLevelMonitor(enable, chatId);
    }
}

- (void)requestHiResVideoForChatId:(uint64_t)chatId clientId:(uint64_t)clientId delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->requestHiResVideo(chatId, clientId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
    }
}

- (void)stopHiResVideoForChatId:(uint64_t)chatId clientIds:(NSArray<NSNumber *> *)clientIds delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) return;
    MEGAHandleList *clientIdList = [MEGAHandleList.alloc initWithMemoryOwn:YES];
    for (NSNumber *handle in clientIds) {
        [clientIdList addMegaHandle:handle.unsignedLongLongValue];
    }
    self.megaChatApi->stopHiResVideo(chatId, clientIdList.getCPtr, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)requestLowResVideoForChatId:(uint64_t)chatId clientIds:(NSArray<NSNumber *> *)clientIds delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) return;
    MEGAHandleList *clientIdList = [MEGAHandleList.alloc initWithMemoryOwn:YES];
    for (NSNumber *handle in clientIds) {
        [clientIdList addMegaHandle:handle.unsignedLongLongValue];
    }
    self.megaChatApi->requestLowResVideo(chatId, clientIdList.getCPtr, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)stopLowResVideoForChatId:(uint64_t)chatId clientIds:(NSArray<NSNumber *> *)clientIds delegate:(id<MEGAChatRequestDelegate>)delegate {
    if (!self.megaChatApi) return;
    MEGAHandleList *clientIdList = [MEGAHandleList.alloc initWithMemoryOwn:YES];
    for (NSNumber *handle in clientIds) {
        [clientIdList addMegaHandle:handle.unsignedLongLongValue];
    }
    self.megaChatApi->stopLowResVideo(chatId, clientIdList.getCPtr, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

#endif

#pragma mark - Debug log messages

+ (void)setLogLevel:(MEGAChatLogLevel)level {
    MegaChatApi::setLogLevel((int)level);
}

+ (void)setLogToConsole:(BOOL)enable {
    MegaChatApi::setLogToConsole(enable);
}

+ (void)setLogObject:(id<MEGAChatLoggerDelegate>)delegate {
    if (delegate) {
        DelegateMEGAChatLoggerListener *newLogger = new DelegateMEGAChatLoggerListener(delegate);
        delete externalLogger;
        externalLogger = newLogger;
    } else {
        MegaChatApi::setLoggerObject(NULL);
    }
}

+ (void)setLogWithColors:(BOOL)userColors {
    MegaChatApi::setLogWithColors(userColors);
}

#pragma mark - Private methods

- (MegaChatRequestListener *)createDelegateMEGAChatRequestListener:(id<MEGAChatRequestDelegate>)delegate singleListener:(BOOL)singleListener {
    return [self createDelegateMEGAChatRequestListener:delegate singleListener:singleListener queueType:ListenerQueueTypeMain];
}

- (MegaChatRequestListener *)createDelegateMEGAChatRequestListener:(id<MEGAChatRequestDelegate>)delegate singleListener:(BOOL)singleListener queueType:(ListenerQueueType)queueType {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatRequestListener *delegateListener = new DelegateMEGAChatRequestListener(self, delegate, singleListener, queueType);
    pthread_mutex_lock(&listenerMutex);
    _activeRequestListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (void)freeRequestListener:(DelegateMEGAChatRequestListener *)delegate {
    if (delegate == nil) return;
    
    pthread_mutex_lock(&listenerMutex);
    _activeRequestListeners.erase(delegate);
    pthread_mutex_unlock(&listenerMutex);
    delete delegate;
}


- (MegaChatRoomListener *)createDelegateMEGAChatRoomListener:(id<MEGAChatRoomDelegate>)delegate singleListener:(BOOL)singleListener queueType:(ListenerQueueType)queueType {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatRoomListener *delegateListener = new DelegateMEGAChatRoomListener(self, delegate, singleListener, queueType);
    pthread_mutex_lock(&listenerMutex);
    _activeChatRoomListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (void)freeChatRoomListener:(DelegateMEGAChatRoomListener *)delegate {
    if (delegate == nil) return;
    
    pthread_mutex_lock(&listenerMutex);
    _activeChatRoomListeners.erase(delegate);
    pthread_mutex_unlock(&listenerMutex);
    delete delegate;
}

- (MegaChatListener *)createDelegateMEGAChatListener:(id<MEGAChatDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatListener *delegateListener = new DelegateMEGAChatListener(self, delegate, singleListener);
    pthread_mutex_lock(&listenerMutex);
    _activeChatListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (void)freeChatListener:(DelegateMEGAChatListener *)delegate {
    if (delegate == nil) return;
    
    pthread_mutex_lock(&listenerMutex);
    _activeChatListeners.erase(delegate);
    pthread_mutex_unlock(&listenerMutex);
    delete delegate;
}

- (MegaChatCallListener *)createDelegateMEGAChatCallListener:(id<MEGAChatCallDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatCallListener *delegateListener = new DelegateMEGAChatCallListener(self, delegate, singleListener);
    pthread_mutex_lock(&listenerMutex);
    _activeChatCallListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (void)freeChatCallListener:(DelegateMEGAChatCallListener *)delegate {
    if (delegate == nil) return;
    
    pthread_mutex_lock(&listenerMutex);
    _activeChatCallListeners.erase(delegate);
    pthread_mutex_unlock(&listenerMutex);
    delete delegate;
}

- (MegaChatVideoListener *)createDelegateMEGAChatLocalVideoListener:(id<MEGAChatVideoDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatVideoListener *delegateListener = new DelegateMEGAChatVideoListener(self, delegate, singleListener);
    pthread_mutex_lock(&listenerMutex);
    _activeChatLocalVideoListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (void)freeChatLocalVideoListener:(DelegateMEGAChatVideoListener *)delegate {
    if (delegate == nil) return;
    
    pthread_mutex_lock(&listenerMutex);
    _activeChatLocalVideoListeners.erase(delegate);
    pthread_mutex_unlock(&listenerMutex);
    delete delegate;
}

- (MegaChatVideoListener *)createDelegateMEGAChatRemoteVideoListener:(id<MEGAChatVideoDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatVideoListener *delegateListener = new DelegateMEGAChatVideoListener(self, delegate, singleListener);
    pthread_mutex_lock(&listenerMutex);
    _activeChatRemoteVideoListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (void)freeChatRemoteVideoListener:(DelegateMEGAChatVideoListener *)delegate {
    if (delegate == nil) return;
    
    pthread_mutex_lock(&listenerMutex);
    _activeChatRemoteVideoListeners.erase(delegate);
    pthread_mutex_unlock(&listenerMutex);
    delete delegate;
}

- (MegaChatNotificationListener *)createDelegateMEGAChatNotificationListener:(id<MEGAChatNotificationDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatNotificationListener *delegateListener = new DelegateMEGAChatNotificationListener(self, delegate, singleListener);
    pthread_mutex_lock(&listenerMutex);
    _activeChatNotificationListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (MegaChatNodeHistoryListener *)createDelegateMEGAChatNodeHistoryListener:(id<MEGAChatNodeHistoryDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatNodeHistoryListener *delegateListener = new DelegateMEGAChatNodeHistoryListener(self, delegate, singleListener);
    pthread_mutex_lock(&listenerMutex);
    _activeChatNodeHistoryListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

- (MegaChatScheduledMeetingListener *)createDelegateMEGAChatScheduledMeetingListener:(id<MEGAChatScheduledMeetingDelegate>)delegate singleListener:(BOOL)singleListener {
    return [self createDelegateMEGAChatScheduledMeetingListener:delegate singleListener:singleListener queueType:ListenerQueueTypeMain];
}

- (MegaChatScheduledMeetingListener *)createDelegateMEGAChatScheduledMeetingListener:(id<MEGAChatScheduledMeetingDelegate>)delegate singleListener:(BOOL)singleListener  queueType:(ListenerQueueType)queueType {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatScheduledMeetingListener *delegateListener = new DelegateMEGAChatScheduledMeetingListener(self, delegate, singleListener, queueType);
    pthread_mutex_lock(&listenerMutex);
    _activeChatScheduledMeetingListeners.insert(delegateListener);
    pthread_mutex_unlock(&listenerMutex);
    return delegateListener;
}

#pragma mark - Exceptions

+ (void)setCatchException:(BOOL)enable {
    MegaChatApi::setCatchException(enable);
}

#pragma mark - Rich links

+ (BOOL)hasUrl:(NSString *)text {
    return MegaChatApi::hasUrl(text.UTF8String);
}

#pragma mark - Node history

- (BOOL)openNodeHistoryForChat:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate {
    return self.megaChatApi ? self.megaChatApi->openNodeHistory(chatId, [self createDelegateMEGAChatNodeHistoryListener:delegate singleListener:YES]) : NO;
}

- (BOOL)closeNodeHistoryForChat:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate {
    return self.megaChatApi ? self.megaChatApi->closeNodeHistory(chatId, [self createDelegateMEGAChatNodeHistoryListener:delegate singleListener:YES]) : NO;
}

- (void)addNodeHistoryDelegate:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate {
    if (self.megaChatApi) {
        self.megaChatApi->addNodeHistoryListener(chatId, [self createDelegateMEGAChatNodeHistoryListener:delegate singleListener:NO]);
    }
}

- (void)removeNodeHistoryDelegate:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate {
    
    std::vector<DelegateMEGAChatNodeHistoryListener *> listenersToRemove;
    
    pthread_mutex_lock(&listenerMutex);
    std::set<DelegateMEGAChatNodeHistoryListener *>::iterator it = _activeChatNodeHistoryListeners.begin();
    while (it != _activeChatNodeHistoryListeners.end()) {
        DelegateMEGAChatNodeHistoryListener *delegateListener = *it;
        if (delegateListener->getUserListener() == delegate) {
            listenersToRemove.push_back(delegateListener);
            _activeChatNodeHistoryListeners.erase(it++);
        }
        else {
            it++;
        }
    }
    pthread_mutex_unlock(&listenerMutex);
    
    for (int i = 0; i < listenersToRemove.size(); i++)
    {
        if (self.megaChatApi) {
            self.megaChatApi->removeNodeHistoryListener(chatId, (listenersToRemove[i]));
        }
        delete listenersToRemove[i];
    }
}

- (MEGAChatSource)loadAttachmentsForChat:(uint64_t)chatId count:(NSInteger)count {
    return self.megaChatApi ? MEGAChatSource(self.megaChatApi->loadAttachments(chatId, (int)count)) : MEGAChatSourceError;
}

#pragma mark - Enumeration to NSString

+ (NSString *)stringForMEGAChatInitState:(MEGAChatInit)initState {
    NSString *ret;
    switch (initState) {
        case MEGAChatInitError:
            ret = @"MEGA chat init state error";
            break;
            
        case MEGAChatInitNotDone:
            ret = @"MEGA chat init not done";
            break;
            
        case MEGAChatInitWaitingNewSession:
            ret = @"MEGA chat init state waiting new session";
            break;
            
        case MEGAChatInitOfflineSession:
            ret = @"MEGA chat init state offline session";
            break;
            
        case MEGAChatInitOnlineSession:
            ret = @"MEGA chat init state online session";
            break;
            
        case MEGAChatInitAnonymous:
            ret = @"MEGA chat init state anonymous";
            break;
            
        case MEGAChatInitNoCache:
            ret = @"MEGA chat init state no cache";
            break;
            
        default:
            ret = @"MEGA chat init state default";
            break;
    }
    
    return ret;
}

@end
