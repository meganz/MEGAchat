#ifndef MCLC_RESOURCES_H
#define MCLC_RESOURCES_H

#include <map>
#include <string>
#include <vector>

namespace mclc::clc_resources
{

/**
 * @brief Removes the arguments associated to global options from the input vector. Then they are
 * used to set options that control the allocation and cleaning of the app.
 *
 * @param args A vector with all the parameters received by the main function
 */
void extractGlobalOptions(std::vector<std::string>& args);

/**
 * @brief Returns a map with the flag associated to each available global option as key and its
 * description as value.
 */
std::map<std::string, std::string> getAvailableGlobalOptionsDescription();

/**
 * @brief Allocates and sets up al the requirements for running the app main loop. This includes the
 * loggers, the apis, etc. This should be called before the main event loop.
 */
void appAllocate();

/**
 * @brief Releases all the resources allocated bye the appAllocate function. This should be called
 * before exiting the app.
 */
void appClean();

}
#endif // MCLC_RESOURCES_H
