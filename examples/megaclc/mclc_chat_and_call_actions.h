/**
 * @file
 * @brief This file is supposed to hold a set of high level functions that perform different actions
 * related to chats and calls.
 */

#pragma once

#include "megachatapi.h"

#include <set>
#include <string_view>
namespace c = ::megachat;

namespace mclc::clc_ccactions
{

/**
 * @brief Given a link to a public chat, this function opens it.
 *
 * NOTE: To call this function the initial state (g_chatApi->getInitState()) must be one of
 * INIT_ONLINE_SESSION or INIT_ANONYMOUS.
 *
 * @param link url to the public chat.
 * @return A std::pair with the chat handle as the first value and the error code associated to the
 * openChatPreview chat api request (to be compare for example with
 * megachat::MegaChatError::ERROR_OK) as second. If something went wrong, the chat handle will hold
 * the MEGACHAT_INVALID_HANDLE value.
 */
std::pair<c::MegaChatHandle, int> openChatLink(const std::string& link);

/**
 * @brief Joins your account to a chat that was previously opened as preview.
 *
 * @param chatId Chat handle
 * @param openPreviewErrCode The error code returned by the openChatPreview request (the one that
 * returns the openChatLink function).
 * @return true if everything went OK, false otherwise.
 */
bool joinChat(const c::MegaChatHandle chatId, const int openPreviewErrCode);

/**
 * @brief Blocks the execution until a call is received. This function assumes you participate in
 * chat whose chatId is provided as param.
 *
 * @param chatId The chat handle
 * @return true if everything went OK, false otherwise.
 */
bool waitUntilCallIsReceived(const c::MegaChatHandle chatId);

/**
 * @brief Starts a call in the chat room with the given handle.
 *
 * @param chatId Chat handle
 * @param audio Enable audio
 * @param video Enable video
 * @param notRinging If call rings for the invited people. If the call rings and you take it you go
 * directly to the call. Otherwise you will be taken to the waiting room.
 * @return true if everything went OK, false otherwise.
 */
bool startChatCall(const c::MegaChatHandle chatId,
                   const bool audio,
                   const bool video,
                   const bool notRinging);

/**
 * @brief Answers the ongoing call.
 *
 * @param chatId The chat handle
 * @param audio  If true we want to answer call with audio enabled (Check
 * MegaChatCall::hasPermissionToSpeak)
 * @param video  If true we want to answer call with video enabled (Check
 * MegaChatCall::hasPermissionToSpeak)
 * @return true if everything went OK, false otherwise.
 */
bool answerCall(const c::MegaChatHandle chatId,
                const bool audio,
                const bool video,
                const std::set<int>& expectedStatus);

/**
 * @brief Hangs up the ongoing call you are in.
 *
 * @param chatId The chat handle
 * @return true if everything went OK, false otherwise.
 */
bool hangUpCall(const c::MegaChatHandle chatId);

/**
 * @brief Tries to set the video input device with the given one
 *
 * @param device The video device to set
 * @return true if it was set properly false otherwise.
 */
bool setChatVideoInDevice(const std::string& device);

}
