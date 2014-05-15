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
/** Message types, based on these the message is dispatched to different handlers */
	enum Type {MMSG_FCALL = 1, MMSG_CURLIO = 2, MMSG_XMPPIO = 3};
	int type;
	const unsigned long magic;
	Message(int aType):type(aType), magic(MEGAMSG_MAGIC){}
	static inline void verifyMagic(Message* msg)
	{
		if (msg->magic != MEGAMSG_MAGIC)
			throw std::runtime_error("Message does not have the correct magic value");
	}
};

/**
* Creates a message that posts a lambda (i.e. std::function<void()>) object
* to the GUI thread.
*/
struct FuncCallMessage: public Message
{
	typedef std::function<void()> Func;
	Func func;
	FuncCallMessage(Func&& aFunc)
	:Message(Message::MMSG_FCALL), func(aFunc)
	{}
};

/** Must be provided by the SDK user.
* Wraps an opaque void*, poiting to a mega message, into a
* platform inter-thread message/signal and posts it to the GUI
* message loop. This function is called by various threads
*/
void postMessageToGui(void* msg);

/** Utility function that uses the messaging infrastructure to marshal a call
 * to the GUI thread */
void marshalCall(std::function<void()>&& call)
{
	FuncCallMessage* msg = new FuncCallMessage(std::forward<std::function<void()> >(call));
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
	Message::verifyMagic(msg);
	switch (msg->type)
	{
		case Message::MMSG_FCALL:
			static_cast<FuncCallMessage*>(msg)->func();
			return;
		case Message::MMSG_XMPPIO:
			//karere::handleXmppIo(static_cast<FuncCallMessage*>(msg));
			return;
		case Message::MMSG_CURLIO:
			//implement when CURL is used
			return;
		default:
			return;
	}
}

}

#endif // GUICALLMARSHALLER_H
