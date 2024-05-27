#include "mclc_globals.h"

namespace mclc::clc_global
{

bool g_detailHigh{false};

std::atomic<bool> g_reportMessagesDeveloper{false};

std::atomic<bool> g_allChatsLoggedIn{false};
std::atomic<bool> g_chatFinishedLogout{false};

std::atomic<bool> g_reviewingPublicChat{false};
std::atomic<bool> g_dumpingChatHistory{false};
std::atomic<bool> g_startedPublicChatReview{false};
std::atomic<int> g_reviewedChatLoggedIn{false};
std::atomic<bool> g_reviewChatLoadAllMsg{false};
std::atomic<int> g_reviewChatMsgCountRemaining{-1};
std::atomic<unsigned int> g_reviewChatMsgCount{0};
std::atomic<::megachat::MegaChatHandle> g_reviewPublicChatid{::megachat::MEGACHAT_INVALID_HANDLE};
std::atomic<::megachat::MegaChatHandle> g_dumpHistoryChatid{::megachat::MEGACHAT_INVALID_HANDLE};
std::unique_ptr<std::ofstream> g_reviewPublicChatOutFile;
std::unique_ptr<std::ofstream> g_reviewPublicChatOutFileLinks;
std::mutex g_reviewPublicChatOutFileLogsMutex;
std::unique_ptr<std::ofstream> g_reviewPublicChatOutFileLogs;

// APIs
std::unique_ptr<::mega::MegaApi> g_megaApi;
std::unique_ptr<::megachat::MegaChatApi> g_chatApi;

// Listeners
std::map<::megachat::MegaChatHandle, clc_listen::CLCRoomListenerRecord> g_roomListeners;
std::map<::megachat::MegaChatHandle, clc_listen::CLCStateChange> g_callStateMap;
clc_listen::CLCListener g_clcListener;
#ifndef KARERE_DISABLE_WEBRTC
clc_listen::CLCCallListener g_clcCallListener;
#endif
clc_listen::CLCMegaListener g_megaclcListener;
clc_listen::CLCChatListener g_chatListener;
clc_listen::CLCMegaGlobalListener g_globalListener;
clc_listen::CLCChatRequestListener g_chatRequestListener;
clc_report::CLCCallReceivedVideos g_callVideoParticipants;

// output
std::mutex g_outputMutex;

// Prompt
clc_prompt::prompttype g_prompt{clc_prompt::prompttype::COMMAND};
char* g_promptLine = nullptr;
int g_promptPwBufPos;
std::unique_ptr<m::Console> g_console;
bool g_promptQuitFlag{false};

// Autocomplete
mega::autocomplete::ACN g_autocompleteTemplate;

// Login
std::string g_login;
std::string g_password;

// Tokens
std::vector<std::unique_ptr<m::MegaCancelToken>> g_cancelTokens;

// Signaling
int g_signalPresencePeriod = 0;
time_t g_signalPresenceLastSent = 0;

int g_repeatPeriod = 5;
time_t g_repeatLastSent = 0;
std::string g_repeatCommand;

}
