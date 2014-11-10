#ifndef GUICALLMARSHALLER_H
#define GUICALLMARSHALLER_H
#include <stdexcept>
#include <utility>
#include <functional>
#include <memory>

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
    enum _magic: unsigned long {kMegaMsgMagic = 0x3e9a3591};

/** Constants that denote the type of the message so that the processMessages()
 *  dispatcher can cast and handle them accordingly
 */
    enum {kMsgFuncCall = 1 << 0, kMsgWithHandler = 1 << 1};
/** The message type field */
    const unsigned char type;
    unsigned long magic;
    Message(unsigned char aType)
        :type(aType), magic(kMegaMsgMagic){}
	inline void verify()
	{
#ifndef NDEBUG
        if (magic != kMegaMsgMagic)
			throw std::runtime_error("Message does not have the correct magic value");
#endif
    }
    virtual ~Message()
    {
#ifndef NDEBUG
        magic = 0;
#endif
    }
};

/** This types of messages contain the message handler function pointer within themselves
 * Their type has bit 30 set, so that the handler can recognized and process them properly
 */
struct MessageWithHandler: public Message
{
    /** Handler C function that the message will be sent to, when received by the GUI thread
     * Dispatching messages by directly specifying the handler function allows
     * to dispatch the same kind of message to the correct module(shared object/main app)
     * so that access/memory management on the message is always performed by the module
     * that created the message.
     */
    typedef void(*HandlerFunc)(Message*);
    HandlerFunc handler;
    MessageWithHandler(HandlerFunc aHandler)
            :Message(Message::kMsgWithHandler), handler(aHandler){}
};

struct AbstractFuncCallMessage: public Message
{
    AbstractFuncCallMessage(): Message(Message::kMsgFuncCall){}
    virtual void doCall() = 0;
    virtual ~AbstractFuncCallMessage() {}
};

/**
* Message that posts a function to be called by the GUI thread.
* The function can be a c-style function, a lambda or a std::function object
*/
template <class F>
struct FuncCallMessage: public AbstractFuncCallMessage
{
    F mFunc;
    FuncCallMessage(F&& aFunc)
        :AbstractFuncCallMessage(), mFunc(std::forward<F>(aFunc))
	{}
    virtual void doCall()	{ mFunc(); }
};

/** Must be provided by the user.
* Wraps an opaque void*, poiting to a mega message, into a
* platform inter-thread message/signal and posts it to the GUI
* message loop. This function is called by various threads
*/
void postMessageToGui(void* msg);

/** Utility end-user function that uses the messaging infrastructure to marshal a function call
 * to the GUI thread. Can be used also to call a function asynchronously, similar to
 * setTimeout(..., 0) in javascript
*/
template <class F>
static inline void marshalCall(F&& func)
{
    FuncCallMessage<F>* msg = new FuncCallMessage<F>(std::forward<F>(func));
	postMessageToGui((void*)msg); //platform-specific, user-defined
}

/** This function must be called by the user when a Mega Message has been received
* by the GUI thread
* It passes a received by the GUI thread to the MegaSDK for processing.
* \warning Must be called only from the GUI thread
*/
static inline void processMessage(void* voidPtr)
{
    std::unique_ptr<Message> msg(static_cast<Message*>(voidPtr));
	msg->verify();
    const unsigned char type = msg->type;
    if (type == Message::kMsgFuncCall)
        (static_cast<AbstractFuncCallMessage*>(msg.get()))->doCall();
    else if (type == Message::kMsgWithHandler)
        (static_cast<MessageWithHandler*>(msg.get()))->handler(msg.get());
    else
        fprintf(stderr, "Unknown message type: %X", type);
}

}

#endif // GUICALLMARSHALLER_H
