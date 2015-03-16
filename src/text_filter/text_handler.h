/*
 * handler.h
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#ifndef TEXT_HANDLER_H_
#define TEXT_HANDLER_H_

#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <exception>

#include "shared_buffer.h"

namespace karere {

#define NO_MATCH_DATA "No match data present."

/**
 * @brief Exception thrown by a handler if there is an error processing
 * the given data.
 *
 * This may not be necessary, but I have created a sub-class in the case
 * that we want to add further information to the exception in the
 * constructor (i.e. and error code).
 *
 * Classes implementing TextHandler should throw this exception on error.
 */
class HandlerException : public std::runtime_error {
public:

    /**
     * @brief Constructor that creates an exception with the given error
     * description string.
     */
    explicit HandlerException(const std::string &what_arg) :
            std::runtime_error(what_arg) {}

    /**
     * @biref Constructor that creates an exception with the given error
     * description string.
     */
    explicit HandlerException(const char *what_arg) :
            std::runtime_error(what_arg) {}
};

/**
 * @brief Struct with information on how to render the processed string.
 */
struct RenderInfo {
    std::string matchData;  ///< The regex data that was matched.
    std::string rawData;    ///< The data that was found to match.
    long int position;      ///< The position the data was found.
    std::string html;       ///< The resulting text in html format.
};

/**
 * @brief The typedef for the function to handle symbol decoding.
 */
typedef std::function<void(mpenc_cpp::SharedBuffer&)> HandlerFunction;

/**
 * @brief the typedef for the vector of symbols (strings, regex) that
 * handlers handle.
 */
typedef std::shared_ptr<std::vector<std::string>> SymbolVector;

/**
 * @brief Interface for handling text for a given symbol.
 *
 * Implementing classes extend this and create the concrete version
 * of handleData. This should provide a html translation of the given
 * sypmbol. This should also throw a HandlerException on error.
 */
class TextHandler {

public:

    /**
     * @brief The no-arg constructor for TextHandler.
     */
    TextHandler() {
        symbolVector = SymbolVector(new std::vector<std::string>());
    }

    /**
     * @brief The destructor for TextHandler.
     */
    virtual ~TextHandler() {}

    /**
     * @brief Handle the given info.
     *
     * @param info The RenderInfo with the data to be handled.
     * @throws HandlerException Errors should be reported by throwing this
     * exception.
     */
    virtual void handleData(RenderInfo &info) = 0;

    /**
     * @brief Get the FunctionVector for this TextHandler.
     *
     * @return The vector with all of the symbols that this handler handles.
     */
    virtual SymbolVector getFunctionVector() {
        return symbolVector;
    };

protected:

    /**
     * @brief The map of symbols that this handler handles.
     */
    SymbolVector symbolVector;
};

/**
 * @brief typedef for shared_ptr for handlers.
 */
typedef std::shared_ptr<TextHandler> SharedHandler;

} /* namespace karere */

#endif /* TEXT_HANDLER_H_ */
