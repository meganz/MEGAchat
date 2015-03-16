/*
 * i_enc_handler.h
 *
 *  Created on: 13/02/2015
 *      Author: michael
 */

#ifndef SRC_IENCHANDLER_H_
#define SRC_IENCHANDLER_H_

#include <shared_buffer.h>
#include <common.h>
#include <member.h>
#include <group_member.h>
#include <upper_handler.h>

namespace karere {

class DummyEncProtocolHandler {
public:

    mpenc_cpp::ProtocolStatePtr protocolState;

    mpenc_cpp::MessageQueue outgoingBuffer;

    mpenc_cpp::MessageQueue dataBuffer;

    mpenc_cpp::ErrorMap errorMap;

    std::shared_ptr<std::map<std::string, mpenc_cpp::SecureKey>> keyMap;

    mpenc_cpp::SharedGroupMemberPtr member;

    DummyEncProtocolHandler() : member("bla") { }

    DummyEncProtocolHandler(mpenc_cpp::SharedGroupMemberPtr member) :
        protocolState(mpenc_cpp::UNINITIALISED),
        member(member) {

    }

    void setGroupMemberUh(mpenc_cpp::SharedMemberPtr member) {

    }

    inline mpenc_cpp::ProtocolState
    getProtocolStateUh() {
        return *protocolState;
    }

    inline int
    createDataMessageUh(mpenc_cpp::SharedBuffer data) {
        return mpenc_cpp::NO_ERROR;
    }

    inline void
    addStateObserver(mpenc_cpp::StateFunction stateFunction) {
        protocolState.addObserver(stateFunction);
    }

    inline void
    addOutgoingMessageObserverUh(mpenc_cpp::MessageAlertFunct alertFunct) {
        outgoingBuffer.addObserver(alertFunct);
    }

    inline void
    addErrorObserverUh(mpenc_cpp::MPENC_ERROR error,
            mpenc_cpp::ErrorFunct errorFunct) {
        errorMap.insert({error, errorFunct});
    }

    inline void
    addIncomingDataMessageObserverUh(mpenc_cpp::MessageAlertFunct alert) {
        dataBuffer.addObserver(alert);
    }

    inline int
    createErrorMessageUh(mpenc_cpp::ErrorMessageType type,
            const std::string &message) {
        return mpenc_cpp::NO_ERROR;
    }

    inline int
    sendDataMessageUh(mpenc_cpp::SharedBuffer data) {
        outgoingBuffer.push_back(data);
        return mpenc_cpp::NO_ERROR;
    }

    inline bool
    hasNextDataMessageUh() {
        return dataBuffer.hasNext();
    }

    inline mpenc_cpp::SharedBuffer
    getNextDataMessageUh() {
        return dataBuffer.popNext();
    }

    inline mpenc_cpp::ErrorMessageInfo
    processErrorMessageUh(mpenc_cpp::SharedBuffer wireMessage) {
        mpenc_cpp::ErrorMessageInfo info;
        return info;
    }

    inline int
    processMessageUh(mpenc_cpp::SharedBuffer wireMessage, const std::string &hint = "") {
        return mpenc_cpp::NO_ERROR;
    }

    inline bool
    hasNextMessageUh() {
        return outgoingBuffer.hasNext();
    }

    inline mpenc_cpp::SharedBuffer
    getNextMessageUh() {
        return outgoingBuffer.popNext();
    }

    inline mpenc_cpp::MessageQueue
    getMessageQueueUh() {
        return outgoingBuffer;
    }

    inline mpenc_cpp::MessageQueue
    getDataMessageQueueUh() {
        return dataBuffer;
    }

    inline void
    setMemberKeyUh(std::string id, mpenc_cpp::SecureKey key) {
        keyMap->insert({id, key});
    }

    inline int
    startUh(mpenc_cpp::MemberVector members, mpenc_cpp::WireMessageFormat
            format = mpenc_cpp::PLAIN_BYTES) {
        return mpenc_cpp::NO_ERROR;
    }

    inline int
    excludeUh(mpenc_cpp::MemberVector excMembers, mpenc_cpp::WireMessageFormat
           format = mpenc_cpp::PLAIN_BYTES ) {
        return mpenc_cpp::NO_ERROR;
    }

    inline int
    exitUh(mpenc_cpp::WireMessageFormat format = mpenc_cpp::PLAIN_BYTES) {
        return mpenc_cpp::NO_ERROR;
    }

    inline
    int joinUh(mpenc_cpp::MemberVector joinMembers, mpenc_cpp::WireMessageFormat
            format = mpenc_cpp::PLAIN_BYTES) {
        return mpenc_cpp::NO_ERROR;
    }

    inline int
    refreshUh(mpenc_cpp::WireMessageFormat format = mpenc_cpp::PLAIN_BYTES) {
        return mpenc_cpp::NO_ERROR;
    }

    inline int
    recoverUh(mpenc_cpp::MemberVector keepMembers, mpenc_cpp::WireMessageFormat
            format = mpenc_cpp::PLAIN_BYTES) {
        return mpenc_cpp::NO_ERROR;
    }
};

} /* namespace karere */
#endif /* SRC_IENCHANDLER_H_ */
