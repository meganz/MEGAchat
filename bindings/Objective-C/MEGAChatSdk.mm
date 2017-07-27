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
#import "MEGAChatPresenceConfig+init.h"
#import "MEGANodeList+init.h"
#import "MEGAHandleList+init.h"
#import "DelegateMEGAChatRequestListener.h"
#import "DelegateMEGAChatLoggerListener.h"
#import "DelegateMEGAChatRoomListener.h"
#import "DelegateMEGAChatListener.h"

#import <set>
#import <pthread.h>

using namespace megachat;

@interface MEGAChatSdk () {
    pthread_mutex_t listenerMutex;
}

@property (nonatomic, assign) std::set<DelegateMEGAChatRequestListener *>activeRequestListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatRoomListener *>activeChatRoomListeners;
@property (nonatomic, assign) std::set<DelegateMEGAChatListener *>activeChatListeners;

- (MegaChatRequestListener *)createDelegateMEGAChatRequestListener:(id<MEGAChatRequestDelegate>)delegate singleListener:(BOOL)singleListener;

@property MegaChatApi *megaChatApi;
- (MegaChatApi *)getCPtr;

@end

@implementation MEGAChatSdk

static DelegateMEGAChatLoggerListener *externalLogger = NULL;

#pragma mark - Init

- (instancetype)init:(MEGASdk *)megaSDK {
    
    if (!externalLogger) {
        externalLogger = new DelegateMEGAChatLoggerListener(nil);
    }
    
    self.megaChatApi = new MegaChatApi((mega::MegaApi *)[megaSDK getCPtr]);
    
    if (pthread_mutex_init(&listenerMutex, NULL)) {
        return nil;
    }
    
    return self;
}

- (MEGAChatInit)initKarereWithSid:(NSString *)sid {
    return (MEGAChatInit) self.megaChatApi->init((sid != nil) ? [sid UTF8String] : NULL);
}

- (MEGAChatInit)initState {
    return (MEGAChatInit) self.megaChatApi->getInitState();
}

- (void)connectWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->connect([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)connect {
    self.megaChatApi->connect();
}

- (void)disconnectWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->disconnect([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)disconnect {
    self.megaChatApi->disconnect();
}

- (void)retryPendingConnections {
    self.megaChatApi->retryPendingConnections();
}

- (void)dealloc {
    delete _megaChatApi;
    pthread_mutex_destroy(&listenerMutex);
}

- (MegaChatApi *)getCPtr {
    return _megaChatApi;
}

- (uint64_t)myUserHandle {
    return self.megaChatApi->getMyUserHandle();
}

#pragma mark - Logout

- (void)logoutWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->logout([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)logout {
    self.megaChatApi->logout();
}

- (void)localLogoutWithDelegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->localLogout([self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)localLogout {
    self.megaChatApi->localLogout();
}

#pragma mark - Presence

- (void)setOnlineStatus:(MEGAChatStatus)status delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->setOnlineStatus((int)status, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)setOnlineStatus:(MEGAChatStatus)status {
    self.megaChatApi->setOnlineStatus((int)status);
}

- (MEGAChatStatus)onlineStatus {
    return (MEGAChatStatus)self.megaChatApi->getOnlineStatus();
}

- (void)setPresenceAutoaway:(BOOL)enable timeout:(NSInteger)timeout {
    self.megaChatApi->setPresenceAutoaway(enable, (int)timeout);
}

- (void)setPresencePersist:(BOOL)enable {
    self.megaChatApi->setPresencePersist(enable);
}

- (BOOL)isSignalActivityRequired {
    return self.megaChatApi->isSignalActivityRequired();
}

- (void)signalPresenceActivity {
    self.megaChatApi->signalPresenceActivity();
}

- (MEGAChatPresenceConfig *)presenceConfig {
    return self.megaChatApi ? [[MEGAChatPresenceConfig alloc] initWithMegaChatPresenceConfig:self.megaChatApi->getPresenceConfig() cMemoryOwn:YES] : nil;
}

- (MEGAChatStatus)userOnlineStatus:(uint64_t)userHandle {
    return (MEGAChatStatus)self.megaChatApi->getUserOnlineStatus(userHandle);
}

- (void)setBackgroundStatus:(BOOL)status delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->setBackgroundStatus(status, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)setBackgroundStatus:(BOOL)status {
    self.megaChatApi->setBackgroundStatus(status);
}

#pragma mark - Add and remove delegates

- (void)addChatRoomDelegate:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    self.megaChatApi->addChatRoomListener(chatId, [self createDelegateMEGAChatRoomListener:delegate singleListener:NO]);
}

- (void)removeChatRoomDelegate:(id<MEGAChatRoomDelegate>)delegate {    
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
        self.megaChatApi->removeChatRoomListener(listenersToRemove[i]);
        delete listenersToRemove[i];
    }
}

- (void)addChatRequestDelegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->addChatRequestListener([self createDelegateMEGAChatRequestListener:delegate singleListener:NO]);
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
        self.megaChatApi->removeChatRequestListener(listenersToRemove[i]);
        delete listenersToRemove[i];
    }
}

- (void)addChatDelegate:(id<MEGAChatDelegate>)delegate {
    self.megaChatApi->addChatListener([self createDelegateMEGAChatListener:delegate singleListener:NO]);
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
        self.megaChatApi->removeChatListener(listenersToRemove[i]);
        delete listenersToRemove[i];
    }
}

#pragma mark - My user attributes

- (NSString *)myFirstname {
    char *val = self.megaChatApi->getMyFirstname();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)myLastname {
    char *val = self.megaChatApi->getMyLastname();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)myFullname {
    char *val = self.megaChatApi->getMyFullname();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)myEmail {
    char *val = self.megaChatApi->getMyEmail();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

#pragma mark - Chat rooms and chat list items

- (MEGAChatRoomList *)chatRooms {
    return [[MEGAChatRoomList alloc] initWithMegaChatRoomList:self.megaChatApi->getChatRooms() cMemoryOwn:YES];
}

- (MEGAChatRoom *)chatRoomForChatId:(uint64_t)chatId {
    return [[MEGAChatRoom alloc] initWithMegaChatRoom:self.megaChatApi->getChatRoom(chatId) cMemoryOwn:YES];
}

- (MEGAChatRoom *)chatRoomByUser:(uint64_t)userHandle {
    return self.megaChatApi->getChatRoomByUser(userHandle) ? [[MEGAChatRoom alloc] initWithMegaChatRoom:self.megaChatApi->getChatRoomByUser(userHandle) cMemoryOwn:YES] : nil;
}

- (MEGAChatListItemList *)chatListItems {
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getChatListItems() cMemoryOwn:YES];
}

- (NSInteger)unreadChats {
    return self.megaChatApi->getUnreadChats();
}

- (MEGAChatListItemList *)activeChatListItems {
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getActiveChatListItems() cMemoryOwn:YES];
}

- (MEGAChatListItemList *)inactiveChatListItems {
    return [[MEGAChatListItemList alloc] initWithMegaChatListItemList:self.megaChatApi->getInactiveChatListItems() cMemoryOwn:YES];
}

- (MEGAChatListItem *)chatListItemForChatId:(uint64_t)chatId {
    return self.megaChatApi->getChatListItem(chatId) ? [[MEGAChatListItem alloc] initWithMegaChatListItem:self.megaChatApi->getChatListItem(chatId) cMemoryOwn:YES] : nil;
}

- (uint64_t)chatIdByUserHandle:(uint64_t)userHandle {
    return self.megaChatApi->getChatHandleByUser(userHandle);
}

#pragma mark - Users attributes

- (void)userEmailByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->getUserEmail(userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)userEmailByUserHandle:(uint64_t)userHandle {
    self.megaChatApi->getUserEmail(userHandle);
}

- (void)userFirstnameByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->getUserFirstname(userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)userFirstnameByUserHandle:(uint64_t)userHandle {
    self.megaChatApi->getUserFirstname(userHandle);
}

- (void)userLastnameByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->getUserLastname(userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)userLastnameByUserHandle:(uint64_t)userHandle {
    self.megaChatApi->getUserLastname(userHandle);
}

- (NSString *)contacEmailByHandle:(uint64_t)userHandle {
    const char *val = self.megaChatApi->getContactEmail(userHandle);
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

#pragma mark - Chat management

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->createChat(group, peers ? [peers getCPtr] : NULL, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers {
    self.megaChatApi->createChat(group, peers ? [peers getCPtr] : NULL);
}

- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->inviteToChat(chatId, userHandle, (int)privilege, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege {
    self.megaChatApi->inviteToChat(chatId, userHandle, (int)privilege);
}

- (void)removeFromChat:(uint64_t)chatId userHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->removeFromChat(chatId, userHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)removeFromChat:(uint64_t)chatId userHandle:(uint64_t)userHandle {
    self.megaChatApi->removeFromChat(chatId, userHandle);
}

- (void)leaveChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->leaveChat(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)leaveChat:(uint64_t)chatId {
    self.megaChatApi->leaveChat(chatId);
}

- (void)updateChatPermissions:(uint64_t)chatId userHandle:(uint64_t)userHandle privilege:(NSInteger)privilege delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->updateChatPermissions(chatId, userHandle, (int)privilege, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)updateChatPermissions:(uint64_t)chatId userHandle:(uint64_t)userHandle privilege:(NSInteger)privilege {
    self.megaChatApi->updateChatPermissions(chatId, userHandle, (int)privilege);
}

- (void)truncateChat:(uint64_t)chatId messageId:(uint64_t)messageId delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->truncateChat(chatId, messageId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)truncateChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    self.megaChatApi->truncateChat(chatId, messageId);
}

- (void)clearChatHistory:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->clearChatHistory(chatId, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)clearChatHistory:(uint64_t)chatId {
    self.megaChatApi->clearChatHistory(chatId);
}

- (void)setChatTitle:(uint64_t)chatId title:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->setChatTitle(chatId, title ? [title UTF8String] : NULL, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)setChatTitle:(uint64_t)chatId title:(NSString *)title {
    self.megaChatApi->setChatTitle(chatId, title ? [title UTF8String] : NULL);
}

- (BOOL)openChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    return self.megaChatApi->openChatRoom(chatId, [self createDelegateMEGAChatRoomListener:delegate singleListener:YES]);
}

- (void)closeChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate {
    for (std::set<DelegateMEGAChatRoomListener *>::iterator it = _activeChatRoomListeners.begin() ; it != _activeChatRoomListeners.end() ; it++) {        
        if ((*it)->getUserListener() == delegate) {
            self.megaChatApi->closeChatRoom(chatId, (*it));
            [self freeChatRoomListener:(*it)];
            break;
        }
    }
}

- (MEGAChatSource)loadMessagesForChat:(uint64_t)chatId count:(NSInteger)count {
    return (MEGAChatSource) self.megaChatApi->loadMessages(chatId, (int)count);
}

- (BOOL)isFullHistoryLoadedForChat:(uint64_t)chatId {
    return self.megaChatApi->isFullHistoryLoaded(chatId);
}

- (MEGAChatMessage *)messageForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi->getMessage(chatId, messageId) ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->getMessage(chatId, messageId) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)sendMessageToChat:(uint64_t)chatId message:(NSString *)message {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->sendMessage(chatId, message ? [message UTF8String] : NULL) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)attachContactsToChat:(uint64_t)chatId contacts:(NSArray *)contacts {
    MEGAHandleList *handleList = [[MEGAHandleList alloc] init];
    
    for (NSInteger i = 0; i < contacts.count; i++) {
        [handleList addMegaHandle:[[contacts objectAtIndex:i] handle]];
    }
    
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->attachContacts(chatId, handleList ? [handleList getCPtr] : NULL) cMemoryOwn:YES] : nil;
}

- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray delegate:(id<MEGAChatRequestDelegate>)delegate {
    MEGANodeList *nodeList = [[MEGANodeList alloc] init];
    NSUInteger count = nodesArray.count;
    for (NSUInteger i = 0; i < count; i++) {
        MEGANode *node = [nodesArray objectAtIndex:i];
        [nodeList addNode:node];
    }
    
    self.megaChatApi->attachNodes(chatId, (nodeList != nil) ? [nodeList getCPtr] : NULL, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray {
    MEGANodeList *nodeList = [[MEGANodeList alloc] init];
    NSUInteger count = nodesArray.count;
    for (NSUInteger i = 0; i < count; i++) {
        MEGANode *node = [nodesArray objectAtIndex:i];
        [nodeList addNode:node];
    }
    
    self.megaChatApi->attachNodes(chatId, (nodeList != nil) ? [nodeList getCPtr] : NULL);
}

- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate {
    self.megaChatApi->revokeAttachment(chatId, nodeHandle, [self createDelegateMEGAChatRequestListener:delegate singleListener:YES]);
}

- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle {
    self.megaChatApi->revokeAttachment(chatId, nodeHandle);
}

- (BOOL)isRevokedNode:(uint64_t)nodeHandle inChat:(uint64_t)chatId {
    return self.megaChatApi->isRevoked(chatId, nodeHandle);
}

- (MEGAChatMessage *)editMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId message:(NSString *)message {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->editMessage(chatId, messageId, message ? [message UTF8String] : NULL) cMemoryOwn:YES] : nil;
}

- (MEGAChatMessage *)deleteMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->deleteMessage(chatId, messageId) cMemoryOwn:YES] : nil;
}

- (BOOL)setMessageSeenForChat:(uint64_t)chatId messageId:(uint64_t)messageId {
    return self.megaChatApi->setMessageSeen(chatId, messageId);
}

- (MEGAChatMessage *)lastChatMessageSeenForChat:(uint64_t)chatId {
    return self.megaChatApi->getLastMessageSeen(chatId) ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatApi->getLastMessageSeen(chatId) cMemoryOwn:YES] : nil;
}

- (void)removeUnsentMessageForChat:(uint64_t)chatId rowId:(uint64_t)rowId {
    self.megaChatApi->removeUnsentMessage(chatId, rowId);
}

- (void)sendTypingNotificationForChat:(uint64_t)chatId {
    self.megaChatApi->sendTypingNotification(chatId);
}

#pragma mark - Debug log messages

+ (void)setLogLevel:(MEGAChatLogLevel)level {
    MegaChatApi::setLogLevel((int)level);
}

+ (void)setLogToConsole:(BOOL)enable {
    MegaChatApi::setLogToConsole(enable);
}

+ (void)setLogObject:(id<MEGAChatLoggerDelegate>)delegate {
    DelegateMEGAChatLoggerListener *newLogger = new DelegateMEGAChatLoggerListener(delegate);
    delete externalLogger;
    externalLogger = newLogger;
}

+ (void)setLogWithColors:(BOOL)userColors {
    MegaChatApi::setLogWithColors(userColors);
}

#pragma mark - Private methods

- (MegaChatRequestListener *)createDelegateMEGAChatRequestListener:(id<MEGAChatRequestDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatRequestListener *delegateListener = new DelegateMEGAChatRequestListener(self, delegate, singleListener);
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


- (MegaChatRoomListener *)createDelegateMEGAChatRoomListener:(id<MEGAChatRoomDelegate>)delegate singleListener:(BOOL)singleListener {
    if (delegate == nil) return nil;
    
    DelegateMEGAChatRoomListener *delegateListener = new DelegateMEGAChatRoomListener(self, delegate, singleListener);
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

+ (void)setCatchException:(BOOL)enable {
    MegaChatApi::setCatchException(enable);
}

@end
