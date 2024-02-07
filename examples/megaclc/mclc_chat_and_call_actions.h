/**
 * @file
 * @brief This file is supposed to hold a set of high level functions that perform different actions related to chats
 * and calls.
 */

#pragma once

#include "megachatapi.h"
#include <string_view>
namespace c = ::megachat;


namespace mclc::clc_ccactions
{

/**
 * @brief Given a link to a public chat, this function opens it.
 *
 * NOTE: You need to be logged in to call this function
 *
 * @param link url to the public chat.
 * @return A std::pair with the chat handle as the first value and the error code associated to the openChatPreview chat
 * api request (to be compare for example with megachat::MegaChatError::ERROR_OK) as second. If something went wrong,
 * the chat handle will hold the MEGACHAT_INVALID_HANDLE value.
 */
std::pair<c::MegaChatHandle, int> openChatLink(const std::string& link);

/**
 * @brief Joins your account to a chat that was previously opened as preview.
 *
 * @param chatId Chat handle
 * @param errCode The error code returned by the openChatPreview request (the one that returns the openChatLink
 * function).
 * @return true if everything went OK, false otherwise.
 */
bool joinChat(const c::MegaChatHandle chatId, const int errCode);

/**
 * @brief Blocks the execution until a call is received. This function assumes you are joined to a chat.
 *
 * @param chatId The chat handle
 * @return true if everything went OK, false otherwise.
 */
bool waitUntilCallIsReceived(const c::MegaChatHandle chatId);

/**
 * @brief Answers the ongoing call.
 *
 * @param chatId The chat handle
 * @param audio  If true the audio is activated for the call.
 * @param video  If true the video is activated for the call.
 * @return true if everything went OK, false otherwise.
 */
bool answerCall(const c::MegaChatHandle chatId, const bool audio, const bool video);

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
