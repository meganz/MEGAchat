/* This source file must be included in shared objects that want to use the
 * gui call marshaller mechanism's megaPostMessageToGui function
 * via a function ppointer passed to an init function rather than dynamically link
 * to the function in the main app executable
 */

#ifdef MEGA_GCM_CLIENT_INIT_API
#include "guiCallMarshaller.h"


megaGcmPostFunc megaPostMessageToGui = (void*)0;

MEGA_GCM_EXPORT int MEGA_GCM_MAKESYM(megaGcmInit_, MEGA_GCM_CLIENT_INIT_API)
(megaGcmPostFunc postFunc)
{
    megaPostMessageToGui = postFunc;
    return 0;
}
#endif
