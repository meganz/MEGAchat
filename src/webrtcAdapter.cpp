#include "guiCallMarshaller.h"
#include "webrtcAdapter.h"

namespace rtcModule
{

/** Global PeerConnectionFactory that initializes and holds a webrtc runtime context*/
talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
 gWebrtcContext;

/** Local DTLS Identity */
Identity gLocalIdentity;
/** Stream id and other ids generator */
unsigned long generateId()
{
	static unsigned long id = 0;
	return ++id;
}

void funcCallMarshalHandler(mega::Message* msg)
{
	mega::FuncCallMessage::doCall(msg);
}
}
