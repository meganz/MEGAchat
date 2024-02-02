#pragma once

#include <megachatapi.h>
namespace c = ::megachat;


namespace mclc::clc_report
{
static const int MAX_NUMBER_MESSAGES = 100; // chatd doesn't allow more than 256

void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid);
void reportMessageHuman(c::MegaChatHandle chatid, c::MegaChatMessage *msg, const char* loadorreceive);
void reportMessage(c::MegaChatHandle chatid, c::MegaChatMessage *msg, const char* loadorreceive);
}

