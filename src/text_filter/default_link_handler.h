/*
 * default_link_handler.h
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#ifndef DEFAULT_LINK_HANDLER_H_
#define DEFAULT_LINK_HANDLER_H_

#include "shared_buffer.h"
#include "text_handler.h"

namespace karere {

/**
 * @brief Default implementation of TextHandler that handles
 * links.
 */
class DefaultLinkHandler : public TextHandler {
public:

    /**
     * @brief No-arg constructor for DefaultLinkHandler.
     */
    DefaultLinkHandler();

    /**
     * @brief Destructor for DefaultLinkHandler.
     */
    virtual ~DefaultLinkHandler();

    /**
     * @brief Implementation of handleData from abstract class Handler.
     *
     * The html field in info is populated with an html link tag
     * with the reference set to the provided link.
     *
     * @param link The data to process for this handler.
     */
    void handleData(RenderInfo &info);
};

} /* namespace stk_tlv_store */

#endif /* DEFAULT_LINK_HANDLER_H_ */
