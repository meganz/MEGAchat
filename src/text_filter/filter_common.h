/*
 * common.h
 *
 *  Created on: 23/01/2015
 *      Author: michael
 */

#ifndef FILTER_COMMON_H_
#define FILTER_COMMON_H_

typedef unsigned char byte;

/**
 * @brief The type of handler.
 *
 * This is used when registering callbacks for handlers.
 */
typedef enum {
    HT_EMOTICON_HANDLER, ///< Emoticon handler type.
    HT_LINK              ///< Link handler type.
} handler_type;

#endif /* FILTER_COMMON_H_ */
