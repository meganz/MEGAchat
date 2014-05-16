#ifndef GUICALLMARSHALLER_H
#define GUICALLMARSHALLER_H
#include <utility>

namespace mega
{

/** The common base message type that gets passed through the app's native GUI
 * message loop. Any memory associated with the message object can be managed only
 * by the shared object that created it, because each shared object is likely to
 * have its own memory management. This includes both directly allocating/freeing
 * members of the message, or calling any methods of members that may try to
 * do memory management on memory associated with the message. For example -
 * method calls on STL container members of the message.
 * Further - if the shared object that created the message and the one that accesses
 * it can be built with different compilers, the C++ ABIs must be considered incompatible.
 * Therefore, only the header part of the message can be accessed, up till a C++ member
 * appears.
*/
struct Message
{
	enum _magic: unsigned long {MEGAMSG_MAGIC = 0x3e9a3591};
	typedef void(*HandlerFunc)(void*);
/** Not required by the messaging mechanism itself, but might be useful
 * when the same handler is called with different kinds of messages
 */
	int type;
/** Handler C function that the message will be sent to, when received by the GUI thread
 * Dispatching messages by directly specifying the handler function allows
 * to dispatch the same kind of message to the correct module(shared object/main app)
 * so that access/memory management on the message is always performed by the module
 * that created the message.
 */
	HandlerFunc handler;
	const unsigned long magic;
	Message(HandlerFunc	aHandler, int aType=0)
		:handler(aHandler), type(aType), magic(MEGAMSG_MAGIC){}
	static inline void verify(Message* msg)
	{
		if (msg->magic != MEGAMSG_MAGIC)
			throw std::runtime_error("Message does not have the correct magic value");
		if (!handler)
			throw std::runtime_error("Message has a NULL handler");
	}
};

/**
* Creates a message that posts a lambda (i.e. std::function<void()>) object
* to the GUI thread.
*/
struct FuncCallMessage: public Message
{
	typedef std::function<void()> Lambda;
	Lambda lambda;
	FuncCallMessage(Message::HandlerFunc aHandler, Lambda&& aLambda, int aType = 0)
		:Message(aHandler, aType), lambda(std::forward<Lambda>(aLambda))
	{}

	static inline doCall(Message* msg)
	{
		std::unique_ptr<FuncCallMessage> fcallMsg(static_cast<FuncCallMessage*>(msg));
		fcallMsg->lambda();
	}
};

/** Must be provided by the user.
* Wraps an opaque void*, poiting to a mega message, into a
* platform inter-thread message/signal and posts it to the GUI
* message loop. This function is called by various threads
*/
void postMessageToGui(void* msg);

/** Utility function that uses the messaging infrastructure to marshal a (lambda) function call
 * to the GUI thread */
static inline void marshalCall(Message::Handler handler, std::function<void()>&& call, int type=0)
{
	FuncCallMessage* msg = new FuncCallMessage(handler, std::forward<std::function<void()> >(call), type);
	postMessageToGui((void*)msg); //platform-specific, user-defined
}

/** This function must be called by the user when a Mega Message has been received
* by the GUI thread
* It passes a received by the GUI thread to the MegaSDK for processing.
* \warning Must be called only from the GUI thread
*/
void processMessage(void* voidPtr)
{
	Message* msg = static_cast<Message*>(voidPtr);
	Message::verify(msg);
	msg->handler(msg);
}

}

#endif // GUICALLMARSHALLER_H
