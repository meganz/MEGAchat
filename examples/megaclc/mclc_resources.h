#pragma once

namespace mclc::clc_resources
{

/**
 * @brief Allocates and setups al the requirements for running the app main loop. This includes the loggers, the apis,
 * etc. This should be called before the main event loop.
 */
void appAllocate();

/**
 * @brief Releases all the resources allocated bye the appAllocate function. This should be called before exiting the
 * app.
 */
void appClean();

}
