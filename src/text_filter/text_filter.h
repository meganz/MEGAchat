/*
 * mega_text_filter.h
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#ifndef MEGA_TEXT_FILTER_H_
#define MEGA_TEXT_FILTER_H_

#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <regex>
#include <deque>
#include <functional>

#include "shared_buffer.h"
#include "default_emoticon_handler.h"
#include "default_link_handler.h"
#include "filter_common.h"
#include "text_handler.h"

#ifdef __TEST
#include <gtest/gtest.h>
#endif

namespace karere {

typedef std::function<void(const std::string&)> AlertFunction;

/**
 * @brief Typedef for a map of string : HandlerFunctions.
 */
typedef std::map<std::string, SharedHandler> SymbolHandlerMap;

/**
 * @brief Typedef for a map of handler_type : Handler.
 */
typedef std::map<handler_type, SharedHandler> TypeHandlerMap;

/**
 * @brief TextFilter class, handles determination of the correct handler
 * to call for a given symbol if it is present in the given buffer.
 */
class TextFilter {

#ifdef __TEST
    FRIEND_TEST(TextFilterTest, test_default_setup);
#endif

public:

    /**
     * @brief Single argument constructor for TextFilter.
     *
     * @param setDefaults If true, default handlers are setup for this
     *                    TextFilter.
     */
    TextFilter(bool defaults = true);

    /**
     * @brief Set the behavior when an exception is thrown
     * by a handler.
     *
     * If this is set to true, then an exception thrown by a
     * handler will cease processing of a given string
     * and an exception will be thrown.
     *
     * @param bork The value to set borkOnHandlerExceptionThrown.
     */
    void setBorkOnHandlerException(bool bork);

    /**
     * @brief Get the current value for borkOnHandlerExceptionThrown.
     *
     * If this is true, then an exception thrown by a handler will
     * cease the current processing of a given string.
     *
     * @return If true, then this handler will cease processing on
     * exception.
     */
    bool getBorkOnHandlerException();

    /**
     * @brief Register a handler for the given symbol.
     *
     * This is used to register handlers for a given function. Currently
     * there is only one handler for a given symbol - this may change in
     * the future.
     *
     * @param symbol The symbol to register for.
     * @param function The function to register.
     * @return 0 on success, < 0 on failure.
     */
    int registerHandler(SharedHandler &handler);

    /**
     * @brief Handle a data string.
     *
     * The data to be filtered is passed in as a shared buffer.
     *
     * @param data The data to handle.
     * @return 0 on success, < 0 on failure.
     */
    inline int handleData(const std::string &data);

    /**
     * @brief Search the given string for matches and put the results into
     * the given vector.
     *
     * @param infoVec The vector to populate with matches.
     * @param data The data to search for matches.
     * @return 0 on success, < 0 on failure.
     */
    int handleData(std::vector<RenderInfo> &infoVec, const std::string &data);

    /**
     * @brief Add an observer to be alerted when
     */
    void addObserver(AlertFunction function);

private:

    /**
     * @brief Process the vector of renderinfo once symbol matching is complete.
     *
     * @param infoVec The vector of RenderInfo to process.
     */
    void processData(std::vector<RenderInfo> &infoVec, const std::string &data);

    /**
     * @brief Setup the default handlers for this TextFilter.
     *
     * This sets the default emoticon and link handlers for this
     * TextFilter.
     */
    void setDefaults();

    /**
     * @brief The map of symbol : handler functions.
     */
    SymbolHandlerMap handlerMap;

    /**
     * @brief If set to true, then an exception on a given handler
     * will stop the processing of the given data.
     */
    bool borkOnHandlerException;

    /**
     * @brief The map of HandlerType : Handler map.
     */
    TypeHandlerMap typeHandlerMap;

    /**
     * @brief The output for the final compiled text.
     */
    std::vector<AlertFunction> alertVector;

};

int
TextFilter::handleData(const std::string &data) {
    std::vector<RenderInfo> infoVec;
    handleData(infoVec, data);
    if(infoVec.size() > 0) {
        processData(infoVec, data);
    }
    else {
        // Alert all of the listeners.
        for(auto &o : alertVector) {
            o(data);
        }
    }

    return 0;
}

} /* namespace karere */

#endif /* MEGA_TEXT_FILTER_H_ */
