/*
 * imember.h
 *
 *  Created on: 9/02/2015
 *      Author: michael
 */

#ifndef SRC_IMEMBER_H_
#define SRC_IMEMBER_H_

#include <memory>
#include <string>
#include <shared_buffer.h>
#include <secure_key.h>

namespace karere {

class DummyMember {
public:
    DummyMember() {}

    DummyMember(const std::string &id) : id(id) { }

    DummyMember(const char *id) : id(id) { }

    void setStaticPublicKey(mpenc_cpp::SecureKey &staticPublicKey) {
        this->staticPublicKey = staticPublicKey;

    }

    mpenc_cpp::SecureKey getStaticPublicKey() {
        return staticPublicKey;
    }

    std::string &getId() { return id; }

    void setId(std::string &id) { this->id = id; }

private:

    std::string id;

    mpenc_cpp::SecureKey staticPublicKey;
};

class DummyGroupMember : public DummyMember {
public:
    DummyGroupMember() {}

    DummyGroupMember(const std::string &id) : DummyMember(id) {}

    DummyGroupMember(const char *id) : DummyMember(id) {}
};

/**
 * @brief Members of a chatroom have their information encapsulated by this
 * class. Currently does not provide any functional extension from the base class
 * M.
 */
template<class M>
class ChatRoomMember : public M {
public:

    /**
     * @brief No-arg constructor for ChatRoomMember.
     */
    ChatRoomMember() : M() {}
    /**
     * @brief Constructor with id just inherited from M.
     */
    ChatRoomMember(const std::string &id) : M(id) {}

    /**
     * @brief Constructor with id just inherited from M.
     */
    ChatRoomMember(const char *id) : M(id) {}

    /**
     * @brief Return if this is a chat room member.
     */
    bool isChatRoomMember() { return true; }

    /**
     * @brief Set the email address for this user.
     *
     * @param emailAddress The email address for the user.
     *
     */
    void setEmailAddress(std::string &emailAddress) {
        this->emailAddress = emailAddress;
    }

    /**
     * @brief Get the email address for this user.
     *
     * @return The email address for this user.
     */
    std::string getEmailAddress() {
        return emailAddress;
    }

private:

    /**
     * @brief The email address for this user.
     */
    std::string emailAddress;
};

/**
 * @brief This is the member who 'owns' the current client.
 */
template<class M, class GM>
class ChatRoomGroupMember : virtual public GM, virtual public ChatRoomMember<M> {
public:
    /**
     * @brief Constructor with id just inherited from M.
     */
    ChatRoomGroupMember(const std::string &id) : GM(id) {}

    /**
     * @brief Constructor with id just inherited from M.
     */
    ChatRoomGroupMember(const char *id) : GM(id) {}
};

/**
 * @brief This is a dummy shared ptr for DummyMember. Used to provide a default
 *  for testing.
 */
class SharedDummyMember : public std::shared_ptr<DummyMember> {
public:
    SharedDummyMember(DummyMember *member) :
        std::shared_ptr<DummyMember>(member) {}

    SharedDummyMember(const char *str) :
        std::shared_ptr<DummyMember>(
                new DummyMember(str)) {}

    SharedDummyMember(const std::string &id) :
        std::shared_ptr<DummyMember>(
                new DummyMember(id)) {}

    SharedDummyMember() :
        std::shared_ptr<DummyMember>() {}
};

/**
 * @brief This is a dummy shared ptr for DummyGroupMember. Used to provide a
 * default for testing.
 */
template<class M = DummyMember, class S = SharedDummyMember>
class SharedChatRoomMember : public S {
public:
    //static_assert(std::is_base_of<std::shared_ptr<M>, S>::value,
    //        "S must be a base of std::shared_ptr<M>");

    explicit SharedChatRoomMember(SharedChatRoomMember<M> *m) :
        S(m) {}

    SharedChatRoomMember(const char *str) :
        S(new ChatRoomMember<M>(str)) {}

    SharedChatRoomMember(const std::string &id) :
        S(new ChatRoomMember<M>(id)) {}

    SharedChatRoomMember() :
        S() {}

    ChatRoomMember<M> *get() {
        return static_cast<ChatRoomMember<M>*>(S::get());
    }

    ChatRoomMember<M> *operator->() {
        return static_cast<ChatRoomMember<M>*>(S::operator->());
    }

    ChatRoomMember<M> &operator*() {
        return static_cast<ChatRoomMember<M>*>(S::operator*());
    }

};

} /* namespace karere */

#endif /* SRC_IMEMBER_H_ */
