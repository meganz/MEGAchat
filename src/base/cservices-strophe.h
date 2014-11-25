#ifndef CSERVICESSTROPHE_H
#define CSERVICESSTROPHE_H

//#ifndef XMPP_NS_CLIENT
//   struct xmpp_ctx_t;
//#endif
#include <mstrophe.h>

//This header is not standalone - it must be included by cservices.h

enum {SVC_STROPHE_LOG = 1};
MEGAIO_IMPEXP int services_strophe_init(int options);
MEGAIO_IMPEXP xmpp_ctx_t* services_strophe_get_ctx();
MEGAIO_IMPEXP int services_strophe_shutdown();

#endif // CSERVICESSTROPHE_H
