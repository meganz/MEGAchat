/*
 * @file messageBus.h
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 *
 * This file is a stand-alone module, for use in MEGA libraries
 * and applications.
 *
 *
 * MessageBus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 *
 * @author Michael Holmwood mh@mega.co.nz
 */

#ifndef SRC_MESSAGEBUS_H_
#define SRC_MESSAGEBUS_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <list>
#include <iostream>
#include <stdexcept>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef __EASYLOGGER
#include <easylogging++.h>
#define LOG_INFO(i) LOG(INFO) << i
#define LOG_FUNCTION(i) LOG_INFO(__FUNCTION__ << i)
#define LOG_F LOG_INFO(__FUNCTION__)
#else
#define LOG_INFO(i)
#define LOG_FUNCTION(i)
#define LOG_F
#endif

#ifdef __TEST
#include <gtest/gtest.h>
#endif

namespace message_bus {

/**
 * @brief Events for messageBus.
 */
#define _MESSAGE_BUS_EVENT "message_bus_events"
#define _MESSAGE_BUS_ERROR "message_bus_errors"
#define _MESSAGE_BUS_ERROR_CONTENT "message_bus_error_content"

/**
 * @brief Enum of error codes for the message bus.
 */
enum BusError {
    BE_NO_ERROR = 0,
    BE_CAST_ERROR = -1,
    BE_NO_VALUE_OF_THAT_NAME = -2,
    BE_INCORRECT_ERROR_CODE = -666
};

/**
 * @brief Error codes for MessageBus.
 */
#define BE_NO_ERROR_STR "No error."
#define BE_CAST_ERROR_STR "Value cast error."
#define BE_NO_VALUE_OF_THAT_NAME_STR "No value of that name."
#define BE_INCORRECT_ERROR_CODE_STR "No error for that code"

/**
 * @brief Get the string for the supplied error.
 *
 * @param error The error to get the string for.
 * @reutrn The string for the supplied error code.
 */
static inline std::string stringForError(BusError error) {
    switch(error) {
    case BE_NO_ERROR : return std::string(BE_NO_ERROR_STR);
    case BE_CAST_ERROR : return std::string(BE_CAST_ERROR_STR);
    case BE_NO_VALUE_OF_THAT_NAME : return std::string(BE_NO_VALUE_OF_THAT_NAME_STR);
    default: return std::string(BE_INCORRECT_ERROR_CODE_STR);
    }
}

/**
 * @brief Class used when not requiring bus messages for errors.
 */
class NoReporter {
public:
    template<typename V>
    static inline void handleError(BusError error) {}
};

/**
 * @brief Base class for Value.
 *
 * This is just used as a type to cast all Values to. Avoids the
 * need for use of void pointers (which would make dynamic casting
 * impossible).
 */
class BaseValue {

    /**
     * @brief Destructor for BaseValue.
     */
    virtual ~BaseValue() {}

    /**
     * @brief Value requires access to the constructor for BaseValue.
     */
    template<typename V>
    friend class Value;

    /**
     * @brief BMessage requires access to the constructor for BaseValue.
     */
    template<typename Z>
    friend class BMessage;

    /**
     * @brief Constructor for BaseVaule.
     */
    inline BaseValue() {}

};

/**
 * @brief Value is used as a container for values stored in BMessage.
 *
 * Currently, the addition of values to messages this way takes control of the
 * given pointer.
 */
template<typename I>
class Value : public BaseValue {
public:

    /**
     * @brief The type of I.
     */
    typedef I valType;

    /**
     * @brief Constructor for Value, takes a pointer.
     */
    inline Value(I *value) : BaseValue(), value(value) {}

    /**
     * @brief Constructor for Value, takes a value.
     */
    inline Value(I value) : Value(new I(value)) {}

    virtual ~Value() {
        LOG_F;
        delete value;
    }

    I *value;
};

/**
 * @brief typedef for the map used in BMessage for value storage.
 */
typedef std::unordered_map<std::string, std::shared_ptr<BaseValue>> MessageMap;

/**
 * @brief typedef for the pair used to store values in BMessage.
 */
template<typename V>
using ValuePair = std::pair<std::string, V>;

/**
 * @brief Class for sending messages over the message bus.
 */
template<class Z>
class BMessage {

public:

    virtual ~BMessage() { LOG_F; }

    /**
     * @brief Constructor for BMessage.
     *
     * @param messageType The type of message.
     */
    inline BMessage(const std::string &messageType) : messageType(messageType){

    }

    /**
     * @brief Constructor for BMessage.
     *
     * @param messageType The type of message.
     */
    inline BMessage(const char *messageType) : messageType(messageType){

    }

    /**
     * @brief Get the type of message.
     *
     * @return The type of message.
     */
    inline std::string getMessageType() {
        return messageType;
    }

    /**
     * @brief Get the specified value as a shared_ptr<Value<V>.
     *
     * If the value does not exist, or the value cannot be cast to
     * the specified type, a runtime_error is thrown.
     *
     * @param valueName The value to get.
     * @return A shared_ptr<Value<V>> encapsulating the vaule.
     * @throws A runtime_error if the vaule does not exist or cannot
     *         be cast to the specified type.
     */
    template<typename V>
    inline std::shared_ptr<Value<V>> get(std::string &valueName) {

        auto i = messageMap.find(valueName);
        if(i == messageMap.end()) {
            std::string error("No Value of that name present:");
            error.append(valueName);
            Z::template handleError<V>(BE_NO_VALUE_OF_THAT_NAME);
            throw std::runtime_error(stringForError(BE_NO_VALUE_OF_THAT_NAME));
        }
        std::shared_ptr<BaseValue> value = i->second;
        std::shared_ptr<Value<V>> vCast =
                std::dynamic_pointer_cast<Value<V>>(value);

        if(!vCast) {
            std::string error("Incorrect type for value: ");
            error.append(valueName);
            Z::template handleError<V>(BE_CAST_ERROR);
            throw std::runtime_error(stringForError(BE_CAST_ERROR));
        }

        return vCast;
    }

    /**
     * @param Get the value for the given valueName.
     *
     * If the vaulue does not exist, or it cannot be cast into the
     * specified vaule, it throws a runtime_error.
     *
     * @param vauleName The name of the value to get.
     * @return The value requested.
     * @throws runtime_error if the value does not exist or cannot
     *         be cast to the specified type.
     */
    template<typename V>
    inline V getValue(const char *valueName) {
        std::string valName(valueName);
        return getValue<V>(valName);
    }

    /**
     * @brief Get the value for the given valueName.
     *
     * If the value does not exist, or it cannot be cast into the
     * specified value, it throws a runtime_error.
     *
     * @param vauleName The name of the value to get.
     * @return The value requested.
     * @throws runtime_error If the value does not exist or cannot be
     *         cast to the specified type.
     */
    template<typename V>
    inline V getValue(std::string &valueName) {
        return  *get<V>(valueName)->value;
    }

    /**
     * @brief Add a c style string to the message.
     *
     * This converts the given c style string to a c++ string
     * before adding it.
     *
     * @param valName The name of the string to add.
     * @param cStr The c style string to add.
     * @return A shared_ptr to the value.
     */
    inline std::shared_ptr<Value<std::string>> addValue(const char *valName,
            const char *cStr) {
        return addValue(valName, std::string(cStr));
    }

    /**
     * @brief Add a value, by vale, to the message.
     *
     * Be aware that this is gonig to copy value v.
     *
     * @param valName The name of the vaule to add.
     * @param v The value to add.
     */
    template<typename V>
    inline std::shared_ptr<Value<V>> addValue(const char *valName, V v) {
        std::string name(valName);
        return addValue(name, v);
    }

    /**
     * @brief Add a value, by value, to the message.
     *
     * Be aware that this is going to copy value v.
     *
     * @param valName The name of the value to add.
     * @param v The value to add.
     * @reutrn A shared_ptr to the value.
     */
    template<typename V>
    inline std::shared_ptr<Value<V>> addValue(std::string valName, V v) {
        std::shared_ptr<Value<V>> valPtr(new Value<V>(v));
        messageMap.insert({ valName,
            std::static_pointer_cast<BaseValue>(valPtr)
        });

        return valPtr;
    }

    /**
     * @brief Add a value by pointer to the message.
     *
     * Be aware that the message embeds the value into a shared_ptr,
     * a copy of which is returned to the user.
     *
     * @param valName The name of the value to add.
     * @param v The pointer to add to the message.
     * @return A shared_ptr containing the provided value.
     */
    template<typename V>
    inline std::shared_ptr<Value<V>> addValue(std::string valName, V *v) {
        std::shared_ptr<Value<V>> valPtr(new Value<V>(v));
        messageMap.insert({ valName,
            std::static_pointer_cast<BaseValue>(valPtr)});

        return valPtr;
    }

    /**
     * @brief Add the value as a shared_ptr<Value<V>.
     *
     * @param valName The name of the value to add.
     * @param s The value, encapsulated in a shared_ptr, to be added.
     */
    template<typename V>
    inline void addValue(std::string valName, std::shared_ptr<Value<V>> s) {
        messageMap.insert({ valName,
            std::static_pointer_cast<BaseValue>(s) });
    }

    /**
     * @brief Add the value as a BaseValue.
     *
     * This is currently only here for testing.
     *
     * @param valName The name of the value to add.
     * @param value The value to add.
     */
    inline BMessage &addValue(std::string valName, std::shared_ptr<BaseValue> value) {
        messageMap.insert({valName, value});
        return *this;
    }

    /**
     * @brief Overload of bitshilf left operator.
     *
     * Provides a mechanism to add values to the message easily.
     * This is only useful if you don't care about getting
     * the shared_ptr for any added values. Retrevial of these
     * will required susequent calls to get.
     *
     * @param value The key-value pair to add.
     * @return Reference to this BMessage.
     */
    template<typename V>
    inline BMessage &operator<<(ValuePair<V> value) {
        addValue(value.first, value.second);
        return *this;
    }

private:

    /**
     * @brief The map of name : values.
     */
    MessageMap messageMap;

    /**
     * @brief The type of this message.
     */
    std::string messageType;
};

/**
 * @brief shared_ptr that encapsulates messages.
 */
template<class Z = NoReporter>
class SharedMessage : public std::shared_ptr<BMessage<Z>> {

public:

    /**
     * @brief No-arg constructor for SharedMessage.
     *
     * Creates an empty shared_ptr.
     */
    inline SharedMessage() : std::shared_ptr<BMessage<Z>>() {}

    /**
     * @brief Creates a message with the specified type.
     */
    inline SharedMessage(const std::string &messageType) :
        std::shared_ptr<BMessage<Z>>(new BMessage<Z>(messageType)) {}

    /**
     * @brief Creates a message with the specified type.
     */
    inline SharedMessage(const char *messageType) :
        std::shared_ptr<BMessage<Z>>(new BMessage<Z>(messageType)) {}

    /**
     * @brief Overload of de-reference operator.
     *
     * @return Pointer to the encapsulated BMessage pointer.
     */
    inline BMessage<Z> *operator->() {
        return std::shared_ptr<BMessage<Z>>::operator->();
    }

    /**
     * @brief Overload of de-reference operator.
     *
     * @return reference to the encapsulated BMessage.
     */
    inline BMessage<Z> &operator*() {
        return std::shared_ptr<BMessage<Z>>::operator*();
    }

    /**
     * @brief Get the encapsulated pointer to BMessage.
     *
     * @brief Encapsulated pointer to BMessage.
     */
    inline BMessage<Z> *get() {
        return std::shared_ptr<BMessage<Z>>::get();
    }

    /**
     * @brief Overload of the bitshift left operator.
     *
     * @param The value to add to the BMessage.
     */
    template<typename V>
    inline BMessage<Z> &operator<<(ValuePair<V> value) {
        return get()->operator<<(value);
    }
};

/**
 * @brief Listener for accepting messages.
 *
 * Each listener for a given event needs to have an unique id.
 *
 */
template<class Z = NoReporter>
struct MessageListener {
    /**
     * @brief The id for this listener.
     */
    std::string id;

    /**
     * @brief The callback function for handling messages.
     */
    std::function<void(SharedMessage<Z>&, MessageListener<Z>&)> function;

    /**
     * @brief Test equality between listeners.
     */
    inline bool operator==(const MessageListener<Z> &other) {
        return this->id == other.id;
    }

    /**
    * @brief The queue of messages if working in async mode.
    */
    std::deque<SharedMessage<Z>> messageQueue;

    /**
     * @brief The default listener for MessageListener.
     */
    static std::function<void(SharedMessage<Z>&, MessageListener<Z>&)> ASYNC_FUNCT;
};


/**
 * @brief Function used for listeners.
 */
template<class Z>
using MessageFunction = std::function<void(SharedMessage<Z>&, MessageListener<Z>&)>;

/**
 * @brief Async function used for when message queueing is required.
 */
template<class Z>
MessageFunction<Z> MessageListener<Z>::ASYNC_FUNCT = [](const SharedMessage<Z>& m,
        const MessageListener<Z>& l){
    l.messageQueue.push_back(m);
};

/**
 * @brief using declaration for the lists of listeners.
 */
template<class Z = NoReporter>
using SharedListenerList = std::shared_ptr<std::list<MessageListener<Z>>>;

/**
 * @brief using declaration for the map used for events : listener lists.
 */
template<class Z>
using ListenerMap =  std::unordered_map<std::string, SharedListenerList<Z>>;

template<class Z, class D>
class MessageBus;

template<class Z>
struct NoStorage {
    NoStorage() {};
};
/**
 * @brief Default message handler for MessageBus.
 *
 * This class just distributes the messages amongst the listeners.
 */
class DefaultHandler {
public:
    template<class Z>
    using Storage = NoStorage<Z>;

    template<class Z>
    inline void init(NoStorage<Z> &storage) {
        std::cout << "init DefaultHandler" << std::endl;
    }

    template<class Z>
    inline void stop(NoStorage<Z> &storage) {

    }

    template<class Z>
    inline void alertListeners(std::string &event, SharedMessage<Z> &message,
            SharedListenerList<Z> &list, NoStorage<Z> &storage) {
        for(auto &l : *list) {
            l.function(message, l);
        }
    }
};

template<class Z, class D = DefaultHandler>
class MessageBus {
    template<class P, template<class, class> class G, class H>
    friend class SharedMessageBus;
#ifdef __TEST
    FRIEND_TEST(MessageBusTest, testMessageBus);
#endif

public:

    inline void shutDown() {
        alerter.stop(storage);
    }

    /**
     * @brief Register the given listener with the specified event.
     *
     * @param event The event to register for.
     * @param listener The listener to register.
     * @return true if the listener was registered.
     */
    inline bool addListener(const char *event, MessageListener<Z> listener) {
        std::string eventStr(event);
        return addListener(eventStr, listener);
    }

    /**
     * @brief Register the given listener with the specified event.
     *
     * We don't want multiples of the same listener registered, so we check for
     * this.
     *
     * @param event The event to register for.
     * @param listener The listener to register.
     * @return true if the listener was successfully registered.
     */
    inline bool addListener(std::string &event, MessageListener<Z> listener) {
        SharedListenerList<Z> listenVector = getEventVector(event);
        if(!listenVector) {
            listenVector = SharedListenerList<Z>(new std::list<MessageListener<Z>>());
            listenerMap.insert({ event, listenVector });
            listenVector->push_back(listener);
        }
        else {
            for(auto &l : *listenVector) {
                if(l.id == listener.id) {
                    return false;
                }
            }

            listenVector->push_back(listener);
        }

        return true;
    }

    /**
     * @brief Remove the given listener from the list of the event specified.
     *
     * @param event The event type to remove the listener from.
     * @param listener The listener to remove.
     * @return true if the listener was removed.
     */
    inline bool removeListener(const char *event, MessageListener<Z> listener) {
        std::string evStr(event);
        return removeListener(evStr, listener);
    }

    /**
     * @brief Remove the given listener from the list of the event specified.
     *
     * @param event The event type to remove the listener from.
     * @param listener The listener to remove.
     * @return true if the listener was removed.
     */
    inline bool removeListener(std::string &event, MessageListener<Z> listener) {
        SharedListenerList<Z> listenVector = getEventVector(event);
        bool removed = false;
        if(listenVector) {
            listenVector->remove_if([listener, &removed](const MessageListener<Z> &otherListener){
                if(listener.id == otherListener.id) {
                    removed = true;
                    return true;
                }

                return false;
            });
        }

        return removed;
    }

    /**
     * @brief Alert the listeners registered with the given event.
     *
     * @param event The event to alert the listeners of.
     * @param message The message to forward.
     */
    inline void alertListeners(const char *event, SharedMessage<Z> &message) {
        std::string evStr(event);
        alertListeners(evStr, message);
    }

    /**
     * @brief Alert the listeners registered with the given event.
     *
     * @param event The event to alert the listeners of.
     * @param message The message to forward.
     */
    inline void alertListeners(std::string &event, SharedMessage<Z> &message) {
        SharedListenerList<Z> listenVector = getEventVector(event);
        if(listenVector) {
//            for(auto l : *listenVector) {
//                l.function(message, l);
//            }
            alerter.template alertListeners<Z>(event, message, listenVector, storage);
        }
    }

private:

    /**
    * @brief Get the list of listeners registered with the given event.
    *
    * @param event The event to get the listeners for.
    * @return A list of listeners for the event, or an empty list.
    */
    inline SharedListenerList<Z> getEventVector(const std::string &event) {
       SharedListenerList<Z> listenVector;

       auto v = listenerMap.find(event);
       if(v != listenerMap.end()) {
           listenVector = v->second;
       }

       return listenVector;
    }

    /**
     * @brief No-arg constructor for MessageBus.
     */
    inline MessageBus() {}

    /**
     * @brief The map of listener vectors in MessageBus.
     */
    ListenerMap<Z> listenerMap;

    D alerter;

    typename D::template Storage<Z> storage;

};

/**
 * @brief Singleton wrapper for MessageBus.
 */
template<class Z = NoReporter, template<class, class> class M = MessageBus,
        class D = DefaultHandler>
class SharedMessageBus : std::shared_ptr<M<Z, D>> {
    friend class ErrorReporter;
public:

    static inline SharedMessageBus<Z, M, D> getMessageBus() {
        if(!messageBus) {
            std::cout << "creating" << std::endl;
            messageBus = SharedMessageBus<Z, M, D>(new M<Z, D>());
            messageBus->alerter.init(messageBus->storage);
        }

        return messageBus;
    }

    /**
     * @brief De-reference override for SharedMessageBus.
     *
     * @return The current pointer in SharedmessageBus.
     */
    inline M<Z, D> *operator->() {
        return std::shared_ptr<M<Z, D>>::operator->();
    }

private:

    /**
     * @brief No-arg constructor for SharedMessageBus.
     */
    inline SharedMessageBus() : std::shared_ptr<M<Z, D>>() {}

    /**
     * @brief Constructor takes pointer to MessageBuffer.
     */
    inline SharedMessageBus(M<Z, D> *m) : std::shared_ptr<M<Z, D>>(m) {}

    /**
     * @brief The singleton messageBus.
     */
    static SharedMessageBus<Z, M, D> messageBus;
};

template<class Z, template<class, class> class M, class D>
SharedMessageBus<Z, M, D> SharedMessageBus<Z, M, D>::messageBus = SharedMessageBus<Z, M, D>();

////////////////// Error handling classes ////////////////////////////

/**
 * @brief If this is supplied for the first parameter Z, then any
 * errors that occur in the bus are reported to listeners.
 */
class ErrorReporter {
public:
    /**
     * @brief Report the given error to listeners on the bus.
     *
     * @param error The error to report.
     */
    template<typename V>
    static inline void handleError(BusError error) {
        SharedMessage<ErrorReporter> busMessage(_MESSAGE_BUS_ERROR);
        busMessage->addValue(_MESSAGE_BUS_ERROR_CONTENT, stringForError(error));
        SharedMessageBus<ErrorReporter>::messageBus->
                alertListeners(_MESSAGE_BUS_ERROR, busMessage);
    }
};

////////////////// Message Handling classes ///////////////////////////

template<typename Z>
class ThreadDeque : public std::deque<std::pair<SharedListenerList<Z>, SharedMessage<Z>>> {
public:
    ThreadDeque() : std::deque<std::pair<SharedListenerList<Z>, SharedMessage<Z>>>(),
        stopDeque(false){

    };

    inline void pushBack(std::pair<SharedListenerList<Z>, SharedMessage<Z>> value) {
        lock.lock();
        this->push_back(value);
        lock.unlock();
        condition.notify_all();
    }

    static void run(ThreadDeque<Z> *z) {
        z->lock.lock();
        while(!z->stopDeque) {
            while(!z->empty()) {
                std::pair<SharedListenerList<Z>, SharedMessage<Z>> p = z->getFront();
                z->lock.unlock();
                for(MessageListener<Z> &l : *p.first) {
                    l.function(p.second, l);
                }
                z->lock.lock();
            }
            z->condition.wait(z->lock);
        }
        z->lock.unlock();
    }

    inline void stop() {
        lock.lock();
        stopDeque = true;
        lock.unlock();
        condition.notify_all();
    }

private:
    inline std::pair<SharedListenerList<Z>, SharedMessage<Z>> getFront() {
       std::pair<SharedListenerList<Z>, SharedMessage<Z>> z = this->front();
       this->pop_front();

       return z;
    }

    std::mutex lock;

    std::condition_variable_any condition;

    bool stopDeque;
};

/**
 * @brief Threaded message handler for MessageBus.
 *
 * This uses a central dispatch thread to distribute messages amongst listeners.
 */
class ThreadedHandler {
public:

    template<class Z>
    using Storage = ThreadDeque<Z>;

    std::shared_ptr<std::thread> threadP;

    template<class Z>
    inline void init(ThreadDeque<Z> &storage) {
        LOG_INFO("init ThreadHandler" << std::endl);
        threadP =
                std::shared_ptr<std::thread>(new std::thread(ThreadDeque<Z>::run, &storage));
    }

    template<class Z>
    inline void stop(ThreadDeque<Z> &storage) {
        storage.stop();
        threadP->join();
    }

    template<class Z>
    inline void alertListeners(std::string &event, SharedMessage<Z> &message,
            SharedListenerList<Z> &list, ThreadDeque<Z> &storage) {
        std::cout << "alerting" << std::endl;
        storage.pushBack({ list, message });
    }

};

} /* namespace karere*/



#endif /* SRC_MESSAGEBUS_H_ */
