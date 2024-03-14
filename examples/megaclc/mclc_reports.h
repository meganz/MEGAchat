/**
 * @file
 * @brief This file contains some useful functions to make reports, e.g. a summary of a public chat,
 * process and organize a set of chat messages, etc.
 */

#pragma once

#include <megachatapi.h>
namespace c = ::megachat;

namespace mclc::clc_report
{
static const int MAX_NUMBER_MESSAGES = 100; // chatd doesn't allow more than 256

/**
 * @brief Writes to a file the contents of the chat with the given chat handle. The file is the one
 * specified in the g_reviewPublicChatOutFile global variable.
 */
void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid);

/**
 * @brief Extracts and organize relevant information inside msg and prints it to the cout and also
 * to the g_reviewPublicChatOutFile file.
 *
 * @param chatid The chat to extract the messages from
 * @param msg The messages
 * @param loadorreceive A text with information about the reception of the message.
 */
void reportMessageHuman(c::MegaChatHandle chatid,
                        c::MegaChatMessage* msg,
                        const char* loadorreceive);
void reportMessage(c::MegaChatHandle chatid, c::MegaChatMessage* msg, const char* loadorreceive);
}
