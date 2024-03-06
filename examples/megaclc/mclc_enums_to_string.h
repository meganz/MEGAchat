#ifndef MCLC_ENUMS_TO_STRING_H
#define MCLC_ENUMS_TO_STRING_H

/**
 * @file
 * @brief Some utilities to convert enums into strings and viceversa
 *
 * The clc_etos namespace stands for enum-to-string
 */

#include <string>

namespace mclc::clc_etos
{

/**
 * @brief Converts a megachat::MegaChatMessage::TYPE_* into a descriptive label
 *
 * @param msgType an instance of the enum defined inside MegaChatMessage class with the messages
 * types.
 * @returns The associated label.
 */
std::string msgTypeToString(const int msgType);

/**
 * @brief Converts a megachat::MegaChatMessage::STATUS_* into a descriptive label
 *
 * @param msgStatus an instance of the enum defined inside MegaChatMessage class with the messages
 * status.
 * @returns The associated label.
 */
std::string msgStatusToString(const int msgStatus);

}
#endif // MCLC_ENUMS_TO_STRING_H
