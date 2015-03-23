/*
 * default_emoticon_handler.h
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#ifndef DEFAULT_EMOTICON_HANDLER_H_
#define DEFAULT_EMOTICON_HANDLER_H_

#include <shared_buffer.h>
#include "text_handler.h"

#include <map>

namespace karere {

// defines for the default smileys and file names used
// in DefaultEmoticonHandler.
#define HAPPY_SMILEY ":-\\)"
#define SAD_SMILEY ":-\\("
#define NEUTRAL_SMILEY ":-\\|"
#define EXTATIC_SMILEY ":-D"

#define HAPPY_SMILEY_PNG_NAME "happy_smiley.png"
#define SAD_SMILEY_PNG_NAME "sad_smiley.png"
#define NEUTRAL_SMILEY_PNG_NAME "neutral_smiley.png"
#define EXTATIC_SMILEY_PNG_NAME "extatic_smiley.png"

// This is the error reported if an icon is not found.
#define NO_ICON_FOR_MATCH "No icon for given match found."

/**
 * @brief Default implementation of TextHandler that handles
 * emoticons.
 */
class DefaultEmoticonHandler : public TextHandler {
public:

    /**
     * @brief The default root path for finding smileys.
     */
    static const std::string DEFAULT_ROOT_PATH;

    /**
     * @brief No-arg constructor for DefaultEmoticon.
     */
    DefaultEmoticonHandler();

    /**
     * @brief Destructor for DefaultEmoticonHandler.
     */
    virtual ~DefaultEmoticonHandler();

    /**
     * @brief Concrete implementation of the abstract member function
     * handleData.
     *
     * This populates the field html in the info object passed in with
     * the appropriate html for the given emoticon.
     *
     * @param info The info to handle.
     * @throws HandlerException The exception thrown on error.
     */
    void handleData(RenderInfo &info);

    /**
     * @brief Set the root path where the emoticon images can be found.
     *
     * The given path should exist. However it is not up to the
     * DefaultEmoticon to test this path.
     *
     * @param path The path where the emoticons can be found.
     */
    void setRootPath(std::string &path);

private:

    /**
     * @brief The map of icons to icon image names.
     */
    std::map<std::string, std::string> iconPathMap;

    /**
     * @brief The root path where the images for the icons are kept.
     */
    std::string rootPath;
};

} /* namespace stk_tlv_store */

#endif /* DEFAULT_EMOTICON_HANDLER_H_ */
