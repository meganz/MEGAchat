#pragma once

#include <mclc_listeners.h>
#include <mclc_prompt.h>

#include <mega/autocomplete.h>
#include <megaapi.h>
#include <megachatapi.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <map>

namespace mclc::clc_global
{

extern bool g_detailHigh;

extern std::atomic<bool> g_reportMessagesDeveloper;

extern std::atomic<bool> g_reviewingPublicChat;
extern std::atomic<bool> g_dumpingChatHistory;
extern std::atomic<bool> g_startedPublicChatReview;
extern std::atomic<int> g_reviewChatMsgCountRemaining;
extern std::atomic<unsigned int> g_reviewChatMsgCount;
extern std::atomic<::megachat::MegaChatHandle> g_reviewPublicChatid;
extern std::atomic<::megachat::MegaChatHandle> g_dumpHistoryChatid;
extern std::unique_ptr<std::ofstream> g_reviewPublicChatOutFile;
extern std::unique_ptr<std::ofstream> g_reviewPublicChatOutFileLinks;
extern std::mutex g_reviewPublicChatOutFileLogsMutex;
extern std::unique_ptr<std::ofstream> g_reviewPublicChatOutFileLogs;

// APIs
extern std::unique_ptr<::mega::MegaApi> g_megaApi;
extern std::unique_ptr<::megachat::MegaChatApi> g_chatApi;

// Listeners
extern std::map<::megachat::MegaChatHandle, clc_listen::CLCRoomListenerRecord> g_roomListeners;
extern std::map<::megachat::MegaChatHandle, clc_listen::CLCStateChange> g_callStateMap;
extern clc_listen::CLCListener g_clcListener;
extern clc_listen::CLCCallListener g_clcCallListener;
extern clc_listen::CLCMegaListener g_megaclcListener;
extern clc_listen::CLCChatListener g_chatListener;
extern clc_listen::CLCMegaGlobalListener g_globalListener;

// output
extern std::mutex g_outputMutex; // lock this for output since we are using cout on multiple threads

// Prompt
extern clc_prompt::prompttype g_prompt; // The state of the prompt
extern char* g_promptLine;                // The input line in the command line
extern int g_promptPwBufPos;
extern std::unique_ptr<m::Console> g_console;
extern bool g_promptQuitFlag;
// Autocomplete
extern mega::autocomplete::ACN g_autocompleteTemplate;

// Login
extern std::string g_login;
extern std::string g_password;

// only add, never remove.  To invalidate one, set it null.
extern std::vector<std::unique_ptr<m::MegaCancelToken>> g_cancelTokens;

// Signaling
extern int g_signalPresencePeriod;
extern time_t g_signalPresenceLastSent;

extern int g_repeatPeriod;
extern time_t g_repeatLastSent;
extern std::string g_repeatCommand;
}
