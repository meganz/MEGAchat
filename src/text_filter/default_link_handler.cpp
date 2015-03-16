/*
 * default_link_handler.cpp
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#include "default_link_handler.h"
#include "shared_buffer.h"
#include "text_handler.h"

#include <sstream>

namespace karere {

DefaultLinkHandler::DefaultLinkHandler() {
    symbolVector->push_back("\\bhttp://[A-Za-z0-9.-@!$'()*+,;=]*\\b");
    symbolVector->push_back("\\bwww\\.[A-Za-z0-9.-@!$'()*+,;=]*\\b");
}

DefaultLinkHandler::~DefaultLinkHandler() {

}

void
DefaultLinkHandler::handleData(RenderInfo &info) {
    if(info.rawData.size() == 0) {
        throw HandlerException(NO_MATCH_DATA);
    }

    std::stringstream stream;
    stream << "<a href=\""
           << info.rawData
           << "\">"
           << info.rawData
           << "<\\a>";
    info.html = stream.str();
}

} /* namespace stk_tlv_store */
