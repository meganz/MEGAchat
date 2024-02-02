#pragma once

#include "megachatapi.h"
#include <string_view>
namespace c = ::megachat;


namespace mclc::clc_ccactions
{

std::pair<c::MegaChatHandle, int> openChatLink(const std::string& link);

bool joinChat(const c::MegaChatHandle chatId, const int errCode);

bool waitUntilCallIsReceived(const c::MegaChatHandle chatId);

bool answerCall(const c::MegaChatHandle chatId, const bool audio, const bool video);

bool hangUpCall(const c::MegaChatHandle chatId);

}
