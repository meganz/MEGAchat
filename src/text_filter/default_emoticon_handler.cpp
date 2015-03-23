/*
 * default_emoticon_handler.cpp
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#include "default_emoticon_handler.h"
#include "shared_buffer.h"

#include <sstream>

namespace karere {

const std::string DefaultEmoticonHandler::DEFAULT_ROOT_PATH("./");

DefaultEmoticonHandler::DefaultEmoticonHandler() {
    symbolVector->push_back(std::string(HAPPY_SMILEY));
    symbolVector->push_back(std::string(SAD_SMILEY));
    symbolVector->push_back(std::string(NEUTRAL_SMILEY));
    symbolVector->push_back(std::string(EXTATIC_SMILEY));

    iconPathMap.insert({std::string(HAPPY_SMILEY), std::string(HAPPY_SMILEY_PNG_NAME)});
    iconPathMap.insert({std::string(SAD_SMILEY), std::string(SAD_SMILEY_PNG_NAME)});
    iconPathMap.insert({std::string(NEUTRAL_SMILEY), std::string(NEUTRAL_SMILEY_PNG_NAME)});
    iconPathMap.insert({std::string(EXTATIC_SMILEY), std::string(EXTATIC_SMILEY_PNG_NAME)});

    rootPath = DEFAULT_ROOT_PATH;
}

DefaultEmoticonHandler::~DefaultEmoticonHandler() {

}

void
DefaultEmoticonHandler::handleData(RenderInfo &info) {
    if(info.rawData.size() == 0) {
        throw HandlerException(NO_MATCH_DATA);
    }

    auto p = iconPathMap.find(info.matchData);
    if(p == iconPathMap.end()) {
        throw HandlerException(NO_ICON_FOR_MATCH);
    }

    std::stringstream stream;
    stream << "<img src=\""
           << rootPath
           << p->second
           << "\">";

    info.html = stream.str();
}

void
DefaultEmoticonHandler::setRootPath(std::string &path) {
    rootPath = path;
}

} /* namespace karere */
