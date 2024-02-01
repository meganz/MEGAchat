/**
 * @file tests/sdk_test.cpp
 * @brief Mega SDK test file
 *
 * (c) 2016 by Mega Limited, Wellsford, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef CHATTEST_H
#define CHATTEST_H

#include "megachatapi.h"
#include <chatClient.h>
#include <future>
#include <fstream> // Win build requires it

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "gtest/gtest.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static const std::string APPLICATION_KEY = "MBoVFSyZ";
static constexpr unsigned int MAX_ATTEMPTS = 3;
static const unsigned int maxTimeout = 600;    // (seconds)
static const unsigned int pollingT = 500000;   // (microseconds) to check if response from server is received
static const unsigned int NUM_ACCOUNTS = 3;

#define TEST_LOG_ERROR(a, message) \
    do { \
        if (!(a)) \
        { \
            postLog(message); \
        } \
    } \
    while(false) \

class MegaLoggerTest : public ::mega::MegaLogger,
        public megachat::MegaChatLogger {

public:
    MegaLoggerTest(const char *filename);
    ~MegaLoggerTest();

    std::ofstream *getOutputStream() { return &testlog; }
    void postLog(const char *message);

private:
    std::ofstream testlog;

protected:
    void log(const char *time, int loglevel, const char *source, const char *message);
    void log(int loglevel, const char *message);
};

class Account
{
public:
    Account();
    Account(const std::string &email, const std::string &password);

    std::string getEmail() const;
    std::string getPassword() const;
private:
    std::string mEmail;
    std::string mPassword;
};

class TestChatRoomListener;

#ifndef KARERE_DISABLE_WEBRTC
class TestChatVideoListener : public megachat::MegaChatVideoListener
{
public:
    TestChatVideoListener();
    virtual ~TestChatVideoListener();

    virtual void onChatVideoData(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, int width, int height, char *buffer, size_t size);
};
#endif

class RequestListener
{
public:
    bool waitForResponse(unsigned int timeout = maxTimeout);
    virtual int getErrorCode() const = 0;

protected:
    bool mFinished = false;
    mega::MegaApi* mMegaApi = nullptr;
    megachat::MegaChatApi* mMegaChatApi = nullptr;
    RequestListener(mega::MegaApi* megaApi, megachat::MegaChatApi *megaChatApi);
};

class TestMegaRequestListener : public mega::MegaRequestListener, public RequestListener
{
public:
    TestMegaRequestListener(mega::MegaApi* megaApi, megachat::MegaChatApi *megaChatApi);
    ~TestMegaRequestListener();
    void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e) override;
    int getErrorCode() const override;
    mega::MegaRequest* getMegaRequest() const;

private:
    mega::MegaRequest *mRequest = nullptr;
    mega::MegaError *mError = nullptr;

};

class TestMegaChatRequestListener : public megachat::MegaChatRequestListener, public RequestListener
{
public:
    TestMegaChatRequestListener(mega::MegaApi *megaApi, megachat::MegaChatApi *megaChatApi);
    ~TestMegaChatRequestListener();
    void onRequestFinish(megachat::MegaChatApi *api, megachat::MegaChatRequest *request, megachat::MegaChatError *e) override;
    int getErrorCode() const override;
    megachat::MegaChatRequest* getMegaChatRequest() const;

private:
    megachat::MegaChatRequest *mRequest = nullptr;
    megachat::MegaChatError *mError = nullptr;
};

class MegaChatApiTest :
        public ::testing::Test,
        public ::mega::MegaListener,
        public ::mega::MegaTransferListener,
        public ::mega::MegaLogger,
        public megachat::MegaChatListener,
        public megachat::MegaChatCallListener,
        public megachat::MegaChatScheduledMeetingListener
{
public:
    // invalid account index
    static constexpr unsigned int testInvalidIdx = UINT16_MAX;

    // to be used as a resources releaser when the test exits. Some example of cleanups are:
    // unregistering listeners, logging out sessions, and any memory used from the free store.
    // Be wary of objects lifetimes and order used in the capture
    struct MegaMrProper
    {
        using CleanupFunction = std::function<void()>;
        CleanupFunction mOnRelease;
        ~MegaMrProper() { if (mOnRelease) mOnRelease(); }

        MegaMrProper(std::function<void()> f): mOnRelease(f){}
        MegaMrProper()                               = delete;
        MegaMrProper(const MegaMrProper&)            = delete;
        MegaMrProper(MegaMrProper&&)                 = delete;
        MegaMrProper& operator=(const MegaMrProper&) = delete;
        MegaMrProper& operator=(MegaMrProper&&)      = delete;
    };

    // DEPRECATED: we need to replace all it's usages by MegaMrProper
    struct MrProper
    {
        MrProper(std::function<void(megachat::MegaChatHandle)> f, const megachat::MegaChatHandle chatid)
            : mCleanup(f), mChatid(chatid){}

        std::function<void(megachat::MegaChatHandle)> mCleanup;
        megachat::MegaChatHandle mChatid;
        ~MrProper() { mCleanup(mChatid); }
    };

    // this structure contains all required data to represent a scheduled meeting, or a scheduled meeting occurrence
    struct SchedMeetingData
    {
        megachat::MegaChatHandle chatId = megachat::MEGACHAT_INVALID_HANDLE;
        megachat::MegaChatHandle schedId = megachat::MEGACHAT_INVALID_HANDLE;
        std::string timeZone, title, description;
        megachat::MegaChatTimeStamp startDate = 0, endDate = 0, overrides = 0, newStartDate = 0, newEndDate = 0;
        bool cancelled = false, newCancelled = false, publicChat = false, speakRequest = false,
            waitingRoom = false, openInvite = false, isMeeting = false;
        std::shared_ptr<megachat::MegaChatScheduledFlags> flags;
        std::shared_ptr<megachat::MegaChatScheduledRules> rules;
        std::shared_ptr<megachat::MegaChatPeerList> peerList;

        ~SchedMeetingData()                                     = default;
        explicit SchedMeetingData()                             = default;
        SchedMeetingData(SchedMeetingData&&)                    = delete;
        SchedMeetingData& operator=(const SchedMeetingData&)    = delete;
        SchedMeetingData& operator=(SchedMeetingData&&)         = default;
        SchedMeetingData(const SchedMeetingData& sm)
            : chatId(sm.chatId), schedId(sm.schedId), timeZone(sm.timeZone)
            , title(sm.title), description(sm.description), startDate(sm.startDate)
            , endDate(sm.endDate), overrides(sm.overrides), newStartDate(sm.newEndDate)
            , newEndDate(sm.newEndDate), cancelled(sm.cancelled), newCancelled(sm.newCancelled)
            , publicChat(sm.publicChat), speakRequest(sm.speakRequest), waitingRoom(sm.waitingRoom)
            , openInvite(sm.openInvite), isMeeting(sm.isMeeting)
            , flags(sm.flags ? sm.flags->copy() : nullptr)
            , rules(sm.rules ? sm.rules->copy() : nullptr)
            , peerList(sm.peerList ? sm.peerList->copy() : nullptr)
        {
        }
    };

    // required data to create or select a chatroom
    struct ChatroomCreationOptions
    {
        unsigned int mChatOpIdx = testInvalidIdx; // index of operator account from which we retrieve chatroom for test (it not necessarily needs to has moderator role for that chat)
        int mOpPriv         = megachat::MegaChatPeerList::PRIV_UNKNOWN;
        bool mCreate        = false;
        bool mPublicChat    = false;
        bool mMeetingRoom   = false;
        bool mWaitingRoom   = false;
        bool mSpeakRequest  = false;
        bool mOpenInvite    = false;

        // scheduled meeting data
        std::unique_ptr<SchedMeetingData> mSchedMeetingData;

        // peer list with the participants of the chatroom used for the test
        std::unique_ptr<megachat::MegaChatPeerList> mChatPeerList;

        // vector with peer accounts idx that participates in the chatroom
        std::vector<unsigned int> mChatPeerIdx;

        ~ChatroomCreationOptions()                                          = default;
        explicit ChatroomCreationOptions()                                  = default;
        ChatroomCreationOptions(ChatroomCreationOptions&& )                 = delete;
        ChatroomCreationOptions& operator=(const ChatroomCreationOptions&)  = delete;
        ChatroomCreationOptions& operator=(ChatroomCreationOptions&&)       = delete;
        ChatroomCreationOptions(const int opPriv,
                                const bool cr,
                                const bool pub,
                                const bool mr,
                                const bool wr,
                                const bool sr,
                                const bool oi,
                                const SchedMeetingData* sm,
                                const megachat::MegaChatPeerList* peers)
            : mOpPriv(opPriv)
            , mCreate(cr)
            , mPublicChat(pub)
            , mMeetingRoom(mr)
            , mWaitingRoom(wr)
            , mSpeakRequest(sr)
            , mOpenInvite(oi)
            , mSchedMeetingData(sm ? std::make_unique<SchedMeetingData>(*sm) : nullptr)
            , mChatPeerList(peers ? peers->copy() : nullptr)
        {
        }

        ChatroomCreationOptions(const ChatroomCreationOptions& opt)
            : mOpPriv(opt.mOpPriv)
            , mCreate(opt.mCreate)
            , mPublicChat(opt.mPublicChat)
            , mMeetingRoom(opt.mMeetingRoom)
            , mWaitingRoom(opt.mWaitingRoom)
            , mSpeakRequest(opt.mSpeakRequest)
            , mOpenInvite(opt.mOpenInvite)
            , mSchedMeetingData(opt.mSchedMeetingData ? std::make_unique<SchedMeetingData>(*opt.mSchedMeetingData) : nullptr)
            , mChatPeerList(opt.mChatPeerList ? opt.mChatPeerList->copy() : nullptr)
        {
        }
    };

    // this structure contains all data common to most of automated tests
    struct TestData
    {
        ~TestData()                           = default;
        explicit TestData()                   = default;
        TestData(const TestData&)             = delete;
        TestData(TestData&& )                 = delete;
        TestData& operator=(const TestData&)  = delete;
        TestData& operator=(TestData&&)       = delete;

        // set test operator role index (will be the account in charge of performing all operations)
        // This index can be changed at any point of the test.
        unsigned int mOpIdx = testInvalidIdx;

        // chatid of chatroom that will be used in the test
        megachat::MegaChatHandle mChatid = megachat::MEGACHAT_INVALID_HANDLE;

        // maps account index to sessions returned by MegaChatApiTest::login
        std::map<unsigned int, std::unique_ptr<char[]>> mSessions;

        // maps account index to user handle
        std::map<unsigned int, megachat::MegaChatHandle> mAccounts;

        // chatroom options required to get/create chatroom for test
        ChatroomCreationOptions mChatOptions;

        // maps account index to ChatroomListener
        std::map<unsigned int, std::shared_ptr<TestChatRoomListener>> mChatroomListeners;

#ifndef KARERE_DISABLE_WEBRTC
        // maps account index to TestChatVideoListener
        std::map<unsigned int, TestChatVideoListener> mMapLocalVideoListeners;
#endif

        // returns MegaChatPeerList that includes peers that participates in the selected chatroom
        const megachat::MegaChatPeerList* getChatParticipantsList()
        {
            return mChatOptions.mChatPeerList.get();
        }

        // returns a vector with all accounts indexes
        std::vector<unsigned int> getIdxVector()
        {
            std::vector<unsigned int> v(mAccounts.size());
            std::transform(mAccounts.begin(), mAccounts.end(), v.begin(),
                           [](const auto& pair) { return pair.first; });
            return v;
        }

        // returns a vector with all chat participants account indexes
        const std::vector<unsigned int> getChatParticipantsIdxs()
        {
            return mChatOptions.mChatPeerIdx;
        }

        // check if sessions returned by MegaChatApiTest::login are valid
        void areSessionsValid()
        {
            std::for_each(mSessions.begin(), mSessions.end(), [](const auto& it)
            {
                ASSERT_TRUE(it.second);
            });
        }

        void checkSessionsAndAccounts()
        {
            ASSERT_EQ(mSessions.size(), mAccounts.size());
            std::for_each(mSessions.begin(), mSessions.end(), [this](const auto& it)
            {
                ASSERT_TRUE(mAccounts.find(it.first) != mAccounts.end());
            });
        }

        std::shared_ptr<TestChatRoomListener> getChatroomListener(const unsigned int i)
        {
            auto it = mChatroomListeners.find(i);
            if (it == mChatroomListeners.end()) { return nullptr; }

            return it->second;
        }
    };

    template <typename T>
    struct AuxVars
    {
    public:
        bool validInput(const unsigned int i, const std::string_view n) const
        {
            return !n.empty() && i < NUM_ACCOUNTS;
        }

        bool exists(const unsigned int i, const std::string_view n) const
        {
            return mVarsMap[i].find(std::string{n}) != mVarsMap[i].end();
        }

        // adds a new entry in map <variable name, val<T>>
        T* add(const unsigned int i, const std::string_view n, const T val)
        {
            if (!validInput(i, n)) { return nullptr; }
            if (exists(i, n))      { return nullptr; }

            auto res = mVarsMap[i].emplace(std::string{n}, val);
            return res.second ? &res.first->second : nullptr;
        }

        // returns pointer to value<T> mapped by key n
        T* getVar(const unsigned int i, const std::string_view n)
        {
            if (!validInput(i, n)) { return nullptr; }
            if (!exists(i, n))     { return nullptr; }

            return &mVarsMap[i][std::string{n}];
        }

        // updates value<T> mapped by key n
        bool updateIfExists(const unsigned int i, const std::string_view n, const T v)
        {
            if (!validInput(i, n)) { return false; }
            if (!exists(i, n))     { return false; }

            mVarsMap[i][std::string{n}] = v;
            return true;
        }

        // remove entry from map given a variable name
        bool remove(const unsigned int i, const std::string_view n)
        {
            if (!validInput(i, n)) { return false; }
            if (!exists(i, n))     { return false; }

            return mVarsMap[i].erase(std::string{n});
        }

        // clean all vars for a given account index
        bool clean(const unsigned int i)
        {
            if (i >= NUM_ACCOUNTS) { return false; }
            mVarsMap[i].clear();
        }

        // clean all vars for all account indexes
        void cleanAll()
        {
            for (unsigned int i = 0; i < NUM_ACCOUNTS; ++i)
            {
                mVarsMap[i].clear();
            }
        }
    private:
        std::array<std::map<std::string, T>, NUM_ACCOUNTS> mVarsMap{};
    };

    /**
     * It can be used to store a subset of variables of mAuxBool, and provide to methods like waitForAction,
     * that will execute an action and wait until (any or all) provided flags has been set true
     */
    struct ExitBoolFlags
    {
        auto find(const std::string_view n)
        {
            return mVars.find(std::string{n}) != mVars.end();
        }

        bool exists(const std::string_view n)
        {
            return mVars.find(std::string{n}) != mVars.end();
        }

        size_t size() const
        {
            return mVars.size();
        }

        bool updateAll(const bool v)
        {
            for (auto& entry : mVars)
            {
                if (entry.second == nullptr) { return false; }
                *(entry.second) = v;
            }
            return true;
        }

        bool allEqualTo(const bool v)
        {
            return std::all_of(mVars.begin(), mVars.end(), [v](const auto& entry)
            {
                return (entry.second != nullptr) && (*entry.second == v);
            });
        }

        bool anyEqualTo(const bool v)
        {
            return std::any_of(mVars.begin(), mVars.end(), [v](const auto& entry)
            {
                return (entry.second != nullptr) && (*entry.second == v);
            });
        }

        bool add(const std::string_view n, bool* v)
        {
            if (exists(n)) { return false; }
            mVars[std::string{n}] = v;
            return true;
        }

        bool updateFlagValue(const std::string_view n, const bool v)
        {
            if (!exists(n)) { return false; }
            *mVars[std::string{n}] = v;
            return true;
        }

        bool remove(const std::string_view n)
        {
            return mVars.erase(std::string{n});
        }

        void clean()
        {
            mVars.clear();
        }

        std::string printAll()
        {
            int i = 0;
            std::string msg;
            for (auto& v : mVars)
            {
                msg += "Flag_" + std::to_string(i++) + ": " + v.first
                    + (*v.second ? " (true)" : " (false)") + "\n";
            }
            return msg;
        }

        std::map<std::string, bool*> mVars;
    };

    static std::string getCallIdStrB64(const megachat::MegaChatHandle h)
    {
        const std::unique_ptr<char[]> idB64(mega::MegaApi::userHandleToBase64(h));
        return idB64 ? idB64.get() : "INVALID callid";
    };

    static std::string getChatIdStrB64(const megachat::MegaChatHandle h)
    {
        const std::unique_ptr<char[]> idB64(mega::MegaApi::userHandleToBase64(h));
        return idB64 ? idB64.get() : "INVALID chatId";
    };

    static std::string getSchedIdStrB64(const megachat::MegaChatHandle h)
    {
        const std::unique_ptr<char[]> idB64(mega::MegaApi::userHandleToBase64(h));
        return idB64 ? idB64.get() : "INVALID schedId";
    };

    MegaChatApiTest();
    ~MegaChatApiTest() override;

    // Global test environment initialization
    static void initFS();
    static void init(const std::string& log);
    // Global test environment clear up
    static void terminate();
    static void terminateFS();

    using AuxVarsBool     = AuxVars<bool>;
    using AuxVarsMCHandle = AuxVars<megachat::MegaChatHandle>;

    AuxVarsBool& boolVars()       { return mAuxBool; };
    AuxVarsMCHandle& handleVars() { return mAuxHandles; };

protected:
    static Account& account(unsigned i) { return getEnv().account(i); }
    static MegaLoggerTest* logger() { return getEnv().logger(); }

    // Specific test environment initialization for each test
    void SetUp() override;
    // Specific test environment clear up for each test
    void TearDown() override;

    // email and password parameter is used if you don't want to use default values for accountIndex
    char *login(unsigned int accountIndex, const char *session = NULL, const char *email = NULL, const char *password = NULL);
    bool chatApiInit(unsigned accountIndex, const char *session = nullptr);
    bool chatApiLogin(unsigned accountIndex, const char *session = nullptr, const char *email = nullptr, const char *password = nullptr);
    bool chatApiJoinAll(unsigned accountIndex, const char *email = nullptr);
    void logout(unsigned int accountIndex, bool closeSession = false);

public:
    static const char* printChatRoomInfo(const megachat::MegaChatRoom *);
    static const char* printMessageInfo(const megachat::MegaChatMessage *);
protected:
    static const char* printChatListItemInfo(const megachat::MegaChatListItem *);
public:
    void postLog(const std::string &msg);

protected:
    // check if any/all flags in eF has been set true
    bool exitWait(ExitBoolFlags& eF, const bool waitForAll) const;
    // deprecated: replace current usages of this method by prototype above
    bool exitWait(const std::vector<bool *>&responsesReceived, bool any) const;
    // waits until any/all flags in eF has been set true
    bool waitForMultiResponse(ExitBoolFlags& eF, bool waitForAll, unsigned int timeout) const;
    // deprecated: replace current usages of this method by prototype above
    bool waitForMultiResponse(std::vector<bool *>responsesReceived, bool any, unsigned int timeout = maxTimeout) const;
    bool waitForResponse(bool *responseReceived, unsigned int timeout = maxTimeout) const;

    /**
     * @brief executes an asynchronous action and wait for results
     * @param maxAttempts max number of attempts the action must be retried
     * @param eF conditions that must be accomplished consider action finished
     * @param actionMsg string that defines the action
     * @param waitForAll wait for all exit conditions
     * @param resetFlags flag that indicates if exitFlags must be reset before executing action
     * @param timeout max timeout (in seconds) to execute the action
     * @param action function to be executed
     */
    void waitForAction(int maxAttempts, ExitBoolFlags& eF, const std::string& actionMsg, bool waitForAll, bool resetFlags, unsigned int timeout, std::function<void()>action);

    /**
     * @brief executes an asynchronous action and wait for results
     * @param maxAttempts max number of attempts the action must be retried
     * @param exitFlags vector of conditions that must be accomplished consider action finished
     * @param flagsStr vector of strings to identify each condition
     * @param actionMsg string that defines the action
     * @param waitForAll wait for all exit conditions
     * @param resetFlags flag that indicates if exitFlags must be reset before executing action
     * @param timeout max timeout (in seconds) to execute the action
     * @param action function to be executed
     * @deprecated replace current usages of this method by prototype above
     */
    void waitForAction(int maxAttempts, std::vector<bool*> exitFlags, const std::vector<std::string>& flagsStr, const std::string& actionMsg, bool waitForAll, bool resetFlags, unsigned int timeout, std::function<void()>action);
    void initChat(unsigned int a1, unsigned int a2, std::unique_ptr<mega::MegaUser>& user, megachat::MegaChatHandle& chatid, std::unique_ptr<char[]>& primarySession, std::unique_ptr<char[]>& secondarySession, std::unique_ptr<TestChatRoomListener>& chatroomListener);

    /**
     * @brief Loads history for the specified chatroom.
     *
     * If there's no error this method will returns:
     * - The number of loaded messages
     *
     * Otherwise:
     * - MegaChatError::ERROR_TOOMANY if Timeout for connecting to chatd has expired
     * - MegaChatError::ERROR_ACCESS if Timeout for loading history from chat has expired
     *
     * @param accountIndex index of account that loads history from chatroom
     * @param chatid MegaChatHandle that identifies the chat room
     * @param chatroomListener TestChatRoomListener that track MegaChatRoomListener events
     */
    int loadHistory(const unsigned int accountIndex, const megachat::MegaChatHandle chatid, TestChatRoomListener* chatroomListener);
    void makeContact(const unsigned int a1, const unsigned int a2);
    bool areContact(unsigned int a1, unsigned int a2);
    bool isChatroomUpdated(unsigned int index, megachat::MegaChatHandle chatid);
    megachat::MegaChatHandle getGroupChatRoom();
    bool addChatVideoListener(const unsigned int idx, const megachat::MegaChatHandle chatid);
    void cleanChatVideoListeners();
    void logoutTestAccounts();
    void closeOpenedChatrooms();
    bool removeChatVideoListener(const unsigned int idx, const megachat::MegaChatHandle chatid, TestChatVideoListener &vl);

    /* select a group chat room, by default with PRIV_MODERATOR for primary account
     * in case chat privileges for primary account doesn't matter, provide PRIV_UNKNOWN in priv param
     * it allows finding/creating a group chat of more than 2 participants where the creator is a[0]
     * param a contains the same participants as param peers + the user who will create the chat (a[0])
     * ToDo: consider removing peers param and create from `a` param in this function instead
     */
    megachat::MegaChatHandle getGroupChatRoom(const std::vector<unsigned int>& a,
                                              megachat::MegaChatPeerList* peers,
                                              const int a1Priv = megachat::MegaChatPeerList::PRIV_MODERATOR,
                                              const bool create = true,
                                              const bool publicChat = false,
                                              const bool meetingRoom = false,
                                              const bool waitingRoom = false,
                                              const bool speakRequest = false,
                                              SchedMeetingData* schedMeetingData = nullptr);

    void createChatroomAndSchedMeeting(megachat::MegaChatHandle& chatid, const unsigned int a1,
                                       const unsigned int a2, const SchedMeetingData& smData);

    // returns the account idx that represents the operator account in the test
    unsigned int getOpIdx() { return mData.mOpIdx; }

    // this method allows to initialize SchedMeetingData structure
    void initLocalSchedMeeting(const megachat::MegaChatHandle chatId, const megachat::MegaChatHandle schedId, const std::string& timeZone,
                               const std::string& title, const std::string& description, const megachat::MegaChatTimeStamp startDate,
                               const megachat::MegaChatTimeStamp endDate, const megachat::MegaChatTimeStamp overrides, const megachat::MegaChatTimeStamp newStartDate,
                               const megachat::MegaChatTimeStamp newEndDate, const bool cancelled, const bool newCancelled, const bool publicChat,
                               const bool speakRequest, const bool waitingRoom, const bool openInvite, const bool isMeeting,
                               const bool sendEmails, const int rulesFreq, const int rulesInterval, const megachat::MegaChatTimeStamp rulesUntil,
                               const ::megachat::MegaChatPeerList* peerlist, const ::mega::MegaIntegerList* rulesByWeekDay,
                               const ::mega::MegaIntegerList* rulesByMonthDay, const ::mega::MegaIntegerMap* rulesByMonthWeekDay);

    // Clears all temporal variables from MegaChatApiTest::mAuxHandles and MegaChatApiTest::mAuxBool
    void clearTemporalVars();

    // Adds a temporal MegaChatHandle variable to MegaChatApiTest::mAuxHandles
    void addHandleVar(const unsigned int i, const std::string& n, const ::megachat::MegaChatHandle val);

    // Adds a temporal boolean variable to MegaChatApiTest::mAuxBool
    bool* addBoolVar(const unsigned int i, const std::string& n, const bool val);

    // Adds a temporal boolean variable, to ExitBoolFlags param, and also to MegaChatApiTest::mAuxBool
    void addBoolVarAndExitFlag(const unsigned int i, ExitBoolFlags &eF, const std::string& n, const bool val);

#ifndef KARERE_DISABLE_WEBRTC

    // starts a call in a chatroom
    void startCallInChat(const unsigned int callerIdx, ExitBoolFlags& eF, const ::megachat::MegaChatHandle chatid,
                         const bool enableVideo, const bool enableAudio, const bool notRinging);

    // rejects a call
    void rejectCall(const unsigned int performerIdx, ExitBoolFlags& eF, const ::megachat::MegaChatHandle chatid);

    // starts a call in a chatroom with waiting room option enabled
    void startWaitingRoomCall(const unsigned int callerIdx, ExitBoolFlags& eF, const ::megachat::MegaChatHandle chatid, const ::megachat::MegaChatHandle schedIdWr,
                              const bool enableVideo, const bool enableAudio);

    // answers a call in a chatroom
    void answerChatCall(unsigned int calleeIdx, ExitBoolFlags& eF, const ::megachat::MegaChatHandle chatid,
                        const bool enableVideo, const bool enableAudio);

    void endChatCallTestCleanup(unsigned int performerIdx, ExitBoolFlags& eF, const megachat::MegaChatHandle chatid);
#endif

    /**
     * @brief Allows to set the title of a group chat
     *
     * The account idx that will perform this operation will be the idx
     * returned by getOpIdx()
     *
     * All accounts idxs registered in TestData::mChatroomListeners (even action performer idx: getOpIdx()) will wait for
     * receiving onChatListItemUpdate(CHANGE_TYPE_TITLE) and onChatRoomUpdate(CHANGE_TYPE_TITLE) events.
     *
     * @param title Null-terminated character string with the title that wants to be set. If the
     * title is longer than 30 characters, it will be truncated to that maximum length.
     *
     * @param waitSecs max timeout (in seconds) that this method will wait for any event (like onRequestFinish, flags updates...)
     */
    void setChatTitle(const std::string& title, const unsigned int waitSecs = maxTimeout);

    megachat::MegaChatHandle getPeerToPeerChatRoom(const unsigned int a1, const unsigned int a2);

    // send msg, wait for confirmation, reception by other side, delivery status. Returns ownership of confirmed msg
    megachat::MegaChatMessage *sendTextMessageOrUpdate(unsigned int senderAccountIndex, unsigned int receiverAccountIndex,
                                               megachat::MegaChatHandle chatid, const std::string& textToSend,
                                               TestChatRoomListener *chatroomListener, megachat::MegaChatHandle messageId = megachat::MEGACHAT_INVALID_HANDLE);

    void checkEmail(unsigned int indexAccount);
    std::string dateToString();
    megachat::MegaChatMessage *attachNode(unsigned int a1, unsigned int a2, megachat::MegaChatHandle chatid,
                                    ::mega::MegaNode *nodeToSend, TestChatRoomListener* chatroomListener);

    void clearHistory(unsigned int a1, unsigned int a2, megachat::MegaChatHandle chatid, TestChatRoomListener *chatroomListener);
    void leaveChat(unsigned int accountIndex, megachat::MegaChatHandle chatid);

    unsigned int getMegaChatApiIndex(megachat::MegaChatApi *api);
    unsigned int getMegaApiIndex(::mega::MegaApi *api);

    void createFile(const std::string &fileName, const std::string &sourcePath, const std::string &contain);
    ::mega::MegaNode *uploadFile(int accountIndex, const std::string &fileName, const std::string &sourcePath, const std::string &targetPath);
    void addTransfer(int accountIndex);
    bool &isNotTransferRunning(int accountIndex);

    bool downloadNode(int accountIndex, ::mega::MegaNode *nodeToDownload);
    bool importNode(int accountIndex, ::mega::MegaNode* node, const std::string& destinationName);

    void getContactRequest(unsigned int accountIndex, bool outgoing, int expectedSize = 1);

    static int purgeLocalTree(const std::string& path);
    void purgeCloudTree(unsigned int accountIndex, ::mega::MegaNode* node);
    void clearAndLeaveChats(unsigned accountIndex, const std::vector<megachat::MegaChatHandle>& skipChats);
    void removePendingContactRequest(unsigned int accountIndex);
    void changeLastName(unsigned int accountIndex, std::string lastName);

    // chatrooms auxiliar methods

    void inviteToChat (const unsigned int& a1, const unsigned int& a2, const megachat::MegaChatHandle& uh, const megachat::MegaChatHandle& chatid, const int privilege,
                       std::shared_ptr<TestChatRoomListener>chatroomListener);


    void updateChatPermission (const unsigned int& a1, const unsigned int& a2, const megachat::MegaChatHandle& uh, const megachat::MegaChatHandle& chatid, const int privilege,
                               std::shared_ptr<TestChatRoomListener>chatroomListener);

    // updates an existing scheduled meeting
    void updateSchedMeeting(const unsigned int a1, const unsigned int a2, const int expectedError, const SchedMeetingData& smData, const bool updateChatTitle);

    // update chatroom title
    void changeTitle(const unsigned int a1, TestChatRoomListener* chatroomListener, const megachat::MegaChatHandle chatid, const std::string& title);

#ifndef KARERE_DISABLE_WEBRTC
    // calls auxiliar methods
    // ----------------------------------------------------------------------------------------------------------------------------

    void startChatCall(const megachat::MegaChatHandle chatid, const unsigned int performerIdx, const std::set<unsigned int> participants, const bool enableVideo, const bool enableAudio);
    void answerChatCall(const megachat::MegaChatHandle chatid, const unsigned int performerIdx, const std::set<unsigned int> participants, const bool enableVideo, const bool enableAudio);

    // gets a pointer to the local flag that indicates if we have reached an specific callstate
    bool* getChatCallStateFlag (unsigned int index, int state);

    // resets the local flag that indicates if we have reached an specific call state
    void resetTestChatCallState (unsigned int index, int state);

    // waits for a specific callstate
    void waitForChatCallState(unsigned int index, int state);

    // ensures that <action> is executed successfully before maxAttempts and before timeout expires
    // if call gets disconnected before action is executed, command queue will be cleared, so we need to wait
    // until performer account is connected (CALL_STATUS_IN_PROGRESS) to SFU for that call and re-try <action>
    void waitForCallAction (unsigned int pIdx, int maxAttempts, bool* exitFlag,  const char* errMsg, unsigned int timeout, std::function<void()>action);
#endif

    ::mega::MegaApi* megaApi[NUM_ACCOUNTS];
    megachat::MegaChatApi* megaChatApi[NUM_ACCOUNTS];

    // flags
    bool requestFlags[NUM_ACCOUNTS][::mega::MegaRequest::TOTAL_OF_REQUEST_TYPES];
    bool initStateChanged[NUM_ACCOUNTS];
    int initState[NUM_ACCOUNTS];
    bool mChatConnectionOnline[NUM_ACCOUNTS];
    int lastErrorTransfer[NUM_ACCOUNTS];

    megachat::MegaChatRoom *chatroom[NUM_ACCOUNTS];
    bool chatUpdated[NUM_ACCOUNTS];
    bool chatItemUpdated[NUM_ACCOUNTS];
    bool chatItemClosed[NUM_ACCOUNTS];
    bool peersUpdated[NUM_ACCOUNTS];
    bool titleUpdated[NUM_ACCOUNTS];
    bool chatArchived[NUM_ACCOUNTS];
    bool chatPreviewClosed[NUM_ACCOUNTS];

    ::mega::MegaHandle mNodeCopiedHandle[NUM_ACCOUNTS];
    ::mega::MegaHandle mNodeUploadHandle[NUM_ACCOUNTS];

    bool mNotTransferRunning[NUM_ACCOUNTS];
    bool mPresenceConfigUpdated[NUM_ACCOUNTS];
    bool mOnlineStatusUpdated[NUM_ACCOUNTS];
    int mOnlineStatus[NUM_ACCOUNTS];

    // structure with all data common to most of automated tests
    TestData mData;

    std::unique_ptr<::mega::MegaContactRequest> mContactRequest[NUM_ACCOUNTS];
    bool mContactRequestUpdated[NUM_ACCOUNTS];
    bool mUsersUpdate[NUM_ACCOUNTS];
    std::map <unsigned int, bool> mUsersChanged[NUM_ACCOUNTS];
    std::map <::megachat::MegaChatHandle, bool> mUsersAllowJoin[NUM_ACCOUNTS];
    std::map <::megachat::MegaChatHandle, bool> mUsersRejectJoin[NUM_ACCOUNTS];

#ifndef KARERE_DISABLE_WEBRTC
    bool mCallWithIdReceived[NUM_ACCOUNTS];
    bool mCallReceived[NUM_ACCOUNTS];
    bool mCallReceivedRinging[NUM_ACCOUNTS];
    bool mCallStopRinging[NUM_ACCOUNTS];
    bool mCallInProgress[NUM_ACCOUNTS];
    bool mCallLeft[NUM_ACCOUNTS];
    bool mCallDestroyed[NUM_ACCOUNTS];
    bool mCallConnecting[NUM_ACCOUNTS];
    bool mCallWR[NUM_ACCOUNTS];
    int mTerminationCode[NUM_ACCOUNTS];
    bool mCallWrChanged[NUM_ACCOUNTS];
    bool mCallWrAllow[NUM_ACCOUNTS];
    bool mCallWrDeny[NUM_ACCOUNTS];
    megachat::MegaChatHandle mChatIdRingInCall[NUM_ACCOUNTS];
    megachat::MegaChatHandle mChatIdStopRingInCall[NUM_ACCOUNTS];
    megachat::MegaChatHandle mChatIdInProgressCall[NUM_ACCOUNTS];
    megachat::MegaChatHandle mCallIdRingIn[NUM_ACCOUNTS];
    megachat::MegaChatHandle mCallIdStopRingIn[NUM_ACCOUNTS];
    megachat::MegaChatHandle mCallIdExpectedReceived[NUM_ACCOUNTS];
    megachat::MegaChatHandle mCallIdJoining[NUM_ACCOUNTS];
    megachat::MegaChatHandle mSchedIdUpdated[NUM_ACCOUNTS];
    megachat::MegaChatHandle mSchedIdRemoved[NUM_ACCOUNTS];
    TestChatVideoListener *mLocalVideoListener[NUM_ACCOUNTS];
    TestChatVideoListener *mRemoteVideoListener[NUM_ACCOUNTS];
    bool mChatCallOnHold[NUM_ACCOUNTS];
    bool mChatCallOnHoldResumed[NUM_ACCOUNTS];
    bool mChatCallAudioEnabled[NUM_ACCOUNTS];
    bool mChatCallAudioDisabled[NUM_ACCOUNTS];
    bool mChatCallSessionStatusInProgress[NUM_ACCOUNTS];
    bool mChatSessionWasDestroyed[NUM_ACCOUNTS];
    bool mChatCallSilenceReq[NUM_ACCOUNTS];
    bool mSchedMeetingUpdated[NUM_ACCOUNTS];
    bool mSchedOccurrUpdated[NUM_ACCOUNTS];
    bool mSessSpeakPermChanged[NUM_ACCOUNTS];
    bool mOwnFlagsChanged[NUM_ACCOUNTS];
    bool mOwnSpeakStatusChanged[NUM_ACCOUNTS];
    bool mOwnCallPermissionsChanged[NUM_ACCOUNTS];
    bool mSessSpeakReqRecv[NUM_ACCOUNTS];
    unsigned mOwnSpeakStatus[NUM_ACCOUNTS];
    std::map<::megachat::MegaChatHandle, bool> mSessSpeakPerm[NUM_ACCOUNTS];
    std::map<::megachat::MegaChatHandle, bool> mSessSpeakRequests[NUM_ACCOUNTS];
#endif

    bool mLoggedInAllChats[NUM_ACCOUNTS];
    std::vector <megachat::MegaChatHandle>mChatListUpdated[NUM_ACCOUNTS];
    bool mChatsUpdated[NUM_ACCOUNTS];
    static const std::string DEFAULT_PATH;
    static const std::string PATH_IMAGE;
    static const std::string FILE_IMAGE_NAME;

    static const std::string LOCAL_PATH;
    static const std::string REMOTE_PATH;
    static const std::string DOWNLOAD_PATH;

    // implementation for MegaListener
    void onRequestStart(::mega::MegaApi *, ::mega::MegaRequest *) override {}
    void onRequestFinish(::mega::MegaApi *api, ::mega::MegaRequest *request, ::mega::MegaError *e) override;
    void onRequestUpdate(::mega::MegaApi*, ::mega::MegaRequest *) override {}
    void onChatsUpdate(mega::MegaApi* api, mega::MegaTextChatList *chats) override;
    void onRequestTemporaryError(::mega::MegaApi *, ::mega::MegaRequest *, ::mega::MegaError*) override {}
    void onContactRequestsUpdate(::mega::MegaApi* api, ::mega::MegaContactRequestList* requests) override;
    void onUsersUpdate(::mega::MegaApi* api, ::mega::MegaUserList* userList) override;

    // implementation for MegaChatListener
    void onChatInitStateUpdate(megachat::MegaChatApi *api, int newState) override;
    void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item) override;
    void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress) override;
    void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config) override;
    void onChatConnectionStateUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid, int state) override;

    void onTransferStart(::mega::MegaApi *api, ::mega::MegaTransfer *transfer) override;
    void onTransferFinish(::mega::MegaApi* api, ::mega::MegaTransfer *transfer, ::mega::MegaError* error) override;
    void onTransferUpdate(::mega::MegaApi *api, ::mega::MegaTransfer *transfer) override;
    void onTransferTemporaryError(::mega::MegaApi *api, ::mega::MegaTransfer *transfer, ::mega::MegaError* error) override;
    bool onTransferData(::mega::MegaApi *api, ::mega::MegaTransfer *transfer, char *buffer, size_t size) override;

#ifndef KARERE_DISABLE_WEBRTC
    void onChatCallUpdate(megachat::MegaChatApi* api, megachat::MegaChatCall *call) override;
    void onChatSessionUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid,
                                     megachat::MegaChatHandle callid,
                                     megachat::MegaChatSession *session) override;

    void onChatSchedMeetingUpdate(megachat::MegaChatApi* api, megachat::MegaChatScheduledMeeting* sm) override;
    void onSchedMeetingOccurrencesUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid, bool append) override;
#endif

private:

    // Aux vars maps: these maps can be used to add temporal variables that needs to be updated by any callback or code path,
    // this avoids defining amounts of vars in MegaChatApiTest class
    //
    // The lifetime of these maps is the lifetime of MegaChatApiTest, so these maps will be automatically released after
    // each test execution. To prevent conflicts between different test cases at any particular automated test, it's recommended to remove
    // unused variables after it's use, or clear maps (just in case we don't need any variable already defined).

    // maps a var name to boolean.
    // It can be used to "register" temporal boolean variables that will be used to wait for async events.
    // <var_name, boolVal>[NUM_ACCOUNTS]
    AuxVarsBool mAuxBool;

    // maps a var name to MegaChatHandle
    // It can be used to "register" temporal variables that will be used to store received handles on MegaChat callbacks
    // <var_name, MegaChatHandle>[NUM_ACCOUNTS]
    AuxVarsMCHandle mAuxHandles;

    class TestEnv
    {
    public:
        void setLogFile(const std::string& f) { mLogger.reset(new MegaLoggerTest(f.c_str())); }
        MegaLoggerTest* logger() const { return mLogger.get(); }
        void addAccount(const std::string& email, const std::string& pswd) { mAccounts.emplace_back(email, pswd); }
        Account& account(unsigned i) { assert(i < mAccounts.size()); return mAccounts[i]; }

    private:
        std::vector<Account> mAccounts;
        std::unique_ptr<MegaLoggerTest> mLogger;
    };

    static TestEnv& getEnv() { static TestEnv env; return env; }
};

class TestChatRoomListener : public megachat::MegaChatRoomListener
{
public:
    TestChatRoomListener(MegaChatApiTest *t, megachat::MegaChatApi **apis, megachat::MegaChatHandle chatid);
    void clearMessages(unsigned int apiIndex);
    bool hasValidMessages(unsigned int apiIndex);
    bool hasArrivedMessage(unsigned int apiIndex, megachat::MegaChatHandle messageHandle);

    MegaChatApiTest *t;
    megachat::MegaChatApi **megaChatApi;
    megachat::MegaChatHandle chatid;

    bool historyLoaded[NUM_ACCOUNTS];   // when, after loadMessage(X), X messages have been loaded
    bool historyTruncated[NUM_ACCOUNTS];
    bool msgLoaded[NUM_ACCOUNTS];
    bool msgConfirmed[NUM_ACCOUNTS];
    bool msgDelivered[NUM_ACCOUNTS];
    bool msgReceived[NUM_ACCOUNTS];
    bool msgEdited[NUM_ACCOUNTS];
    bool msgDeleted[NUM_ACCOUNTS];
    bool msgSeen[NUM_ACCOUNTS] = {};
    bool msgRejected[NUM_ACCOUNTS];
    bool msgAttachmentReceived[NUM_ACCOUNTS];
    bool msgContactReceived[NUM_ACCOUNTS];
    bool msgRevokeAttachmentReceived[NUM_ACCOUNTS];
    bool reactionReceived[NUM_ACCOUNTS];
    bool retentionHistoryTruncated[NUM_ACCOUNTS];
    megachat::MegaChatHandle mConfirmedMessageHandle[NUM_ACCOUNTS];
    megachat::MegaChatHandle mEditedMessageHandle[NUM_ACCOUNTS];
    megachat::MegaChatHandle mRetentionMessageHandle[NUM_ACCOUNTS];

    megachat::MegaChatMessage *message;
    std::vector <megachat::MegaChatHandle>msgId[NUM_ACCOUNTS];
    int msgCount[NUM_ACCOUNTS];
    megachat::MegaChatHandle uhAction[NUM_ACCOUNTS];
    int priv[NUM_ACCOUNTS];
    std::string content[NUM_ACCOUNTS];
    bool chatUpdated[NUM_ACCOUNTS];
    bool userTyping[NUM_ACCOUNTS];
    bool titleUpdated[NUM_ACCOUNTS];
    bool archiveUpdated[NUM_ACCOUNTS];
    bool previewsUpdated[NUM_ACCOUNTS];
    bool retentionTimeUpdated[NUM_ACCOUNTS];
    bool chatModeUpdated[NUM_ACCOUNTS];

    // implementation for MegaChatRoomListener
    void onChatRoomUpdate(megachat::MegaChatApi* megaChatApi, megachat::MegaChatRoom *chat) override;
    void onMessageLoaded(megachat::MegaChatApi* megaChatApi, megachat::MegaChatMessage *msg) override;   // loaded by getMessages()
    void onMessageReceived(megachat::MegaChatApi* megaChatApi, megachat::MegaChatMessage *msg) override;
    void onMessageUpdate(megachat::MegaChatApi* megaChatApi, megachat::MegaChatMessage *msg) override;   // new or updated
    void onReactionUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle msgid, const char *reaction, int count) override;
    void onHistoryTruncatedByRetentionTime(megachat::MegaChatApi *api, megachat::MegaChatMessage *msg) override;

private:
    unsigned int getMegaChatApiIndex(megachat::MegaChatApi *api);
};

class MegaChatApiUnitaryTest: public ::testing::Test
{
};

class ResultHandler
{
public:
    int waitForResult(int seconds = maxTimeout)
    {
        if (std::future_status::ready != futureResult.wait_for(std::chrono::seconds(seconds)))
        {
            errorStr = "Timeout";
            return -999; // local timeout
        }
        return futureResult.get();
    }

    const std::string& getErrorString() const { return errorStr; }

protected:
    void finish(int errCode, std::string&& errStr)
    {
        assert(!resultReceived); // call this function only once!
        errorStr.swap(errStr);
        resultReceived = true;
        promiseResult.set_value(errCode);
    }

    bool finished() const { return resultReceived; }

protected:
    std::promise<int> promiseResult;
    std::future<int> futureResult = promiseResult.get_future();
    std::atomic<bool> resultReceived = false;
    std::string errorStr;
};

class RequestTracker : public ::mega::MegaRequestListener, public ResultHandler
{
public:
    RequestTracker(mega::MegaApi *megaApi)
        : mApi(megaApi)
    {
    }

    ~RequestTracker()
    {
        if (!resultReceived)
        {
            mApi->removeRequestListener(this);
        }
    }

    void onRequestFinish(::mega::MegaApi*, ::mega::MegaRequest* req,
                         ::mega::MegaError* e) override
    {
        request.reset(req ? req->copy() : nullptr);
        finish(e->getErrorCode(), e->getErrorString() ? e->getErrorString() : "");
    }

    ::mega::MegaHandle getNodeHandle() const
    {
        // if the operation succeeded and supplies a node handle
        return (finished() && request) ? request->getNodeHandle() : ::mega::INVALID_HANDLE;
    }

    std::string getLink() const
    {
        // if the operation succeeded and supplied a link
        return (finished() && request && request->getLink()) ? request->getLink() : std::string();
    }

    std::unique_ptr<::mega::MegaNode> getPublicMegaNode() const
    {
        return (finished() && request) ? std::unique_ptr<::mega::MegaNode>(request->getPublicMegaNode()) : nullptr;
    }

    std::string getText() const
    {
        return (finished() && request && request->getText()) ? request->getText() : std::string();
    }

private:
    std::unique_ptr<::mega::MegaRequest> request;
    mega::MegaApi* mApi;
};

class ChatRequestTracker : public megachat::MegaChatRequestListener, public ResultHandler
{
public:
    ChatRequestTracker(megachat::MegaChatApi* megaChatApi)
        : mMegaChatApi(megaChatApi)
    {
    }

    ~ChatRequestTracker()
    {
        if (!resultReceived)
        {
            mMegaChatApi->removeChatRequestListener(this);
        }
    }

    void onRequestFinish(::megachat::MegaChatApi*, ::megachat::MegaChatRequest* req,
                         ::megachat::MegaChatError* e) override
    {
        request.reset(req ? req->copy() : nullptr);
        finish(e->getErrorCode(), e->getErrorString() ? e->getErrorString() : "");
    }

    std::string getText() const
    {
        return (finished() && request && request->getText()) ? request->getText() : std::string();
    }

    long long getNumber() const
    {
        return (finished() && request) ? request->getNumber() : 0;
    }

    bool getFlag() const
    {
        return (finished() && request) ? request->getFlag() : false;
    }

    ::megachat::MegaChatHandle getChatHandle() const
    {
        return (finished() && request) ? request->getChatHandle() : ::megachat::MEGACHAT_INVALID_HANDLE;
    }

    int getParamType() const
    {
        return (finished() && request) ? request->getParamType() : 0;
    }

    int getPrivilege() const
    {
        return (finished() && request) ? request->getPrivilege() : 0;
    }

    bool hasScheduledMeetings() const
    {
        return finished() && request
                && request->getMegaChatScheduledMeetingList()
                && request->getMegaChatScheduledMeetingList()->size();
    }

    bool hasScheduledMeetingOccurrList() const
    {
        return finished() && request && request->getMegaChatScheduledMeetingOccurrList();
    }

    std::unique_ptr<::megachat::MegaChatScheduledMeetingOccurrList> getScheduledMeetingsOccurrences() const
    {
        return hasScheduledMeetingOccurrList()
                  ? std::unique_ptr<::megachat::MegaChatScheduledMeetingOccurrList>(request->getMegaChatScheduledMeetingOccurrList()->copy())
                  : nullptr;
    }

private:
    std::unique_ptr<::megachat::MegaChatRequest> request;
    megachat::MegaChatApi* mMegaChatApi;
};

class ChatLogoutTracker : public ::megachat::MegaChatRequestListener, public ResultHandler
{
public:
    void onRequestFinish(::megachat::MegaChatApi*, ::megachat::MegaChatRequest* req,
                         ::megachat::MegaChatError* e) override
    {
        if (req && req->getType() == ::megachat::MegaChatRequest::TYPE_LOGOUT)
        {
            finish(e->getErrorCode(), e->getErrorString() ? e->getErrorString() : "");
        }
    }
};

#ifndef KARERE_DISABLE_WEBRTC
class MockupCall : public sfu::SfuInterface
{
public:
    bool handleAvCommand(Cid_t cid, unsigned av, uint32_t amid) override;
    bool handleAnswerCommand(Cid_t cid, std::shared_ptr<sfu::Sdp> sdp, uint64_t callJoinOffset, std::vector<sfu::Peer>& peers, const std::map<Cid_t, std::string>& keystrmap, const std::map<Cid_t, sfu::TrackDescriptor>& vthumbs, const std::map<Cid_t, sfu::TrackDescriptor>& speakers) override;
    bool handleKeyCommand(const Keyid_t& keyid, const Cid_t& cid, const std::string&key) override;
    bool handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &) override;
    bool handleVThumbsStartCommand() override;
    bool handleVThumbsStopCommand() override;
    bool handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor> &) override;
    bool handleHiResStartCommand() override;
    bool handleHiResStopCommand() override;
    bool handleSpeakReqsCommand(const std::vector<Cid_t>&) override;
    bool handleSpeakReqDelCommand(Cid_t cid) override;
    bool handleSpeakOnCommand(Cid_t cid) override;
    bool handleSpeakOffCommand(Cid_t cid) override;
    bool handlePeerJoin(Cid_t cid, uint64_t userid, sfu::SfuProtocol sfuProtoVersion, int av, std::string& keyStr, std::vector<std::string>& ivs) override;
    bool handlePeerLeft(Cid_t cid, unsigned termcode) override;
    bool handleBye(const unsigned termCode, const bool wr, const std::string& errMsg) override;
    bool handleModAdd(uint64_t userid) override;
    bool handleModDel(uint64_t userid) override;
    void onByeCommandSent() override;
    void onSfuDisconnected() override;
    bool error(unsigned int, const std::string &) override;
    bool processDeny(const std::string&, const std::string&) override;
    void logError(const char* error) override;
    bool handleHello(const Cid_t cid, const unsigned int nAudioTracks,
                     const std::set<karere::Id>& mods, const bool wr, const bool allowed,
                     const bool speakRequest, const sfu::WrUserList& wrUsers) override;

    bool handleWrDump(const sfu::WrUserList& wrUsers) override;
    bool handleWrEnter(const sfu::WrUserList& wrUsers) override;
    bool handleWrLeave(const karere::Id& user) override;
    bool handleWrAllow(const Cid_t& cid, const std::set<karere::Id>& mods) override;
    bool handleWrDeny(const std::set<karere::Id>& mods) override;
    bool handleWrUsersAllow(const std::set<karere::Id>& users) override;
    bool handleWrUsersDeny(const std::set<karere::Id>& users) override;
    bool handleMutedCommand(const unsigned av, const Cid_t /*cidPerf*/) override;
};
#endif
#endif // CHATTEST_H
